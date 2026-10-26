// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal temporary file helpers (not installed, not part of the public API)

#pragma once
#include <Pothos/Exception.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace Pothos {
namespace Util {
namespace Detail {

/*!
 * A unique, currently unused path in the system temporary directory.
 * The file is not created.
 */
inline std::filesystem::path uniqueTempPath(const std::string &ext = ".tmp")
{
    static std::atomic<unsigned long long> counter(0);
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path();
    while (true)
    {
        const auto name = "pothos-" + std::to_string(rd()) + "-" + std::to_string(counter++) + ext;
        const auto candidate = dir / name;
        if (not std::filesystem::exists(candidate)) return candidate;
    }
}

/*!
 * A file with a unique name in the system temporary directory.
 * The constructor creates an empty file (unless create is false);
 * the destructor removes the path.
 */
class TempFile
{
public:
    explicit TempFile(const bool create = true):
        _path(uniqueTempPath())
    {
        if (not create) return;
        std::ofstream file(_path, std::ios::binary);
        if (not file) throw Pothos::FileException("TempFile()", "cannot create " + _path.string());
    }

    ~TempFile(void)
    {
        std::error_code ec;
        std::filesystem::remove(_path, ec);
    }

    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;

    const std::filesystem::path &path(void) const
    {
        return _path;
    }

private:
    std::filesystem::path _path;
};

} //namespace Detail
} //namespace Util
} //namespace Pothos
