// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/DynamicLibrary.hpp"
#include <Pothos/Exception.hpp>
#include <utility>
#include <dlfcn.h>

Pothos::Platform::DynamicLibrary::DynamicLibrary(const std::filesystem::path &path):
    _handle(nullptr),
    _path(path)
{
    //lazy binding with global symbols (as modules were always loaded) for module interdependencies
    _handle = ::dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
    if (_handle == nullptr)
    {
        const char *err = ::dlerror();
        throw Pothos::LibraryLoadException(path.string(), err == nullptr ? "dlopen failed" : err);
    }
}

Pothos::Platform::DynamicLibrary::~DynamicLibrary(void)
{
    if (_handle != nullptr) ::dlclose(_handle);
}

Pothos::Platform::DynamicLibrary::DynamicLibrary(DynamicLibrary &&other) noexcept:
    _handle(std::exchange(other._handle, nullptr)),
    _path(std::move(other._path))
{
    return;
}

Pothos::Platform::DynamicLibrary &Pothos::Platform::DynamicLibrary::operator=(DynamicLibrary &&other) noexcept
{
    if (this == &other) return *this;
    if (_handle != nullptr) ::dlclose(_handle);
    _handle = std::exchange(other._handle, nullptr);
    _path = std::move(other._path);
    return *this;
}

void *Pothos::Platform::DynamicLibrary::symbol(const std::string &name) const
{
    return ::dlsym(_handle, name.c_str());
}

const std::filesystem::path &Pothos::Platform::DynamicLibrary::path(void) const
{
    return _path;
}

std::string Pothos::Platform::DynamicLibrary::suffix(void)
{
    #ifdef __APPLE__
    return ".dylib";
    #else
    return ".so";
    #endif
}
