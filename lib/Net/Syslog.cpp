// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/Syslog.hpp"
#include "Net/Address.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <asio/ip/host_name.hpp>
#include <asio/ip/udp.hpp>
#include <asio/ip/v6_only.hpp>
#include <asio/post.hpp>
#include <array>
#include <charconv>
#include <ctime>
#include <format>
#include <future>
#include <mutex>

using namespace Pothos::Net;
using Pothos::System::LogLevel;
using Pothos::System::Detail::LogRecord;

namespace {

constexpr int FacilityUser = 1;
constexpr std::string_view SdId = "pothos@32473";

constexpr std::array<std::string_view, 8> levelNames{
    "fatal", "critical", "error", "warning", "notice", "information", "debug", "trace"};

std::string_view levelName(const LogLevel level)
{
    const int i = int(level) - 1;
    return (i >= 0 and i < int(levelNames.size()))? levelNames[std::size_t(i)] : "information";
}

std::optional<LogLevel> levelFromName(const std::string_view name)
{
    for (std::size_t i = 0; i < levelNames.size(); i++)
    {
        if (levelNames[i] == name) return LogLevel(int(i) + 1);
    }
    return std::nullopt;
}

//! Syslog severity: fatal->alert ... information->info, debug and trace->debug
int severity(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::Fatal: return 1;
    case LogLevel::Critical: return 2;
    case LogLevel::Error: return 3;
    case LogLevel::Warning: return 4;
    case LogLevel::Notice: return 5;
    case LogLevel::Information: return 6;
    default: return 7;
    }
}

LogLevel levelFromSeverity(const int sev)
{
    switch (sev)
    {
    case 0:
    case 1: return LogLevel::Fatal;
    case 2: return LogLevel::Critical;
    case 3: return LogLevel::Error;
    case 4: return LogLevel::Warning;
    case 5: return LogLevel::Notice;
    case 6: return LogLevel::Information;
    default: return LogLevel::Debug;
    }
}

//! Header fields: printable ASCII without spaces, "-" when empty
std::string headerField(const std::string_view s, const std::size_t maxLen)
{
    std::string out;
    for (const char ch : s.substr(0, maxLen)) out += (ch > 32 and ch < 127)? ch : '_';
    return out.empty()? "-" : out;
}

//! PARAM-VALUE escaping of RFC 5424: '"', '\' and ']'
std::string escapeParam(const std::string_view s)
{
    std::string out;
    for (const char ch : s)
    {
        if (ch == '"' or ch == '\\' or ch == ']') out += '\\';
        out += ch;
    }
    return out;
}

//! Cut at most maxLen bytes without splitting a UTF-8 sequence
std::string_view truncateUtf8(std::string_view s, const std::size_t maxLen)
{
    if (s.size() <= maxLen) return s;
    std::size_t n = maxLen;
    while (n > 0 and (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) n--;
    return s.substr(0, n);
}

template <typename T>
bool parseNumber(const std::string_view s, T &out)
{
    const auto result = std::from_chars(s.data(), s.data()+s.size(), out);
    return not s.empty() and result.ec == std::errc() and result.ptr == s.data()+s.size();
}

//! Days since 1970-01-01 of a civil date (proleptic Gregorian)
std::chrono::sys_days civilDays(const int y, const unsigned m, const unsigned d)
{
    return std::chrono::sys_days(std::chrono::year(y)/std::chrono::month(m)/std::chrono::day(d));
}

//! RFC 3339 timestamp of RFC 5424: YYYY-MM-DDThh:mm:ss[.frac](Z|+hh:mm|-hh:mm)
std::optional<std::chrono::system_clock::time_point> parseTimestamp(std::string_view s)
{
    int y = 0;
    unsigned mo = 0, d = 0, h = 0, mi = 0, se = 0;
    if (s.size() < 20 or s[4] != '-' or s[7] != '-' or s[10] != 'T' or s[13] != ':' or s[16] != ':') return std::nullopt;
    if (not parseNumber(s.substr(0, 4), y) or not parseNumber(s.substr(5, 2), mo) or not parseNumber(s.substr(8, 2), d) or
        not parseNumber(s.substr(11, 2), h) or not parseNumber(s.substr(14, 2), mi) or not parseNumber(s.substr(17, 2), se)) return std::nullopt;
    if (mo < 1 or mo > 12 or d < 1 or d > 31 or h > 23 or mi > 59 or se > 60) return std::nullopt;
    s.remove_prefix(19);

    std::chrono::microseconds frac(0);
    if (s.starts_with('.'))
    {
        s.remove_prefix(1);
        std::size_t n = 0;
        long long value = 0, scale = 1000000;
        while (n < s.size() and s[n] >= '0' and s[n] <= '9')
        {
            if (scale > 1) {scale /= 10; value += (s[n] - '0') * scale;}
            n++;
        }
        if (n == 0) return std::nullopt;
        frac = std::chrono::microseconds(value);
        s.remove_prefix(n);
    }

    std::chrono::minutes offset(0);
    if (s == "Z") {}
    else if (s.size() == 6 and (s[0] == '+' or s[0] == '-') and s[3] == ':')
    {
        unsigned oh = 0, om = 0;
        if (not parseNumber(s.substr(1, 2), oh) or not parseNumber(s.substr(4, 2), om)) return std::nullopt;
        offset = std::chrono::minutes(oh*60 + om) * ((s[0] == '-')? -1 : 1);
    }
    else return std::nullopt;

    const auto days = civilDays(y, mo, d);
    return std::chrono::system_clock::time_point(days.time_since_epoch()) +
        std::chrono::hours(h) + std::chrono::minutes(mi) + std::chrono::seconds(se) + frac - offset;
}

//! Read one space terminated header field
bool nextField(std::string_view &s, std::string_view &field)
{
    const auto sp = s.find(' ');
    if (sp == std::string_view::npos or sp == 0) return false;
    field = s.substr(0, sp);
    s.remove_prefix(sp+1);
    return true;
}

//! Structured data: "-" or [id name="value"...]... ; collects the pothos element parameters
bool parseStructuredData(std::string_view &s, std::vector<std::pair<std::string, std::string>> &pothosParams)
{
    if (s.starts_with('-'))
    {
        s.remove_prefix(1);
        return true;
    }
    if (not s.starts_with('[')) return false;
    while (s.starts_with('['))
    {
        s.remove_prefix(1);
        const auto idEnd = s.find_first_of(" ]");
        if (idEnd == std::string_view::npos or idEnd == 0) return false;
        const bool ours = s.substr(0, idEnd) == SdId;
        s.remove_prefix(idEnd);
        while (s.starts_with(' '))
        {
            s.remove_prefix(1);
            const auto eq = s.find("=\"");
            if (eq == std::string_view::npos or eq == 0) return false;
            const std::string name(s.substr(0, eq));
            s.remove_prefix(eq+2);
            std::string value;
            bool closed = false;
            while (not s.empty())
            {
                const char ch = s.front();
                s.remove_prefix(1);
                if (ch == '\\' and not s.empty() and (s.front() == '"' or s.front() == '\\' or s.front() == ']'))
                {
                    value += s.front();
                    s.remove_prefix(1);
                }
                else if (ch == '"') {closed = true; break;}
                else value += ch;
            }
            if (not closed) return false;
            if (ours) pothosParams.emplace_back(name, std::move(value));
        }
        if (not s.starts_with(']')) return false;
        s.remove_prefix(1);
    }
    return true;
}

std::optional<LogRecord> parse5424(std::string_view s, const int sev)
{
    std::string_view timestamp, host, app, procId, msgId;
    if (not nextField(s, timestamp) or not nextField(s, host) or not nextField(s, app) or
        not nextField(s, procId) or not nextField(s, msgId)) return std::nullopt;

    std::vector<std::pair<std::string, std::string>> params;
    if (not parseStructuredData(s, params)) return std::nullopt;
    if (not s.empty())
    {
        if (s.front() != ' ') return std::nullopt;
        s.remove_prefix(1);
    }
    if (s.starts_with("\xef\xbb\xbf")) s.remove_prefix(3); //UTF-8 BOM

    LogRecord r;
    r.level = levelFromSeverity(sev);
    r.source = (app == "-")? "" : std::string(app);
    r.text = std::string(s);
    r.time = std::chrono::system_clock::now();
    if (timestamp != "-")
    {
        if (const auto t = parseTimestamp(timestamp)) r.time = *t;
    }
    parseNumber(procId, r.pid);
    for (const auto &[name, value] : params)
    {
        if (name == "source") r.source = value;
        else if (name == "level") {if (const auto l = levelFromName(value)) r.level = *l;}
        else if (name == "tid") parseNumber(std::string_view(value), r.threadId);
    }
    return r;
}

//! RFC 3164: Mmm dd hh:mm:ss HOST TAG[PID]: TEXT
std::optional<LogRecord> parse3164(std::string_view s, const int sev)
{
    static constexpr std::array<std::string_view, 12> months{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (s.size() < 16 or s[3] != ' ' or s[6] != ' ' or s[9] != ':' or s[12] != ':' or s[15] != ' ') return std::nullopt;
    int month = -1;
    for (std::size_t i = 0; i < months.size(); i++) if (s.substr(0, 3) == months[i]) month = int(i);
    unsigned day = 0, h = 0, mi = 0, se = 0;
    auto dayText = s.substr(4, 2);
    if (dayText.front() == ' ') dayText.remove_prefix(1);
    if (month < 0 or not parseNumber(dayText, day) or not parseNumber(s.substr(7, 2), h) or
        not parseNumber(s.substr(10, 2), mi) or not parseNumber(s.substr(13, 2), se)) return std::nullopt;
    s.remove_prefix(16);

    std::string_view host;
    if (not nextField(s, host)) return std::nullopt;
    const auto tagEnd = s.find_first_of("[:");
    if (tagEnd == std::string_view::npos or tagEnd == 0) return std::nullopt;
    LogRecord r;
    r.source = std::string(s.substr(0, tagEnd));
    s.remove_prefix(tagEnd);
    if (s.starts_with('['))
    {
        const auto close = s.find(']');
        if (close == std::string_view::npos or not parseNumber(s.substr(1, close-1), r.pid)) return std::nullopt;
        s.remove_prefix(close+1);
    }
    if (not s.starts_with(':')) return std::nullopt;
    s.remove_prefix(1);
    if (s.starts_with(' ')) s.remove_prefix(1);

    //no year or zone in the timestamp: this year, local time
    std::tm tm{};
    const auto now = std::time(nullptr);
    #ifdef _WIN32
    localtime_s(&tm, &now);
    #else
    localtime_r(&now, &tm);
    #endif
    tm.tm_mon = month;
    tm.tm_mday = int(day);
    tm.tm_hour = int(h);
    tm.tm_min = int(mi);
    tm.tm_sec = int(se);
    tm.tm_isdst = -1;
    const auto t = std::mktime(&tm);
    r.time = (t == std::time_t(-1))? std::chrono::system_clock::now() : std::chrono::system_clock::from_time_t(t);
    r.level = levelFromSeverity(sev);
    r.text = std::string(s);
    return r;
}

} //namespace

std::string Pothos::Net::formatSyslog(const LogRecord &r, const std::string_view hostname)
{
    const auto time = std::chrono::floor<std::chrono::microseconds>(r.time);
    std::string out = std::format("<{}>1 {:%FT%T}Z {} Pothos {} - [{} source=\"{}\" level=\"{}\" tid=\"{}\"] ",
        FacilityUser*8 + severity(r.level), time, headerField(hostname, 255),
        (r.pid == 0)? std::string("-") : std::to_string(r.pid), SdId,
        escapeParam(r.source), levelName(r.level), r.threadId);
    out += truncateUtf8(r.text, MaxSyslogMessage);
    return out;
}

std::optional<LogRecord> Pothos::Net::parseSyslog(std::string_view s)
{
    if (not s.starts_with('<')) return std::nullopt;
    const auto close = s.find('>');
    if (close == std::string_view::npos or close < 2 or close > 4) return std::nullopt;
    int pri = 0;
    if (not parseNumber(s.substr(1, close-1), pri) or pri > 191) return std::nullopt;
    s.remove_prefix(close+1);
    if (s.starts_with("1 ")) return parse5424(s.substr(2), pri % 8);
    return parse3164(s, pri % 8);
}

/***********************************************************************
 * UDP sender
 **********************************************************************/
struct SyslogSender::Impl
{
    Impl(void):
        socket(ioContext())
    {
        return;
    }
    std::mutex mutex;
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint target;
    std::string hostname;
};

SyslogSender::SyslogSender(const std::string &addr):
    _impl(std::make_unique<Impl>())
{
    const auto hp = parseHostPort(addr, 514);
    _impl->target = asio::ip::udp::endpoint(resolveHost(hp.host, std::chrono::seconds(5)), hp.port);
    asio::error_code ec;
    _impl->socket.open(_impl->target.protocol(), ec);
    if (ec) throw Pothos::IOException("syslog sender " + addr, ec.message());
    _impl->hostname = asio::ip::host_name(ec);
}

SyslogSender::~SyslogSender(void)
{
    asio::error_code ec;
    _impl->socket.close(ec);
}

void SyslogSender::send(const LogRecord &r)
{
    const auto line = formatSyslog(r, _impl->hostname);
    std::lock_guard<std::mutex> lock(_impl->mutex);
    asio::error_code ec; //like a syslog channel: losing a message is not an error
    _impl->socket.send_to(asio::buffer(line), _impl->target, 0, ec);
}

/***********************************************************************
 * UDP listener
 **********************************************************************/
struct SyslogListener::Impl : std::enable_shared_from_this<SyslogListener::Impl>
{
    explicit Impl(std::function<void(const LogRecord &)> onRecord):
        socket(ioContext()),
        onRecord(std::move(onRecord)),
        buffer(65536)
    {
        return;
    }

    void receive(void)
    {
        auto self = this->shared_from_this();
        socket.async_receive_from(asio::buffer(buffer), sender, [self](const asio::error_code &ec, const std::size_t n)
        {
            if (ec == asio::error::operation_aborted) return;
            if (not ec)
            {
                if (const auto r = parseSyslog(std::string_view(self->buffer.data(), n))) self->onRecord(*r);
            }
            self->receive();
        });
    }

    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint sender;
    const std::function<void(const LogRecord &)> onRecord;
    std::vector<char> buffer;
};

SyslogListener::SyslogListener(std::function<void(const LogRecord &)> onRecord):
    _impl(std::make_shared<Impl>(std::move(onRecord)))
{
    auto &socket = _impl->socket;
    asio::error_code ec;
    if (ipv6Supported())
    {
        socket.open(asio::ip::udp::v6(), ec);
        asio::error_code v6ec; //dual-stack where available
        if (not ec) socket.set_option(asio::ip::v6_only(false), v6ec);
        if (not ec) socket.bind(asio::ip::udp::endpoint(asio::ip::address_v6::any(), 0), ec);
    }
    else
    {
        socket.open(asio::ip::udp::v4(), ec);
        if (not ec) socket.bind(asio::ip::udp::endpoint(asio::ip::address_v4::any(), 0), ec);
    }
    if (ec) throw Pothos::IOException("syslog listener", ec.message());

    //start receiving on the io_context thread, which owns the socket from now on
    auto impl = _impl;
    asio::post(ioContext(), [impl]{impl->receive();});
}

SyslogListener::~SyslogListener(void)
{
    //close on the io_context thread: no callback runs after this returns
    auto impl = _impl;
    if (ioContext().stopped()) //process exit: no thread to hand the close to
    {
        asio::error_code ec;
        impl->socket.close(ec);
        return;
    }
    auto closed = std::make_shared<std::promise<void>>(); //outlives a timed out wait
    auto done = closed->get_future();
    asio::post(ioContext(), [impl, closed]
    {
        asio::error_code ec;
        impl->socket.close(ec);
        closed->set_value();
    });
    if (done.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        asio::error_code ec; //the io_context is not running (process exit)
        impl->socket.close(ec);
    }
}

std::uint16_t SyslogListener::port(void) const
{
    asio::error_code ec;
    return _impl->socket.local_endpoint(ec).port();
}
