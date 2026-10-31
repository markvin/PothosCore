// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal file system helpers (not installed, not part of the public API)

#pragma once
#include "Platform/Environment.hpp" //getEnv: UTF-8 on all systems
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#ifndef _WIN32
#include <unistd.h> //access
#endif

namespace Pothos {
namespace Util {
namespace Detail {

/*!
 * A std::string path of Pothos is UTF-8. On Windows the std::string
 * constructor of std::filesystem::path and path::string() use the ANSI code
 * page: a path with other characters would change. Convert with these.
 */
inline std::filesystem::path utf8Path(const std::string &s)
{
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

//! The UTF-8 text of a path (refer to utf8Path)
inline std::string pathUtf8(const std::filesystem::path &p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

enum class Access {Read, Write, Execute};

//! Can the current process access the path in the given way?
inline bool canAccess(const std::filesystem::path &path, const Access access)
{
    #ifdef _WIN32
    //Windows: existence, and no read-only attribute for writes
    if (not std::filesystem::exists(path)) return false;
    if (access != Access::Write) return true;
    const auto perms = std::filesystem::status(path).permissions();
    return (perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
    #else
    const int mode = (access == Access::Read)? R_OK : (access == Access::Write)? W_OK : X_OK;
    return ::access(path.c_str(), mode) == 0;
    #endif
}

/*!
 * The helpers below take std::filesystem::path. A std::string path is UTF-8:
 * the deleted overloads stop an implicit conversion (on Windows, through the
 * ANSI code page). Use utf8Path() for the argument.
 */
bool canAccess(const std::string &path, Access access) = delete;
std::filesystem::path makeAbsolute(const std::string &path, const std::filesystem::path &base) = delete;
std::filesystem::path makeAbsolute(const std::filesystem::path &path, const std::string &base) = delete;
std::filesystem::path makeAbsolute(const std::string &path, const std::string &base) = delete;
void moveFile(const std::string &from, const std::filesystem::path &to) = delete;
void moveFile(const std::filesystem::path &from, const std::string &to) = delete;
void moveFile(const std::string &from, const std::string &to) = delete;

//! A relative path is taken relative to base; an absolute path is kept
inline std::filesystem::path makeAbsolute(const std::filesystem::path &path, const std::filesystem::path &base)
{
    return (path.is_absolute()? path : (base / path)).lexically_normal();
}

//! Move a file, falling back to copy and remove across file systems
inline void moveFile(const std::filesystem::path &from, const std::filesystem::path &to)
{
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (not ec) return;
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(from);
}

inline std::string envOrEmpty(const std::string &name)
{
    return Pothos::Platform::getEnv(name).value_or("");
}

/*!
 * Expand environment variables in a path string.
 * Unix: a leading ~ (alone or followed by /) is the home directory,
 * $VAR and ${VAR} are expanded. Windows: %VAR% is expanded.
 * Undefined variables expand to an empty string.
 */
inline std::string expandPath(const std::string &path)
{
    std::string out;
    size_t i = 0;
    #ifdef _WIN32
    while (i < path.size())
    {
        const auto end = (path[i] == '%') ? path.find('%', i+1) : std::string::npos;
        if (end == std::string::npos)
        {
            out.push_back(path[i++]);
            continue;
        }
        out += envOrEmpty(path.substr(i+1, end-i-1));
        i = end+1;
    }
    #else
    if (not path.empty() and path[0] == '~' and (path.size() == 1 or path[1] == '/'))
    {
        out += envOrEmpty("HOME");
        i = 1;
    }
    const auto isNameChar = [](const char ch){return std::isalnum(static_cast<unsigned char>(ch)) or ch == '_';};
    while (i < path.size())
    {
        if (path[i] != '$')
        {
            out.push_back(path[i++]);
            continue;
        }
        if (i+1 < path.size() and path[i+1] == '{')
        {
            const auto end = path.find('}', i+2);
            if (end == std::string::npos)
            {
                out.append(path, i, std::string::npos);
                break;
            }
            out += envOrEmpty(path.substr(i+2, end-i-2));
            i = end+1;
            continue;
        }
        size_t end = i+1;
        while (end < path.size() and isNameChar(path[end])) end++;
        out += envOrEmpty(path.substr(i+1, end-i-1));
        i = end;
    }
    #endif
    return out;
}

} //namespace Detail
} //namespace Util
} //namespace Pothos
