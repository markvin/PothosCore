// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/Uri.hpp"
#include "Net/Address.hpp"
#include "Net/Authority.hpp"
#include <Pothos/Exception.hpp>
#include <algorithm>

Pothos::Net::Uri Pothos::Net::parseUri(const std::string_view s)
{
    const auto bad = [s](const std::string &why)
    {
        return Pothos::InvalidArgumentException("invalid URI '" + std::string(s) + "'", why);
    };
    const auto sep = s.find("://");
    if (sep == std::string_view::npos) throw bad("expected scheme://host[:port]");
    const auto scheme = s.substr(0, sep);
    const auto isSchemeChar = [](const char ch)
    {
        return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or (ch >= '0' and ch <= '9') or ch == '+' or ch == '-' or ch == '.';
    };
    if (scheme.empty() or not std::ranges::all_of(scheme, isSchemeChar)) throw bad("invalid scheme");

    auto authority = s.substr(sep+3);
    if (authority.ends_with('/')) authority.remove_suffix(1);
    if (authority.find_first_of("/?#") != std::string_view::npos) throw bad("paths and queries are not supported");

    Uri uri;
    uri.scheme = std::string(scheme);
    std::string why;
    if (not Detail::parseAuthority(authority, uri.host, uri.port, why)) throw bad(why);
    return uri;
}

std::string Pothos::Net::toString(const Uri &uri)
{
    std::string out = uri.scheme + "://";
    if (uri.host.find(':') != std::string::npos) out += "[" + uri.host + "]";
    else out += uri.host;
    if (uri.port != 0) out += ":" + std::to_string(uri.port);
    return out;
}
