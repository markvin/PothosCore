// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal string helpers (not installed, not part of the public API)

#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace Pothos {
namespace Util {
namespace Detail {

inline bool isSpace(const char ch)
{
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

//! Strip whitespace from both ends
inline std::string_view trim(std::string_view s)
{
    while (not s.empty() and isSpace(s.front())) s.remove_prefix(1);
    while (not s.empty() and isSpace(s.back())) s.remove_suffix(1);
    return s;
}

//! ASCII lower case
inline std::string toLower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
        [](const unsigned char ch){return static_cast<char>(std::tolower(ch));});
    return out;
}

//! ASCII upper case
inline std::string toUpper(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
        [](const unsigned char ch){return static_cast<char>(std::toupper(ch));});
    return out;
}

//! ASCII case insensitive equality
inline bool iequals(std::string_view a, std::string_view b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](const unsigned char x, const unsigned char y){return std::tolower(x) == std::tolower(y);});
}

/*!
 * Split a string on any of the separator characters.
 * \param trimTokens strip whitespace from each token
 * \param skipEmpty drop tokens that are empty (after trimming)
 */
inline std::vector<std::string> split(std::string_view s, std::string_view separators, const bool trimTokens = true, const bool skipEmpty = true)
{
    std::vector<std::string> tokens;
    if (s.empty()) return tokens;
    size_t start = 0;
    while (true)
    {
        const auto end = s.find_first_of(separators, start);
        auto token = s.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (trimTokens) token = trim(token);
        if (not (skipEmpty and token.empty())) tokens.emplace_back(token);
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return tokens;
}

/*!
 * Match a whole string against a glob pattern:
 * * any sequence (including /), ? any one character,
 * [abc] [a-z] a character set, [!abc] a negated set.
 */
inline bool globMatch(std::string_view pattern, std::string_view text)
{
    if (pattern.empty()) return text.empty();
    const char p = pattern.front();
    if (p == '*')
    {
        for (size_t i = 0; i <= text.size(); i++)
        {
            if (globMatch(pattern.substr(1), text.substr(i))) return true;
        }
        return false;
    }
    if (text.empty()) return false;
    if (p == '[')
    {
        const auto close = pattern.find(']', 2);
        if (close == std::string_view::npos) return p == text.front() and globMatch(pattern.substr(1), text.substr(1));
        auto set = pattern.substr(1, close-1);
        const bool negate = not set.empty() and set.front() == '!';
        if (negate) set.remove_prefix(1);
        bool found = false;
        for (size_t i = 0; i < set.size(); i++)
        {
            if (i+2 < set.size() and set[i+1] == '-')
            {
                if (set[i] <= text.front() and text.front() <= set[i+2]) found = true;
                i += 2;
            }
            else if (set[i] == text.front()) found = true;
        }
        if (found == negate) return false;
        return globMatch(pattern.substr(close+1), text.substr(1));
    }
    if (p != '?' and p != text.front()) return false;
    return globMatch(pattern.substr(1), text.substr(1));
}

//! Replace every occurrence of from with to (non-recursive)
inline std::string replaceAll(std::string s, std::string_view from, std::string_view to)
{
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos)
    {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

} //namespace Detail
} //namespace Util
} //namespace Pothos
