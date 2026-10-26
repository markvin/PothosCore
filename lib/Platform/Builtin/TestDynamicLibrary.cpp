// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include "Platform/DynamicLibrary.hpp"
#include <string>

using Pothos::Platform::DynamicLibrary;

POTHOS_TEST_BLOCK("/platform/tests", test_dynamic_library_load)
{
    #if defined(_WIN32)
    const std::string path("kernel32.dll"), name("GetCurrentProcessId");
    #elif defined(__APPLE__)
    const std::string path("/usr/lib/libSystem.B.dylib"), name("getpid");
    #else
    const std::string path("libc.so.6"), name("getpid");
    #endif
    DynamicLibrary lib(path);
    POTHOS_TEST_TRUE(lib.symbol(name) != nullptr);
    POTHOS_TEST_TRUE(lib.symbol("pothos_no_such_symbol") == nullptr);
}

POTHOS_TEST_BLOCK("/platform/tests", test_dynamic_library_missing)
{
    POTHOS_TEST_THROWS(DynamicLibrary("/no/such/lib.so"), Pothos::LibraryLoadException);
    try
    {
        DynamicLibrary lib("/no/such/lib.so");
    }
    catch (const Pothos::LibraryLoadException &ex)
    {
        POTHOS_TEST_TRUE(ex.displayText().find("/no/such/lib.so") != std::string::npos);
    }
}
