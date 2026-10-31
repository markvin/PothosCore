// Copyright (c) 2013-2016 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Logger.hpp>
#include <Pothos/Plugin/Static.hpp> //static block
#include <Pothos/Exception.hpp>
#include "System/LoggerSinks.hpp"
#include "Platform/Environment.hpp"
#include "Util/StringUtils.hpp"
#include "Net/Address.hpp" //isAddressLiteral (Windows)
#include "Net/Syslog.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace Pothos::System;

/***********************************************************************
 * Logger state: never destroyed, so that logging from the destructors
 * of static objects (plugins, registries) stays safe during shutdown
 **********************************************************************/
namespace {
struct LoggerState
{
    //level thresholds
    std::mutex levelMutex;
    LogLevel rootLevel = LogLevel::Information;
    std::map<std::string, LogLevel, std::less<>> levels;

    //the active sink and the listeners: one writer at a time
    std::mutex sinkMutex;
    Detail::LogSink sink;
    std::map<std::size_t, LogListener> listeners;
    std::size_t nextListenerId = 1;

    //asynchronous writer
    std::mutex queueMutex;
    std::condition_variable queueCond, idleCond;
    std::deque<Detail::LogRecord> queue;
    unsigned long long enqueued = 0; //records ever queued
    unsigned long long written = 0; //records taken off the queue and written
    unsigned long long generation = 0; //a writer stops when shutdownLogging() increments it
    std::thread worker;
    std::atomic<bool> async{true};
    std::atomic<bool> shutdown{false};
    std::atomic<bool> disabled{false}; //POTHOS_LOG_LEVEL=none: no record at all
};
}

static LoggerState &state(void)
{
    static auto *s = new LoggerState();
    return *s;
}

//the work of one listener loop: records and listener changes from the
//listeners, applied after the loop (the thread has the sink lock)
struct ListenerContext
{
    std::vector<Detail::LogRecord> nestedRecords;
    std::vector<std::pair<std::size_t, LogListener>> pendingAdds;
    std::vector<std::size_t> pendingRemoves;
};

//not null while this thread calls the listeners; a plain pointer to a stack
//object, thus safe to use in static destructors after thread_local cleanup
static thread_local ListenerContext *listenerContext = nullptr;

static void callSink(LoggerState &s, const Detail::LogRecord &record)
{
    if (not s.sink) return;
    try {s.sink(record);}
    catch (...) {} //a failing sink must never break the caller
}

static void writeToSink(const Detail::LogRecord &record, const bool tryOnly = false)
{
    auto &s = state();
    std::unique_lock<std::mutex> lock(s.sinkMutex, std::defer_lock);
    if (tryOnly)
    {
        if (not lock.try_lock()) return; //give up rather than hang
    }
    else lock.lock();
    callSink(s, record);
    if (s.listeners.empty()) return;

    ListenerContext context;
    listenerContext = &context;
    for (const auto &pair : s.listeners)
    {
        try {pair.second(record);}
        catch (...) {} //a failing listener must never break the caller
    }
    listenerContext = nullptr;

    //the messages of the listeners go only to the sink: no recursion
    for (const auto &nested : context.nestedRecords) callSink(s, nested);

    //listener changes that the listeners made
    for (auto &pair : context.pendingAdds) s.listeners.emplace(pair.first, std::move(pair.second));
    for (const auto id : context.pendingRemoves) s.listeners.erase(id);
}

//the writer of one generation: a detached writer of an earlier generation
//(Windows, after shutdownLogging) never takes records of a later one
static void workerLoop(const unsigned long long generation)
{
    auto &s = state();
    std::unique_lock<std::mutex> lock(s.queueMutex);
    while (true)
    {
        s.queueCond.wait(lock, [&s, generation]{return s.generation != generation or not s.queue.empty();});
        if (s.generation != generation) break; //stop requested
        auto record = std::move(s.queue.front());
        s.queue.pop_front();
        lock.unlock();
        writeToSink(record);
        lock.lock();
        s.written++;
        s.idleCond.notify_all();
    }
}

//write a record: synchronously or through the asynchronous writer
static void dispatch(Detail::LogRecord &&record)
{
    if (state().disabled) return; //POTHOS_LOG_LEVEL=none
    //a listener logs: this thread has the sink lock, writeToSink() writes it later
    if (listenerContext != nullptr)
    {
        listenerContext->nestedRecords.push_back(std::move(record));
        return;
    }
    auto &s = state();
    const bool urgent = record.level <= LogLevel::Critical;
    if (urgent or not s.async or s.shutdown)
    {
        Logger::flush(); //keep the order of earlier messages
        writeToSink(record);
        return;
    }
    std::unique_lock<std::mutex> lock(s.queueMutex);
    //shutdownLogging() can come between the check above and this lock: no new writer then
    if (s.shutdown)
    {
        lock.unlock();
        writeToSink(record);
        return;
    }
    if (not s.worker.joinable()) s.worker = std::thread(&workerLoop, s.generation); //started on first use
    s.queue.push_back(std::move(record));
    s.enqueued++;
    s.queueCond.notify_one();
}

static Detail::LogRecord makeRecord(const LogLevel level, const std::string &source, const std::string &message)
{
    Detail::LogRecord r;
    r.level = level;
    r.source = source;
    r.text = message;
    r.time = std::chrono::system_clock::now();
    r.threadId = static_cast<unsigned long>(std::hash<std::thread::id>()(std::this_thread::get_id()));
    static const int pid = Pothos::Platform::currentProcessId();
    r.pid = pid;
    return r;
}

/***********************************************************************
 * Public logging API
 **********************************************************************/
void Logger::log(const LogLevel level, const std::string &source, const std::string &message)
{
    if (not isEnabled(level, source)) return;
    dispatch(makeRecord(level, source, message));
}

bool Logger::isEnabled(const LogLevel level, const std::string &source)
{
    if (state().disabled) return false;
    return level <= getLevel(source);
}

void Logger::setLevel(const LogLevel level, const std::string &source)
{
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.levelMutex);
    if (source.empty())
    {
        s.rootLevel = level;
        s.disabled = false; //a root level turns logging on again
    }
    else s.levels[source] = level;
}

LogLevel Logger::getLevel(const std::string &source)
{
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.levelMutex);
    if (not source.empty())
    {
        const auto it = s.levels.find(source);
        if (it != s.levels.end()) return it->second;
    }
    return s.rootLevel;
}

std::optional<LogLevel> Logger::parseLevel(std::string_view name)
{
    const auto lower = Pothos::Util::Detail::toLower(Pothos::Util::Detail::trim(name));
    for (int i = int(LogLevel::Fatal); i <= int(LogLevel::Trace); i++)
    {
        if (lower == Pothos::Util::Detail::toLower(Detail::levelName(LogLevel(i)))) return LogLevel(i);
    }
    int number = 0;
    const auto parsed = std::from_chars(lower.data(), lower.data()+lower.size(), number);
    if (not lower.empty() and parsed.ec == std::errc() and parsed.ptr == lower.data()+lower.size() and
        number >= int(LogLevel::Fatal) and number <= int(LogLevel::Trace)) return LogLevel(number);
    return std::nullopt;
}

void Logger::flush(void)
{
    //in a listener: the record in progress is not written yet, a wait never ends
    if (listenerContext != nullptr) return;
    auto &s = state();
    std::unique_lock<std::mutex> lock(s.queueMutex);
    if (not s.worker.joinable()) return;
    //only the records logged before this call: other threads may keep logging
    const auto target = s.enqueued;
    s.idleCond.wait(lock, [&s, target]{return s.written >= target;});
}

std::size_t Logger::addListener(LogListener listener)
{
    auto &s = state();
    if (listenerContext != nullptr) //this thread has the sink lock: apply after the current record
    {
        const auto id = s.nextListenerId++;
        listenerContext->pendingAdds.emplace_back(id, std::move(listener));
        return id;
    }
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    const auto id = s.nextListenerId++;
    s.listeners.emplace(id, std::move(listener));
    return id;
}

void Logger::removeListener(const std::size_t id)
{
    auto &s = state();
    if (listenerContext != nullptr) //this thread has the sink lock: apply after the current record
    {
        listenerContext->pendingRemoves.push_back(id);
        return;
    }
    //the sink lock is held for each listener call: this waits for a call in progress
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    s.listeners.erase(id);
}

/***********************************************************************
 * Internal API
 **********************************************************************/
void Detail::setLogSink(LogSink sink)
{
    //in a listener this thread has the sink lock: the change would deadlock
    if (listenerContext != nullptr) throw std::logic_error("Pothos::System::Logger: the sink cannot change in a log listener");
    Logger::flush();
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    s.sink = std::move(sink);
}

Detail::LogSink Detail::getLogSink(void)
{
    //in a listener this thread has the sink lock: the change would deadlock
    if (listenerContext != nullptr) throw std::logic_error("Pothos::System::Logger: the sink cannot change in a log listener");
    Logger::flush();
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    return s.sink;
}

void Detail::deliver(const LogRecord &record)
{
    auto copy = record;
    dispatch(std::move(copy));
}

void Detail::shutdownLogging(void)
{
    auto &s = state();
    std::deque<LogRecord> remaining;
    {
        std::lock_guard<std::mutex> lock(s.queueMutex);
        s.shutdown = true; //from now on every message is written synchronously
        remaining.swap(s.queue);
        s.generation++; //the writer stops
        s.queueCond.notify_all();
    }
    //write what the writer did not get to here: at process exit on Windows
    //the writer thread may already be gone (killed while holding the sink
    //lock), so it is never waited on and the lock is only tried
    #ifdef _WIN32
    constexpr bool tryOnly = true;
    #else
    constexpr bool tryOnly = false;
    #endif
    for (const auto &record : remaining) writeToSink(record, tryOnly);
    {
        std::lock_guard<std::mutex> lock(s.queueMutex);
        s.written += remaining.size();
    }
    if (s.worker.joinable())
    {
        #ifdef _WIN32
        s.worker.detach();
        #else
        s.worker.join();
        #endif
    }
    std::lock_guard<std::mutex> lock(s.queueMutex);
    s.idleCond.notify_all();
}

void Detail::restartLogging(void)
{
    state().shutdown = false;
}

//stop the writer when the library is unloaded or the process exits
static struct LoggingShutdownGuard
{
    ~LoggingShutdownGuard(void)
    {
        Detail::shutdownLogging();
    }
} loggingShutdownGuard;

/***********************************************************************
 * Syslog forwarding and listening (lib/Net)
 **********************************************************************/
static std::mutex &getSetupLoggerMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

static std::unique_ptr<Pothos::Net::SyslogListener> listener;

std::string Logger::startSyslogListener(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());

    //a listener on all interfaces that feeds the active sink
    if (not listener) listener = std::make_unique<Pothos::Net::SyslogListener>(&Detail::deliver);

    //return the port number of the log service
    return std::to_string(listener->port());
}

void Logger::stopSyslogListener(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    listener.reset();
}

static std::shared_ptr<Pothos::Net::SyslogSender> forwarder;

static void __startSyslogForwarding(const std::string &addr)
{
    forwarder = std::make_shared<Pothos::Net::SyslogSender>(addr);
    auto sender = forwarder;
    Detail::setLogSink([sender](const Detail::LogRecord &r)
    {
        sender->send(r);
    });

    //subprocesses will automatically forward to this address
    Pothos::Platform::setEnv("POTHOS_SYSLOG_ADDR", addr);
}

void Logger::startSyslogForwarding(const std::string &addr)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    __startSyslogForwarding(addr);
}

void Logger::stopSyslogForwarding(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    if (not forwarder) return;
    Detail::setLogSink(Detail::LogSink()); //messages are dropped, as before
    forwarder.reset();
}

/***********************************************************************
 * InterceptStream custom streambuf to redirect to logger
 **********************************************************************/
class InterceptStream : public std::streambuf
{
public:
    InterceptStream(std::ostream& stream, const std::string &source):
      _source(source),
      _orgstream(stream),
      _lastPut(std::chrono::high_resolution_clock::now()),
      _flush(false)
    {
        //Swap the the old buffer in ostream with this buffer.
        _orgbuf=_orgstream.rdbuf(this);
        //disable automatic flushing on this stream
        stream << std::nounitbuf;
    }

    ~InterceptStream(void)
    {
        _orgstream.rdbuf(_orgbuf);//Restore old buffer
    }
protected:
    std::streamsize xsputn(const char *msg, std::streamsize count) override
    {
        //has old data? force flush on sync regardless of newline
        if ((std::chrono::high_resolution_clock::now() - _lastPut) > std::chrono::nanoseconds(long(0.5*1e9))) _flush = true;
        _lastPut = std::chrono::high_resolution_clock::now();
        _outbuf.append(msg, count);
        return count;
    }

    int_type overflow(int_type c) override
    {
        if(not traits_type::eq_int_type(c, traits_type::eof()))
        {
            char_type const t = traits_type::to_char_type(c);
            this->xsputn(&t, 1);
        }
        return not traits_type::eof();
    }

    int sync(void) override
    {
        if (not _outbuf.empty() and (_outbuf.back() == '\n' or _flush))
        {
            if (_outbuf.back() == '\n') _outbuf = _outbuf.substr(0, _outbuf.size()-1);
            if (not _outbuf.empty()) Logger::log(Detail::guessLevel(_outbuf), _source, _outbuf);
            _outbuf.clear();
            _flush = false;
        }
        return 0;
    }

private:
    const std::string _source;
    std::string _outbuf;
    std::streambuf* _orgbuf;
    std::ostream& _orgstream;
    std::chrono::high_resolution_clock::time_point _lastPut;
    bool _flush;
};

void Logger::forwardStdIoToLogging(const std::string &source)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    static std::shared_ptr<InterceptStream> clogRedirected;
    clogRedirected.reset(new InterceptStream(std::clog, source));
    static std::shared_ptr<InterceptStream> coutRedirected;
    coutRedirected.reset(new InterceptStream(std::cout, source));
    static std::shared_ptr<InterceptStream> cerrRedirected;
    cerrRedirected.reset(new InterceptStream(std::cerr, source));
}

/***********************************************************************
 * Configuration from the environment
 **********************************************************************/
void Detail::configureFromEnvironment(LogSink sinkOverride, const bool atLibraryLoad)
{
    using Pothos::Platform::getEnv;
    const auto levelStr = getEnv("POTHOS_LOG_LEVEL").value_or("information");
    const auto channel = getEnv("POTHOS_LOG_CHANNEL").value_or("color");
    const auto file = getEnv("POTHOS_LOG_FILE").value_or("pothos.log");
    const auto pattern = getEnv("POTHOS_LOG_FORMAT").value_or("%Y-%m-%d %H:%M:%S %s: %t");
    const auto timeZone = getEnv("POTHOS_LOG_TIMEZONE").value_or("local"); //local or UTC
    const auto syslogAddr = getEnv("POTHOS_SYSLOG_ADDR").value_or("");
    state().async = (getEnv("POTHOS_LOG_ASYNC").value_or("1") != "0");

    //"none" (or "off") logs nothing, also not the records of other processes
    const bool none = Pothos::Util::Detail::iequals(levelStr, "none") or Pothos::Util::Detail::iequals(levelStr, "off");
    const auto level = none? std::optional<LogLevel>(LogLevel::Information) : Logger::parseLevel(levelStr);
    Logger::setLevel(level.value_or(LogLevel::Information));
    state().disabled = none;

    std::string fileError, syslogError;
    if (sinkOverride) setLogSink(std::move(sinkOverride));
    else
    {
        const bool utc = Pothos::Util::Detail::iequals(timeZone, "UTC");
        try
        {
            setLogSink(makeChannelSink(channel, file, pattern, utc));
        }
        catch (const Pothos::Exception &ex)
        {
            //a log file that cannot be opened: the console, and a warning there
            setLogSink(makeChannelSink("console", "", pattern, utc));
            fileError = ex.displayText();
        }
        //syslog forwarding when the address is specified; an address that
        //does not resolve is not fatal: the logs stay on the channel sink
        bool deferSyslog = false;
        #ifdef _WIN32
        //the lookup thread of a host name cannot start under the loader lock
        if (atLibraryLoad and not syslogAddr.empty()) try
        {
            deferSyslog = not Pothos::Net::isAddressLiteral(Pothos::Net::parseHostPort(syslogAddr, 514).host);
        }
        catch (const Pothos::Exception &) {} //the forwarding below reports a bad address
        #else
        (void)atLibraryLoad;
        #endif
        if (not syslogAddr.empty() and not deferSyslog) try
        {
            __startSyslogForwarding(syslogAddr);
        }
        catch (const Pothos::Exception &ex)
        {
            syslogError = ex.displayText();
        }
    }
    if (not fileError.empty()) POTHOS_LOG_WARNING("Pothos.Logger", "POTHOS_LOG_FILE: {}; the logs go to the console", fileError);

    //warn once per invalid value: configuration runs at load and again from setupDefaultLogging()
    static std::mutex warnMutex;
    static std::string warnedLevel;
    static std::string warnedSyslog;
    std::lock_guard<std::mutex> lock(warnMutex);
    if (syslogError.empty()) warnedSyslog.clear();
    else if (warnedSyslog != syslogAddr)
    {
        warnedSyslog = syslogAddr;
        //on stderr, not through the channel: with the null or file channel the user must see it too
        const auto text = std::format("Pothos.Logger: POTHOS_SYSLOG_ADDR \"{}\": {}; the logs are not forwarded\n", syslogAddr, syslogError);
        std::fputs(text.c_str(), stderr);
        std::fflush(stderr);
    }
    if (level) warnedLevel.clear();
    else if (warnedLevel != levelStr)
    {
        warnedLevel = levelStr;
        POTHOS_LOG_WARNING("Pothos.Logger", "invalid POTHOS_LOG_LEVEL \"{}\", using information", levelStr);
    }
}

void Logger::setupDefaultLogging(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    Detail::configureFromEnvironment();
}

pothos_static_block(pothosLoggingInit)
{
    Detail::configureFromEnvironment(nullptr, true);
}

#include <Pothos/Managed.hpp>

static auto managedSystemLogger = Pothos::ManagedClass()
    .registerClass<Pothos::System::Logger>()
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, startSyslogListener))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, stopSyslogListener))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, startSyslogForwarding))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, stopSyslogForwarding))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, forwardStdIoToLogging))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, setupDefaultLogging))
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::Logger, flush))
    .commit("Pothos/System/Logger");
