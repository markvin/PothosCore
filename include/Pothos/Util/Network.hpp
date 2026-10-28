///
/// \file Util/Network.hpp
///
/// Utility functions to help with networking.
///
/// \copyright
/// Copyright (c) 2015-2015 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Config.hpp>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace Pothos {
namespace Util {

/*!
 * Does this system support IPv6?
 * \return true if supported
 */
POTHOS_API bool isIPv6Supported(void);

/*!
 * Get a functioning wildcard address "::" or "0.0.0.0".
 * \param service an optional service or port number
 * \return the wildcard address (IPv6 is available)
 */
POTHOS_API std::string getWildcardAddr(const std::string &service = "");

/*!
 * Get a functioning loopback address "::1" or "127.0.0.1".
 * \param service an optional service or port number
 * \return the loopback address (IPv6 is available)
 */
POTHOS_API std::string getLoopbackAddr(const std::string &service = "");

/*!
 * The parts of a URI in the format scheme://host[:port][/].
 */
struct Uri
{
    std::string scheme;
    std::string host; //!< an IPv6 host has no brackets
    std::uint16_t port = 0; //!< 0 when the URI has no port
};

/*!
 * Parse a URI in the format scheme://host[:port][/].
 * An IPv6 host must be in brackets: tcp://[::1]:1234.
 * Paths and queries are not supported.
 * \param uri the URI string
 * \return the parts of the URI
 * \throws Pothos::InvalidArgumentException when the string is not correct
 */
POTHOS_API Uri parseUri(std::string_view uri);

/*!
 * Make a URI string from its parts (an IPv6 host gets brackets).
 * \param uri the parts of the URI
 * \return the URI string, without the port when the port is 0
 */
POTHOS_API std::string toString(const Uri &uri);

/*!
 * Resolve a host name or an address literal to one address.
 * An IPv4 address comes first. The lookup of a name continues in the
 * background after a timeout: the next call for the same name waits for it.
 * \param host the host name or address literal
 * \param timeout the maximum time to wait for the lookup
 * \return the address as a string, for example "127.0.0.1" or "::1"
 * \throws Pothos::TimeoutException when the lookup takes longer than timeout
 * \throws Pothos::IOException when the name cannot be resolved
 */
POTHOS_API std::string resolveHost(const std::string &host, std::chrono::microseconds timeout);

} //namespace Util
} //namespace Pothos
