// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/Address.hpp"
#include "Net/Authority.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <future>
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

asio::ip::address Pothos::Net::resolveHost(const std::string &host, const std::chrono::microseconds timeout)
{
    asio::error_code ec;
    const auto literal = asio::ip::make_address(host, ec);
    if (not ec) return literal;

    //the resolver and the result outlive this call when it times out
    auto resolver = std::make_shared<asio::ip::tcp::resolver>(ioContext());
    auto promise = std::make_shared<std::promise<asio::ip::address>>();
    auto future = promise->get_future();
    resolver->async_resolve(host, "", [resolver, promise, host](const asio::error_code &error, const asio::ip::tcp::resolver::results_type &results)
    {
        if (error or results.empty())
        {
            const auto why = error? error.message() : std::string("no addresses");
            promise->set_exception(std::make_exception_ptr(Pothos::IOException("resolve '" + host + "'", why)));
        }
        else promise->set_value(results.begin()->endpoint().address());
    });
    if (future.wait_for(timeout) != std::future_status::ready)
    {
        asio::post(ioContext(), [resolver]{resolver->cancel();});
        throw Pothos::TimeoutException("resolve '" + host + "'", "timed out");
    }
    return future.get();
}
