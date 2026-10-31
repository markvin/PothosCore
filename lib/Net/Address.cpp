// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/Address.hpp"
#include "Net/Authority.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <algorithm>
#include <future>
#include <map>
#include <mutex>
#include <thread>
#include <memory>

Pothos::Net::HostPort Pothos::Net::parseHostPort(const std::string_view s, const std::uint16_t defaultPort)
{
    HostPort hp{std::string(), defaultPort};
    std::string why;
    if (not Detail::parseAuthority(s, hp.host, hp.port, why))
    {
        throw Pothos::InvalidArgumentException("invalid address '" + std::string(s) + "'", why);
    }
    return hp;
}

std::string Pothos::Net::formatHostPort(const std::string_view host, const std::uint16_t port)
{
    const std::string h(host);
    return ((h.find(':') != std::string::npos)? "[" + h + "]" : h) + ":" + std::to_string(port);
}

bool Pothos::Net::isWildcard(const std::string_view host)
{
    return host.empty() or host == "0.0.0.0" or host == "::";
}

std::string Pothos::Net::loopbackFor(const std::string_view wildcardHost)
{
    return (wildcardHost == "::")? "::1" : "127.0.0.1";
}

bool Pothos::Net::ipv6Supported(void)
{
    static const bool supported = []
    {
        asio::io_context context;
        asio::ip::udp::socket sock(context);
        asio::error_code ec;
        sock.open(asio::ip::udp::v6(), ec);
        if (not ec) sock.bind(asio::ip::udp::endpoint(asio::ip::address_v6::any(), 0), ec);
        return not ec;
    }();
    return supported;
}

namespace {

using Addresses = std::vector<asio::ip::address>;

//! The blocking lookup, run on its own thread
Addresses lookup(const std::string &host)
{
    asio::io_context context;
    asio::ip::tcp::resolver resolver(context);
    asio::error_code ec;
    const auto results = resolver.resolve(host, "", ec);
    if (ec) throw Pothos::IOException("resolve '" + host + "'", ec.message());
    Addresses addrs;
    for (const auto &entry : results)
    {
        const auto addr = entry.endpoint().address();
        if (std::ranges::find(addrs, addr) == addrs.end()) addrs.push_back(addr);
    }
    if (addrs.empty()) throw Pothos::IOException("resolve '" + host + "'", "no addresses");
    std::ranges::stable_partition(addrs, [](const asio::ip::address &addr){return addr.is_v4();});
    return addrs;
}

//! Lookups in flight by host name: a slow name does not block the others
struct LookupCache
{
    std::mutex mutex;
    std::map<std::string, std::shared_future<Addresses>> lookups;
};

//! Made on the first use and never destroyed: the logger resolves the syslog
//! address in a static block, before the globals of this file can exist
LookupCache &lookupCache(void)
{
    static auto *cache = new LookupCache();
    return *cache;
}

} //namespace

std::vector<asio::ip::address> Pothos::Net::resolveHostAll(const std::string &host, const std::chrono::microseconds timeout)
{
    asio::error_code ec;
    const auto literal = asio::ip::make_address(host, ec);
    if (not ec) return {literal};

    auto &cache = lookupCache();
    std::shared_future<Addresses> future;
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        auto &entry = cache.lookups[host];
        if (not entry.valid())
        {
            auto promise = std::make_shared<std::promise<Addresses>>();
            entry = promise->get_future().share();
            std::thread([promise, host]
            {
                try {promise->set_value(lookup(host));}
                catch (...) {promise->set_exception(std::current_exception());}
            }).detach();
        }
        future = entry;
    }

    if (future.wait_for(timeout) != std::future_status::ready)
    {
        throw Pothos::TimeoutException("resolve '" + host + "'", "timed out");
    }
    {
        std::lock_guard<std::mutex> lock(cache.mutex);
        const auto it = cache.lookups.find(host);
        if (it != cache.lookups.end() and it->second.valid() and it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            cache.lookups.erase(it); //done: the next call looks the name up again
        }
    }
    return future.get();
}

asio::ip::address Pothos::Net::resolveHost(const std::string &host, const std::chrono::microseconds timeout)
{
    return resolveHostAll(host, timeout).front();
}
