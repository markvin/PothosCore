// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/Logger.hpp>
#include "System/LoggerSinks.hpp"
#include "Platform/Environment.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <set>
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
