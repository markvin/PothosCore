// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/DynamicLibrary.hpp"
#include <Pothos/Exception.hpp>
#include <utility>
#include <windows.h>

static std::string lastErrorText(void)
{
    const DWORD err = GetLastError();
    char *msg = nullptr;
    const DWORD n = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    std::string text = (n == 0 or msg == nullptr) ? "error " + std::to_string(err) : std::string(msg, n);
    if (msg != nullptr) LocalFree(msg);
    while (not text.empty() and (text.back() == '\n' or text.back() == '\r')) text.pop_back();
    return text;
}

Pothos::Platform::DynamicLibrary::DynamicLibrary(const std::filesystem::path &path):
    _handle(nullptr),
    _path(path)
{
    //altered search path: dependencies are found next to the library
    _handle = LoadLibraryExW(path.c_str(), nullptr, path.is_absolute() ? LOAD_WITH_ALTERED_SEARCH_PATH : 0);
    if (_handle == nullptr) throw Pothos::LibraryLoadException(path.string(), lastErrorText());
}

Pothos::Platform::DynamicLibrary::~DynamicLibrary(void)
{
    if (_handle != nullptr) FreeLibrary(static_cast<HMODULE>(_handle));
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
    if (_handle != nullptr) FreeLibrary(static_cast<HMODULE>(_handle));
    _handle = std::exchange(other._handle, nullptr);
    _path = std::move(other._path);
    return *this;
}

void *Pothos::Platform::DynamicLibrary::symbol(const std::string &name) const
{
    return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(_handle), name.c_str()));
}

const std::filesystem::path &Pothos::Platform::DynamicLibrary::path(void) const
{
    return _path;
}

std::string Pothos::Platform::DynamicLibrary::suffix(void)
{
    return ".dll";
}
