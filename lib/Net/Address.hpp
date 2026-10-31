// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Host and port helpers, name resolution (internal, not installed)

#pragma once
#include <asio/ip/address.hpp>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Pothos {
namespace Net {

struct HostPort
{
    std::string host; //!< without the brackets of an IPv6 address
    std::uint16_t port;
};

/*!
 * Parse host[:port], IPv6 hosts in brackets ([::1]:514).
 * \throws Pothos::InvalidArgumentException naming the string
 */
HostPort parseHostPort(std::string_view s, std::uint16_t defaultPort);

//! host:port with brackets around IPv6 hosts
std::string formatHostPort(std::string_view host, std::uint16_t port);

//! Is this an IPv4 or IPv6 address (not a name that needs a lookup)?
bool isAddressLiteral(std::string_view host);

//! Is this a bind-to-all address: empty, 0.0.0.0 or ::?
bool isWildcard(std::string_view host);

//! The loopback address of the same family as a wildcard host: ::1 or 127.0.0.1
std::string loopbackFor(std::string_view wildcardHost);

//! Can this host open and bind an IPv6 socket? (cached)
bool ipv6Supported(void);

/*!
 * Resolve a host name or address literal to its addresses, IPv4 first (in
 * resolver order otherwise), like Poco did. Every name is looked up on its
 * own thread and a lookup that outlives the timeout keeps running: the next
 * call for the same name waits for it instead of starting over.
 * \throws Pothos::TimeoutException when the lookup takes longer than timeout
 * \throws Pothos::IOException when the name cannot be resolved
 */
std::vector<asio::ip::address> resolveHostAll(const std::string &host, std::chrono::microseconds timeout);

//! The first address of resolveHostAll() (IPv4 when there is one)
asio::ip::address resolveHost(const std::string &host, std::chrono::microseconds timeout);

} //namespace Net
} //namespace Pothos
