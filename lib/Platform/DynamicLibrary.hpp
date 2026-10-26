// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal platform layer: shared library loading
//(not installed, not part of the public API)

#pragma once
#include <filesystem>
#include <string>

namespace Pothos {
namespace Platform {

/*!
 * A loaded shared library (dlopen / LoadLibrary).
 * The library is unloaded by the destructor.
 */
class DynamicLibrary
{
public:
    //! Load a library, throws Pothos::LibraryLoadException with the path and the OS error
    explicit DynamicLibrary(const std::filesystem::path &path);

    ~DynamicLibrary(void);

    DynamicLibrary(DynamicLibrary &&other) noexcept;
    DynamicLibrary &operator=(DynamicLibrary &&other) noexcept;
    DynamicLibrary(const DynamicLibrary &) = delete;
    DynamicLibrary &operator=(const DynamicLibrary &) = delete;

    //! The address of a symbol, or nullptr when the library does not export it
    void *symbol(const std::string &name) const;

    //! The path given to the constructor
    const std::filesystem::path &path(void) const;

    //! File name suffix of shared libraries on this platform (".so", ".dylib", ".dll")
    static std::string suffix(void);

private:
    void *_handle;
    std::filesystem::path _path;
};

} //namespace Platform
} //namespace Pothos
