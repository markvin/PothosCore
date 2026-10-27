// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Network URIs of the form scheme://host[:port] (internal, not installed)

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace Pothos {
namespace Net {

struct Uri
{
    std::string scheme;
    std::string host; //!< without the brackets of an IPv6 address
    std::uint16_t port = 0; //!< 0 when not specified
};

/*!
 * Parse scheme://host[:port][/], IPv6 hosts in brackets.
 * \throws Pothos::InvalidArgumentException naming the string
 */
Uri parseUri(std::string_view s);

//! Format a URI, brackets around IPv6 hosts, the port only when not 0
std::string toString(const Uri &uri);

} //namespace Net
} //namespace Pothos
