// Copyright (c) 2015-2015 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Util/Network.hpp>
#include "Net/Address.hpp"
#include "Net/Uri.hpp"

bool Pothos::Util::isIPv6Supported(void)
{
    return Pothos::Net::ipv6Supported();
}

std::string Pothos::Util::getWildcardAddr(const std::string &service)
{
    std::string addr = isIPv6Supported()?"[::]":"0.0.0.0";
    if (not service.empty()) addr += ":" + service;
    return addr;
}

std::string Pothos::Util::getLoopbackAddr(const std::string &service)
{
    std::string addr = isIPv6Supported()?"[::1]":"127.0.0.1";
    if (not service.empty()) addr += ":" + service;
    return addr;
}

Pothos::Util::Uri Pothos::Util::parseUri(const std::string_view uri)
{
    auto parsed = Pothos::Net::parseUri(uri);
    return Uri{std::move(parsed.scheme), std::move(parsed.host), parsed.port};
}

std::string Pothos::Util::toString(const Uri &uri)
{
    return Pothos::Net::toString(Pothos::Net::Uri{uri.scheme, uri.host, uri.port});
}

std::string Pothos::Util::resolveHost(const std::string &host, const std::chrono::microseconds timeout)
{
    return Pothos::Net::resolveHost(host, timeout).to_string();
}
