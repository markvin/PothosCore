// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Syslog over UDP: RFC 5424 sender, RFC 5424/3164 listener (internal, not installed)

#pragma once
#include "System/LoggerSinks.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Pothos {
namespace Net {

/*!
 * Largest datagram that formatSyslog() makes: the text is cut to fit.
 * macOS refuses UDP datagrams larger than 9216 bytes (net.inet.udp.maxdgram).
 */
constexpr std::size_t MaxSyslogDatagram = 8192;

//! Longest source (before the escape of the characters) in a datagram
constexpr std::size_t MaxSyslogSource = 255;

/*!
 * RFC 5424 line, facility user:
 * <PRI>1 TIMESTAMP HOST Pothos PID - [pothos@32473 source="..." level="..." tid="..."] TEXT
 * The source and the exact Pothos level travel as structured data
 * (32473 is the documentation enterprise number of RFC 5612).
 */
std::string formatSyslog(const System::Detail::LogRecord &r, std::string_view hostname);

/*!
 * Parse an RFC 5424 or RFC 3164 datagram. Records from Pothos keep their
 * source, level, pid and thread id; for others the source is the APP-NAME
 * (5424) or TAG (3164); a 3164 message without a TAG has an empty source.
 * Returns nullopt for anything malformed.
 */
std::optional<System::Detail::LogRecord> parseSyslog(std::string_view datagram);

//! parseSyslog() with the receive time: RFC 3164 has no year (a time more than a day after now is from the year before)
std::optional<System::Detail::LogRecord> parseSyslog(std::string_view datagram, std::chrono::system_clock::time_point now);

//! Sends records to host[:port] (default port 514), send errors are ignored
class SyslogSender
{
public:
    //! \throws Pothos::Exception when the address is invalid or cannot be resolved
    explicit SyslogSender(const std::string &addr);
    ~SyslogSender(void);
    void send(const System::Detail::LogRecord &r);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/*!
 * Receives syslog datagrams on a free UDP port of all interfaces
 * (IPv6 and IPv4 when available) and passes the parsed records on.
 * The callback runs on the shared io_context thread and must not block.
 */
class SyslogListener
{
public:
    explicit SyslogListener(std::function<void(const System::Detail::LogRecord &)> onRecord);
    ~SyslogListener(void);
    std::uint16_t port(void) const;

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};

} //namespace Net
} //namespace Pothos
