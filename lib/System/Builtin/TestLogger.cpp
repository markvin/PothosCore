// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/Logger.hpp>
#include "System/LoggerSinks.hpp"
#include "Platform/Environment.hpp"
#include "Net/Syslog.hpp"
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Pothos::System::Logger;
using Pothos::System::LogLevel;
namespace Detail = Pothos::System::Detail;

//! Capture records into a vector while in scope, then restore the previous sink
struct CaptureSink
{
    CaptureSink(const std::chrono::milliseconds delay = std::chrono::milliseconds(0)):
        previous(Detail::getLogSink())
    {
        Detail::setLogSink([this, delay](const Detail::LogRecord &r)
        {
            if (delay.count() != 0) std::this_thread::sleep_for(delay);
            std::lock_guard<std::mutex> lock(mutex);
            records.push_back(r);
        });
    }
    ~CaptureSink(void)
    {
        Detail::setLogSink(previous);
    }
    std::vector<Detail::LogRecord> get(void)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return records;
    }
    Detail::LogSink previous;
    std::mutex mutex;
    std::vector<Detail::LogRecord> records;
};

POTHOS_TEST_BLOCK("/system/tests", test_logger_levels)
{
    CaptureSink capture;
    Logger::setLevel(LogLevel::Warning, "t.lvl");
    POTHOS_LOG_INFORMATION("t.lvl", "x");
    POTHOS_LOG_ERROR("t.lvl", "v={}", 42);
    Logger::flush();
    const auto records = capture.get();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_EQUAL(records.at(0).text, "v=42");
    POTHOS_TEST_TRUE(records.at(0).level == LogLevel::Error);
    POTHOS_TEST_EQUAL(records.at(0).source, "t.lvl");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_format_pattern)
{
    using namespace std::chrono;
    Detail::LogRecord r;
    r.level = LogLevel::Error;
    r.source = "src";
    r.text = "msg";
    r.time = sys_days(year(2026)/1/2) + hours(3) + minutes(4) + seconds(5) + milliseconds(6);
    r.threadId = 77;
    r.pid = 1234;
    POTHOS_TEST_EQUAL(Detail::formatLogRecord(r, "%Y-%m-%d %H:%M:%S.%i %p %s: %t", true),
        "2026-01-02 03:04:05.006 Error src: msg");
    POTHOS_TEST_EQUAL(Detail::formatLogRecord(r, "[%q] %P/%T %% %z", true), "[E] 1234/77 % %z");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_braces_in_message)
{
    CaptureSink capture;
    Logger::log(LogLevel::Error, "t.braces", "{raw}");
    POTHOS_LOG_ERROR("t.braces", "{}", "{\"a\":1}");
    Logger::flush();
    const auto records = capture.get();
    POTHOS_TEST_EQUAL(records.size(), 2u);
    POTHOS_TEST_EQUAL(records.at(0).text, "{raw}");
    POTHOS_TEST_EQUAL(records.at(1).text, "{\"a\":1}");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_concurrent)
{
    CaptureSink capture;
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) threads.emplace_back([t]
    {
        for (int i = 0; i < 1000; i++) POTHOS_LOG_ERROR("t.mt", "{} {}", t, i);
    });
    for (auto &thread : threads) thread.join();
    Logger::flush();
    const auto records = capture.get();
    POTHOS_TEST_EQUAL(records.size(), 8000u);
    std::set<std::string> texts;
    for (const auto &r : records) texts.insert(r.text);
    POTHOS_TEST_EQUAL(texts.size(), 8000u);
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_async_order)
{
    CaptureSink capture;
    for (int i = 0; i < 1000; i++) POTHOS_LOG_ERROR("t.order", "{}", i);
    Logger::flush();
    const auto records = capture.get();
    POTHOS_TEST_EQUAL(records.size(), 1000u);
    for (int i = 0; i < 1000; i++) POTHOS_TEST_EQUAL(records.at(i).text, std::to_string(i));
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_fatal_sync)
{
    CaptureSink capture(std::chrono::milliseconds(1));
    for (int i = 0; i < 100; i++) POTHOS_LOG_ERROR("t.fatal", "{}", i);
    POTHOS_LOG_FATAL("t.fatal", "stop");
    //no flush: fatal messages are written before log() returns
    const auto records = capture.get();
    POTHOS_TEST_EQUAL(records.size(), 101u);
    POTHOS_TEST_EQUAL(records.back().text, "stop");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_flush_under_load)
{
    //a slow sink and a thread that never stops logging:
    //flush() must still return once the messages logged before it are written
    //the sink writes one record per 2ms while two arrive: the queue never empties
    CaptureSink capture(std::chrono::milliseconds(2));
    std::atomic<bool> stop(false);
    std::thread spammer([&stop]
    {
        while (not stop)
        {
            POTHOS_LOG_ERROR("t.load", "spam");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto flushed = std::async(std::launch::async, []{Logger::flush();});
    const bool returned = flushed.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    stop = true;
    spammer.join();
    flushed.wait();
    POTHOS_TEST_TRUE(returned);
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_after_shutdown)
{
    CaptureSink capture;
    Detail::shutdownLogging();
    POTHOS_LOG_ERROR("t.down", "late");
    const auto records = capture.get(); //synchronous after shutdown
    Detail::restartLogging();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_EQUAL(records.at(0).text, "late");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_bad_level_env)
{
    POTHOS_TEST_TRUE(Logger::parseLevel("bogus") == std::nullopt);
    POTHOS_TEST_TRUE(Logger::parseLevel("DEBUG") == LogLevel::Debug);
    POTHOS_TEST_TRUE(Logger::parseLevel("information") == LogLevel::Information);

    const auto oldLevel = Pothos::Platform::getEnv("POTHOS_LOG_LEVEL");
    const auto oldChannel = Pothos::Platform::getEnv("POTHOS_LOG_CHANNEL");
    const auto rootLevel = Logger::getLevel();
    Pothos::Platform::setEnv("POTHOS_LOG_LEVEL", "bogus");
    Pothos::Platform::setEnv("POTHOS_LOG_CHANNEL", "null");
    std::vector<Detail::LogRecord> records;
    {
        CaptureSink capture;
        Detail::configureFromEnvironment(); //replaces the sink with the null channel
        Logger::flush();
        POTHOS_TEST_TRUE(Logger::getLevel() == LogLevel::Information);
    }
    if (oldLevel) Pothos::Platform::setEnv("POTHOS_LOG_LEVEL", *oldLevel);
    else Pothos::Platform::unsetEnv("POTHOS_LOG_LEVEL");
    if (oldChannel) Pothos::Platform::setEnv("POTHOS_LOG_CHANNEL", *oldChannel);
    else Pothos::Platform::unsetEnv("POTHOS_LOG_CHANNEL");
    Detail::configureFromEnvironment();
    Logger::setLevel(rootLevel);

    //the warning about the bad level goes through the configured sink:
    //check it with a capture installed before reconfiguring
    Pothos::Platform::setEnv("POTHOS_LOG_LEVEL", "bogus");
    {
        CaptureSink capture;
        Detail::configureFromEnvironment(Detail::getLogSink());
        Logger::flush();
        records = capture.get();
    }
    if (oldLevel) Pothos::Platform::setEnv("POTHOS_LOG_LEVEL", *oldLevel);
    else Pothos::Platform::unsetEnv("POTHOS_LOG_LEVEL");
    Detail::configureFromEnvironment();
    Logger::setLevel(rootLevel);

    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_TRUE(records.at(0).level == LogLevel::Warning);
    POTHOS_TEST_TRUE(records.at(0).text.find("bogus") != std::string::npos);
}

namespace {

//! Wait until a predicate over the captured records holds
template <typename Pred>
bool waitForRecords(CaptureSink &capture, Pred pred)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (pred(capture.get())) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

Detail::LogRecord syslogTestRecord(const std::string &text)
{
    Detail::LogRecord r;
    r.level = LogLevel::Warning;
    r.source = "Test.Syslog";
    r.text = text;
    r.time = std::chrono::system_clock::now();
    return r;
}

} //namespace

POTHOS_TEST_BLOCK("/system/tests", test_logger_syslog_restart)
{
    CaptureSink capture;
    for (const std::string text : {"first", "second"})
    {
        const auto port = Logger::startSyslogListener();
        POTHOS_TEST_TRUE(not port.empty());
        Pothos::Net::SyslogSender("127.0.0.1:" + port).send(syslogTestRecord(text));
        POTHOS_TEST_TRUE(waitForRecords(capture, [&text](const std::vector<Detail::LogRecord> &rs)
        {
            for (const auto &r : rs) if (r.text == text and r.source == "Test.Syslog" and r.level == LogLevel::Warning) return true;
            return false;
        }));
        Logger::stopSyslogListener();
    }
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_syslog_forwarding)
{
    std::mutex mutex;
    std::vector<Detail::LogRecord> received;
    Pothos::Net::SyslogListener listener([&](const Detail::LogRecord &r)
    {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(r);
    });
    const auto previousAddr = Pothos::Platform::getEnv("POTHOS_SYSLOG_ADDR");
    {
        CaptureSink restore; //forwarding replaces the sink, this puts the previous one back
        Logger::startSyslogForwarding("127.0.0.1:" + std::to_string(listener.port()));
        POTHOS_LOG_WARNING("Test.Syslog", "forwarded {}", 42);
        Logger::flush();
        bool found = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (not found and std::chrono::steady_clock::now() < deadline)
        {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (const auto &r : received) found = found or (r.text == "forwarded 42" and r.source == "Test.Syslog" and r.level == LogLevel::Warning);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        POTHOS_TEST_TRUE(found);
        Logger::stopSyslogForwarding();
    }
    if (previousAddr) Pothos::Platform::setEnv("POTHOS_SYSLOG_ADDR", *previousAddr);
    else Pothos::Platform::unsetEnv("POTHOS_SYSLOG_ADDR");
}

/***********************************************************************
 * Log listeners
 **********************************************************************/
using Pothos::System::LogRecord;

//! Record what a listener gets while in scope, then remove the listener
struct CaptureListener
{
    CaptureListener(void):
        id(Logger::addListener([this](const LogRecord &r)
        {
            std::lock_guard<std::mutex> lock(mutex);
            records.push_back(r);
        }))
    {
        return;
    }
    ~CaptureListener(void)
    {
        Logger::removeListener(id);
    }
    std::vector<LogRecord> get(void)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return records;
    }
    std::mutex mutex;
    std::vector<LogRecord> records;
    const std::size_t id;
};

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_fields)
{
    CaptureSink capture;
    CaptureListener listener;
    POTHOS_TEST_TRUE(listener.id != 0);
    POTHOS_LOG_WARNING("Test.Listener", "hello {}", 42);
    Logger::flush();
    const auto records = listener.get();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_TRUE(records.at(0).level == LogLevel::Warning);
    POTHOS_TEST_EQUAL(records.at(0).source, "Test.Listener");
    POTHOS_TEST_EQUAL(records.at(0).text, "hello 42");
    const auto age = std::chrono::system_clock::now() - records.at(0).time;
    POTHOS_TEST_TRUE(age >= std::chrono::seconds(0) and age < std::chrono::seconds(10));
    POTHOS_TEST_TRUE(records.at(0).pid != 0);
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_level_filter)
{
    CaptureSink capture;
    const auto oldLevel = Logger::getLevel("Test.Filter");
    Logger::setLevel(LogLevel::Information, "Test.Filter");
    CaptureListener listener;
    POTHOS_LOG_DEBUG("Test.Filter", "hidden");
    POTHOS_LOG_INFORMATION("Test.Filter", "shown");
    Logger::flush();
    Logger::setLevel(oldLevel, "Test.Filter");
    const auto records = listener.get();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_EQUAL(records.at(0).text, "shown");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_deliver)
{
    //the syslog listener path: no level filter, as for the sink
    CaptureSink capture;
    CaptureListener listener;
    LogRecord r;
    r.level = LogLevel::Trace;
    r.source = "Test.Remote";
    r.text = "from far away";
    r.time = std::chrono::system_clock::now();
    Detail::deliver(r);
    Logger::flush();
    const auto records = listener.get();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_EQUAL(records.at(0).source, "Test.Remote");
    POTHOS_TEST_EQUAL(records.at(0).text, "from far away");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_null_sink)
{
    const auto previous = Detail::getLogSink();
    Detail::setLogSink(nullptr);
    CaptureListener listener;
    POTHOS_LOG_ERROR("Test.Null", "no sink");
    Logger::flush();
    Detail::setLogSink(previous);
    const auto records = listener.get();
    POTHOS_TEST_EQUAL(records.size(), 1u);
    POTHOS_TEST_EQUAL(records.at(0).text, "no sink");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_two_and_remove)
{
    CaptureSink capture;
    auto listener1 = std::make_unique<CaptureListener>();
    CaptureListener listener2;
    POTHOS_TEST_TRUE(listener1->id != listener2.id);
    for (int i = 0; i < 3; i++) POTHOS_LOG_ERROR("Test.Two", "{}", i);
    Logger::flush();
    Logger::removeListener(listener1->id);
    POTHOS_LOG_ERROR("Test.Two", "{}", 3);
    Logger::flush();
    Logger::removeListener(12345678); //unknown: does nothing
    const auto records1 = listener1->get();
    const auto records2 = listener2.get();
    POTHOS_TEST_EQUAL(records1.size(), 3u);
    POTHOS_TEST_EQUAL(records2.size(), 4u);
    for (int i = 0; i < 3; i++)
    {
        POTHOS_TEST_EQUAL(records1.at(i).text, std::to_string(i));
        POTHOS_TEST_EQUAL(records2.at(i).text, std::to_string(i));
    }
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_throws)
{
    CaptureSink capture;
    const auto thrower = Logger::addListener([](const LogRecord &)
    {
        throw std::runtime_error("listener failure");
    });
    CaptureListener listener;
    POTHOS_LOG_ERROR("Test.Throw", "a");
    POTHOS_LOG_ERROR("Test.Throw", "b");
    Logger::flush();
    Logger::removeListener(thrower);
    POTHOS_TEST_EQUAL(listener.get().size(), 2u);
    POTHOS_TEST_EQUAL(capture.get().size(), 2u);
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_nested_log)
{
    //a listener that logs: no deadlock (async and sync path), no recursion
    CaptureSink capture;
    std::atomic<bool> criticalDone(false);
    std::mutex mutex;
    std::vector<LogRecord> seen;
    const auto id = Logger::addListener([&](const LogRecord &r)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            seen.push_back(r);
        }
        if (r.source == "Test.Nested") return;
        POTHOS_LOG_ERROR("Test.Nested", "nested for {}", r.text);
        if (not criticalDone.exchange(true)) POTHOS_LOG_CRITICAL("Test.Nested", "nested critical");
    });
    for (int i = 0; i < 3; i++) POTHOS_LOG_ERROR("Test.Outer", "{}", i);
    POTHOS_LOG_CRITICAL("Test.Outer", "outer critical");
    Logger::flush();
    Logger::removeListener(id);
    Logger::flush();

    size_t nestedInSink = 0;
    for (const auto &r : capture.get()) if (r.source == "Test.Nested") nestedInSink++;
    POTHOS_TEST_EQUAL(nestedInSink, 5u); //4 nested errors and 1 nested critical
    std::lock_guard<std::mutex> lock(mutex);
    POTHOS_TEST_EQUAL(seen.size(), 4u);
    for (const auto &r : seen) POTHOS_TEST_EQUAL(r.source, "Test.Outer");
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_remove_waits)
{
    //after removeListener returns, no call of the listener is in progress
    CaptureSink capture;
    std::atomic<bool> entered(false), done(false);
    const auto id = Logger::addListener([&](const LogRecord &)
    {
        if (entered.exchange(true)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        done = true;
    });
    POTHOS_LOG_ERROR("Test.Remove", "slow");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (not entered and std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    POTHOS_TEST_TRUE(entered.load());
    Logger::removeListener(id);
    POTHOS_TEST_TRUE(done.load());
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_flush_inside)
{
    //flush() in a listener does not wait (the record in progress is not written yet)
    CaptureSink capture;
    std::atomic<int> calls(0);
    const auto id = Logger::addListener([&calls](const LogRecord &r)
    {
        if (r.source != "Test.FlushInside") return;
        Logger::flush();
        calls++;
    });
    POTHOS_LOG_ERROR("Test.FlushInside", "async");
    POTHOS_LOG_CRITICAL("Test.FlushInside", "sync");
    auto flushed = std::async(std::launch::async, []{Logger::flush();});
    const bool returned = flushed.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    POTHOS_TEST_TRUE(returned);
    Logger::removeListener(id);
    POTHOS_TEST_EQUAL(calls.load(), 2);
}

POTHOS_TEST_BLOCK("/system/tests", test_logger_listener_sink_change_inside)
{
    //a change of the sink in a listener is an error, not a deadlock
    CaptureSink capture;
    std::atomic<int> refused(0);
    const auto id = Logger::addListener([&refused](const LogRecord &r)
    {
        if (r.source != "Test.SinkInside") return;
        try {Detail::getLogSink();}
        catch (const std::logic_error &) {refused++;}
        try {Detail::setLogSink(nullptr);}
        catch (const std::logic_error &) {refused++;}
    });
    POTHOS_LOG_ERROR("Test.SinkInside", "x");
    auto flushed = std::async(std::launch::async, []{Logger::flush();});
    const bool returned = flushed.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    POTHOS_TEST_TRUE(returned);
    Logger::removeListener(id);
    POTHOS_TEST_EQUAL(refused.load(), 2);
    POTHOS_TEST_EQUAL(capture.get().size(), 1u);
}
