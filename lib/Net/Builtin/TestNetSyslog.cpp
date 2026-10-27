// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Net/Syslog.hpp"
#include "Net/Address.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

using namespace Pothos::Net;
using Pothos::System::LogLevel;
namespace Detail = Pothos::System::Detail;
using namespace std::chrono_literals;

namespace {

Detail::LogRecord makeRecord(const LogLevel level, const std::string &source, const std::string &text)
{
    Detail::LogRecord r;
    r.level = level;
    r.source = source;
    r.text = text;
    r.time = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::now());
    r.threadId = 1234567;
    r.pid = 4321;
    return r;
}

//! Records received by a listener
struct Received
{
    void push(const Detail::LogRecord &r)
    {
        std::lock_guard<std::mutex> lock(mutex);
        records.push_back(r);
        cond.notify_all();
    }
    bool waitFor(const std::size_t n)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return cond.wait_for(lock, 2s, [&]{return records.size() >= n;});
    }
    std::mutex mutex;
    std::condition_variable cond;
    std::vector<Detail::LogRecord> records;
};

} //namespace

POTHOS_TEST_BLOCK("/net/tests", test_net_syslog_roundtrip)
{
    const std::string text = "spaces and \"quotes\" ] \\ back\\slash \xc3\xa9 and\nnewline";
    for (const auto level : {LogLevel::Trace, LogLevel::Debug, LogLevel::Information, LogLevel::Notice,
        LogLevel::Warning, LogLevel::Error, LogLevel::Critical, LogLevel::Fatal})
    {
        const auto r = makeRecord(level, "My Source] \"x\"", text);
        const auto parsed = parseSyslog(formatSyslog(r, "host"));
        POTHOS_TEST_TRUE(bool(parsed));
        POTHOS_TEST_TRUE(parsed->level == level);
        POTHOS_TEST_EQUAL(parsed->source, r.source);
        POTHOS_TEST_EQUAL(parsed->text, text);
        POTHOS_TEST_EQUAL(parsed->pid, 4321);
        POTHOS_TEST_EQUAL(parsed->threadId, 1234567ul);
        POTHOS_TEST_TRUE(parsed->time == r.time);
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_syslog_format)
{
    const auto line = formatSyslog(makeRecord(LogLevel::Warning, "My Source", "hello"), "myhost");
    POTHOS_TEST_TRUE(line.starts_with("<12>1 "));
    POTHOS_TEST_TRUE(line.find(" myhost Pothos 4321 - [pothos@32473 source=\"My Source\" level=\"warning\" tid=\"1234567\"] hello") != std::string::npos);
    POTHOS_TEST_TRUE(formatSyslog(makeRecord(LogLevel::Fatal, "s", "x"), "h").starts_with("<9>1 "));
    POTHOS_TEST_TRUE(formatSyslog(makeRecord(LogLevel::Trace, "s", "x"), "h").starts_with("<15>1 "));

    const auto longLine = formatSyslog(makeRecord(LogLevel::Information, "s", std::string(100000, 'x')), "h");
    POTHOS_TEST_TRUE(longLine.size() <= MaxSyslogMessage + 200);
    POTHOS_TEST_TRUE(longLine.size() > MaxSyslogMessage);
}

POTHOS_TEST_BLOCK("/net/tests", test_net_syslog_parse_foreign)
{
    //RFC 3164 (rsyslog, BSD syslog)
    auto r = parseSyslog("<34>Oct 11 22:14:15 mymachine su[123]: 'su root' failed for lonvick");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_TRUE(r->level == LogLevel::Critical);
    POTHOS_TEST_EQUAL(r->source, "su");
    POTHOS_TEST_EQUAL(r->pid, 123);
    POTHOS_TEST_EQUAL(r->text, "'su root' failed for lonvick");

    r = parseSyslog("<13>Feb  5 17:32:18 10.0.0.99 myapp: no pid here");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_TRUE(r->level == LogLevel::Notice);
    POTHOS_TEST_EQUAL(r->source, "myapp");
    POTHOS_TEST_EQUAL(r->text, "no pid here");

    //RFC 5424 without the pothos structured data
    r = parseSyslog("<165>1 2003-10-11T22:14:15.003Z mymachine.example.com evntslog 77 ID47 [exampleSDID@32473 iut=\"3\" eventSource=\"App]lication\"] An application event");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_TRUE(r->level == LogLevel::Notice);
    POTHOS_TEST_EQUAL(r->source, "evntslog");
    POTHOS_TEST_EQUAL(r->pid, 77);
    POTHOS_TEST_EQUAL(r->text, "An application event");

    r = parseSyslog("<13>1 - - - - - -");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_EQUAL(r->text, "");

    r = parseSyslog("<14>1 2003-10-11T22:14:15Z h app - - - \xef\xbb\xbfwith BOM");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_EQUAL(r->text, "with BOM");

    //garbage is ignored without crashing
    for (const auto &bad : std::vector<std::string>{"", "<", "<>", "<999>x", "<13", "<13>1 2003", "<13>1 - - - - - [unterminated",
        "<13>1 - - - - - [a b=\"c]", "no priority", std::string("<13>\0\xff\xfe", 6)})
    {
        POTHOS_TEST_TRUE(not parseSyslog(bad));
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_syslog_udp)
{
    Received received;
    SyslogListener listener([&received](const Detail::LogRecord &r){received.push(r);});
    POTHOS_TEST_TRUE(listener.port() != 0);

    std::vector<std::string> targets{"127.0.0.1:" + std::to_string(listener.port())};
    if (ipv6Supported()) targets.push_back("[::1]:" + std::to_string(listener.port()));
    for (std::size_t i = 0; i < targets.size(); i++)
    {
        SyslogSender sender(targets[i]);
        sender.send(makeRecord(LogLevel::Error, "udp", targets[i]));
        POTHOS_TEST_TRUE(received.waitFor(i+1));
        std::lock_guard<std::mutex> lock(received.mutex);
        POTHOS_TEST_EQUAL(received.records.back().text, targets[i]);
        POTHOS_TEST_TRUE(received.records.back().level == LogLevel::Error);
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_syslog_timestamp_range)
{
    //timestamps outside the clock range fall back to the receive time (no overflow)
    for (const std::string ts : {"9999-01-01T00:00:00Z", "0001-01-01T00:00:00Z", "2262-04-12T00:00:00Z"})
    {
        const auto r = parseSyslog("<13>1 " + ts + " h app - - - x");
        POTHOS_TEST_TRUE(bool(r));
        const auto age = std::chrono::system_clock::now() - r->time;
        POTHOS_TEST_TRUE(age < std::chrono::minutes(1) and age > -std::chrono::minutes(1));
    }
    const auto r = parseSyslog("<13>1 2003-10-11T22:14:15.003Z h app - - - x");
    POTHOS_TEST_TRUE(bool(r));
    POTHOS_TEST_TRUE(r->time == std::chrono::system_clock::time_point(std::chrono::sys_days(std::chrono::year(2003)/10/11).time_since_epoch()) + 22h + 14min + 15s + 3ms);
}
