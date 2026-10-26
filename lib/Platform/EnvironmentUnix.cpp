// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/Environment.hpp"
#include <Pothos/Exception.hpp>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <format>
#include <map>
#include <thread>
#include <sys/utsname.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#ifdef __APPLE__
#include <net/if_dl.h>
#else
#include <netpacket/packet.h>
#endif

std::optional<std::string> Pothos::Platform::getEnv(const std::string &name)
{
    const char *value = std::getenv(name.c_str());
    if (value == nullptr) return std::nullopt;
    return std::string(value);
}

void Pothos::Platform::setEnv(const std::string &name, const std::string &value)
{
    if (::setenv(name.c_str(), value.c_str(), 1) != 0)
        throw Pothos::SystemException("Pothos::Platform::setEnv("+name+")", std::strerror(errno));
}

void Pothos::Platform::unsetEnv(const std::string &name)
{
    if (::unsetenv(name.c_str()) != 0)
        throw Pothos::SystemException("Pothos::Platform::unsetEnv("+name+")", std::strerror(errno));
}

static struct utsname getUname(void)
{
    struct utsname uts{};
    if (::uname(&uts) != 0)
        throw Pothos::SystemException("Pothos::Platform uname()", std::strerror(errno));
    return uts;
}

std::string Pothos::Platform::osName(void)
{
    return getUname().sysname;
}

std::string Pothos::Platform::osVersion(void)
{
    return getUname().release;
}

std::string Pothos::Platform::osArchitecture(void)
{
    return getUname().machine;
}

std::string Pothos::Platform::nodeName(void)
{
    return getUname().nodename;
}

std::string Pothos::Platform::nodeId(void)
{
    //hardware addresses of non-loopback adapters, ordered by adapter name
    std::map<std::string, std::string> macs;
    struct ifaddrs *list = nullptr;
    if (::getifaddrs(&list) == 0)
    {
        for (auto *ifa = list; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr or (ifa->ifa_flags & IFF_LOOPBACK) != 0) continue;
            const unsigned char *addr = nullptr;
            size_t len = 0;
            #ifdef __APPLE__
            if (ifa->ifa_addr->sa_family != AF_LINK) continue;
            const auto *sdl = reinterpret_cast<const struct sockaddr_dl *>(ifa->ifa_addr);
            addr = reinterpret_cast<const unsigned char *>(LLADDR(sdl));
            len = sdl->sdl_alen;
            #else
            if (ifa->ifa_addr->sa_family != AF_PACKET) continue;
            const auto *sll = reinterpret_cast<const struct sockaddr_ll *>(ifa->ifa_addr);
            addr = sll->sll_addr;
            len = sll->sll_halen;
            #endif
            if (len != 6) continue;
            bool allZero = true;
            for (size_t i = 0; i < len; i++) if (addr[i] != 0) allZero = false;
            if (allZero) continue;
            macs.emplace(ifa->ifa_name, std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
                addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));
        }
        ::freeifaddrs(list);
    }
    if (macs.empty()) return nodeName();
    return macs.begin()->second;
}

unsigned Pothos::Platform::processorCount(void)
{
    const auto n = std::thread::hardware_concurrency();
    return n == 0 ? 1 : n;
}

int Pothos::Platform::currentProcessId(void)
{
    return static_cast<int>(::getpid());
}
