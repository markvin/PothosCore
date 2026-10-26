// Copyright (c) 2013-2016 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Logger.hpp>
#include <Pothos/Util/Network.hpp>
#include <Pothos/Plugin/Static.hpp> //static block
#include "System/LoggerSinks.hpp"
#include "Platform/Environment.hpp"
#include "Util/StringUtils.hpp"
#include <Poco/Net/RemoteSyslogChannel.h>
#include <Poco/Net/RemoteSyslogListener.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/AutoPtr.h>
#include <Poco/Message.h>
#include <Poco/Channel.h>
#include <Poco/URI.h>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

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

    //the active sink: one writer at a time
    std::mutex sinkMutex;
    Detail::LogSink sink;

    //asynchronous writer
    std::mutex queueMutex;
    std::condition_variable queueCond, idleCond;
    std::deque<Detail::LogRecord> queue;
    bool busy = false;
    bool stopWorker = false;
    std::thread worker;
    std::atomic<bool> async{true};
    std::atomic<bool> shutdown{false};
};
}

static LoggerState &state(void)
{
    static auto *s = new LoggerState();
    return *s;
}

static void writeToSink(const Detail::LogRecord &record)
{
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    if (not s.sink) return;
    try {s.sink(record);}
    catch (...) {} //a failing sink must never break the caller
}

static void workerLoop(void)
{
    auto &s = state();
    std::unique_lock<std::mutex> lock(s.queueMutex);
    while (true)
    {
        s.queueCond.wait(lock, [&s]{return s.stopWorker or not s.queue.empty();});
        if (s.queue.empty()) break; //stop requested and nothing left
        auto record = std::move(s.queue.front());
        s.queue.pop_front();
        s.busy = true;
        lock.unlock();
        writeToSink(record);
        lock.lock();
        s.busy = false;
        if (s.queue.empty()) s.idleCond.notify_all();
    }
}

//write a record: synchronously or through the asynchronous writer
static void dispatch(Detail::LogRecord &&record)
{
    auto &s = state();
    const bool urgent = record.level <= LogLevel::Critical;
    if (urgent or not s.async or s.shutdown)
    {
        Logger::flush(); //keep the order of earlier messages
        writeToSink(record);
        return;
    }
    std::lock_guard<std::mutex> lock(s.queueMutex);
    if (not s.worker.joinable()) s.worker = std::thread(&workerLoop); //started on first use
    s.queue.push_back(std::move(record));
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
    return level <= getLevel(source);
}

void Logger::setLevel(const LogLevel level, const std::string &source)
{
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.levelMutex);
    if (source.empty()) s.rootLevel = level;
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
    auto &s = state();
    std::unique_lock<std::mutex> lock(s.queueMutex);
    if (not s.worker.joinable()) return;
    s.idleCond.wait(lock, [&s]{return s.queue.empty() and not s.busy;});
}

/***********************************************************************
 * Internal API
 **********************************************************************/
void Detail::setLogSink(LogSink sink)
{
    Logger::flush();
    auto &s = state();
    std::lock_guard<std::mutex> lock(s.sinkMutex);
    s.sink = std::move(sink);
}

Detail::LogSink Detail::getLogSink(void)
{
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
        s.stopWorker = true;
        s.queueCond.notify_all();
    }
    //write what the writer did not get to here: at process exit on Windows
    //the writer thread may already be gone, so it is never waited on for this
    for (const auto &record : remaining) writeToSink(record);
    if (s.worker.joinable())
    {
        #ifdef _WIN32
        s.worker.detach();
        #else
        s.worker.join();
        #endif
    }
    std::lock_guard<std::mutex> lock(s.queueMutex);
    s.stopWorker = false;
    s.busy = false;
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
 * Syslog forwarding and listening (Poco Net until it is replaced)
 **********************************************************************/
static std::mutex &getSetupLoggerMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

//! A channel for the syslog listener that writes received messages to the active sink
class LoggerChannel : public Poco::Channel
{
public:
    void log(const Poco::Message &msg) override
    {
        Detail::LogRecord r;
        const int prio = int(msg.getPriority());
        r.level = (prio >= int(LogLevel::Fatal) and prio <= int(LogLevel::Trace))? LogLevel(prio) : LogLevel::Information;
        r.source = msg.getSource();
        r.text = msg.getText();
        r.time = std::chrono::system_clock::time_point(std::chrono::microseconds(msg.getTime().epochMicroseconds()));
        r.threadId = static_cast<unsigned long>(msg.getTid());
        r.pid = static_cast<int>(msg.getPid());
        Detail::deliver(r);
    }
};

static Poco::AutoPtr<Poco::Net::RemoteSyslogListener> listener;

std::string Logger::startSyslogListener(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());

    if (not listener)
    {
        //find an available udp port
        const auto addr = Poco::URI("udp://"+Pothos::Util::getWildcardAddr()).getHost();
        Poco::Net::DatagramSocket sock(Poco::Net::SocketAddress(addr, 0), false);
        const auto port = sock.address().port();
        sock.close();

        //create a new listener that feeds the logger
        //FIXME syslog listener always binds to IPv4...
        listener = new Poco::Net::RemoteSyslogListener(port);
        listener->addChannel(Poco::AutoPtr<Poco::Channel>(new LoggerChannel()));
        listener->open();
    }

    //return the port number of the log service
    return listener->getProperty(Poco::Net::RemoteSyslogListener::PROP_PORT);
}

void Logger::stopSyslogListener(void)
{
    std::lock_guard<std::mutex> lock(getSetupLoggerMutex());
    if (not listener) return;
    listener->close();
    listener = nullptr;
}

static Poco::AutoPtr<Poco::Net::RemoteSyslogChannel> forwarder;

static void __startSyslogForwarding(const std::string &addr)
{
    forwarder = new Poco::Net::RemoteSyslogChannel(addr, ""/*empty name*/);
    auto channel = forwarder;
    Detail::setLogSink([channel](const Detail::LogRecord &r) mutable
    {
        Poco::Message msg(r.source, r.text, Poco::Message::Priority(int(r.level)));
        channel->log(msg);
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
    forwarder->close();
    forwarder = nullptr;
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
void Detail::configureFromEnvironment(LogSink sinkOverride)
{
    using Pothos::Platform::getEnv;
    const auto levelStr = getEnv("POTHOS_LOG_LEVEL").value_or("information");
    const auto channel = getEnv("POTHOS_LOG_CHANNEL").value_or("color");
    const auto file = getEnv("POTHOS_LOG_FILE").value_or("pothos.log");
    const auto pattern = getEnv("POTHOS_LOG_FORMAT").value_or("%Y-%m-%d %H:%M:%S %s: %t");
    const auto timeZone = getEnv("POTHOS_LOG_TIMEZONE").value_or("local"); //local or UTC
    const auto syslogAddr = getEnv("POTHOS_SYSLOG_ADDR").value_or("");
    state().async = (getEnv("POTHOS_LOG_ASYNC").value_or("1") != "0");

    const auto level = Logger::parseLevel(levelStr);
    Logger::setLevel(level.value_or(LogLevel::Information));

    if (sinkOverride) setLogSink(std::move(sinkOverride));
    else
    {
        setLogSink(makeChannelSink(channel, file, pattern, Pothos::Util::Detail::iequals(timeZone, "UTC")));
        //syslog forwarding when the address is specified
        if (not syslogAddr.empty()) __startSyslogForwarding(syslogAddr);
    }

    //warn once per invalid value: configuration runs at load and again from setupDefaultLogging()
    static std::mutex warnMutex;
    static std::string warnedLevel;
    std::lock_guard<std::mutex> lock(warnMutex);
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
    Detail::configureFromEnvironment();
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
