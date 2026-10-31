// Copyright (c) 2016-2016 Josh Blum
//               2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Config.hpp>
#include <filesystem>
#include <string>
#include <system_error>
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8

/*!
 * Resolve symbolic links into an absolute path.
 * The path is returned unchanged when it cannot be resolved.
 */
std::string Pothos_FileRealPath(const std::string &path)
{
    std::error_code ec;
    const auto real = std::filesystem::canonical(Pothos::Util::Detail::utf8Path(path), ec);
    return ec? path : Pothos::Util::Detail::pathUtf8(real);
}
