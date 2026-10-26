// Copyright (c) 2016-2016 Josh Blum
//               2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Config.hpp>
#include <filesystem>
#include <string>
#include <system_error>

/*!
 * Resolve symbolic links into an absolute path.
 * The path is returned unchanged when it cannot be resolved.
 */
std::string Pothos_FileRealPath(const std::string &path)
{
    std::error_code ec;
    const auto real = std::filesystem::canonical(path, ec);
    return ec? path : real.string();
}
