// Copyright (c) 2015-2015 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Util/Network.hpp>
#include "Net/Address.hpp"

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
