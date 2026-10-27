// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Shared parser of host[:port] (internal)

#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace Pothos {
namespace Net {
namespace Detail {

/*!
 * Parse host[:port] with IPv6 hosts in brackets.
 * The port is left unchanged when absent. Returns false with a reason on error.
 */
inline bool parseAuthority(std::string_view s, std::string &host, std::uint16_t &port, std::string &why)
{
    std::string_view portText;
    bool hasPort = false;
    if (s.starts_with('['))
    {
        const auto close = s.find(']');
        if (close == std::string_view::npos) {why = "missing ']'"; return false;}
        host = std::string(s.substr(1, close-1));
        const auto rest = s.substr(close+1);
        if (not rest.empty())
        {
            if (rest.front() != ':') {why = "unexpected text after ']'"; return false;}
            portText = rest.substr(1);
            hasPort = true;
        }
    }
    else
    {
        const auto colon = s.find(':');
        if (colon != std::string_view::npos and s.find(':', colon+1) != std::string_view::npos)
        {
            why = "IPv6 addresses must be in brackets";
            return false;
        }
        host = std::string(s.substr(0, colon));
        if (colon != std::string_view::npos)
        {
            portText = s.substr(colon+1);
            hasPort = true;
        }
    }
    if (host.empty()) {why = "missing host"; return false;}
    if (hasPort)
    {
        unsigned value = 0;
        const auto result = std::from_chars(portText.data(), portText.data()+portText.size(), value);
        if (portText.empty() or result.ec != std::errc() or result.ptr != portText.data()+portText.size() or value > 65535)
        {
            why = "invalid port";
            return false;
        }
        port = static_cast<std::uint16_t>(value);
    }
    return true;
}

} //namespace Detail
} //namespace Net
} //namespace Pothos
