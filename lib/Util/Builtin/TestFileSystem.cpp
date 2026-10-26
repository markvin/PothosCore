// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Util/FileSystem.hpp"
#include <cstdlib>
#include <string>

static void setTestEnv(const char *name, const char *value)
{
    #ifdef _WIN32
    _putenv_s(name, value);
    #else
    setenv(name, value, 1);
    #endif
}

POTHOS_TEST_BLOCK("/util/tests", test_expand_path)
{
    using Pothos::Util::Detail::expandPath;
    setTestEnv("POTHOS_TEST_EXPAND", "/opt/x");

    POTHOS_TEST_EQUAL(expandPath("/plain/path.json"), "/plain/path.json");
    POTHOS_TEST_EQUAL(expandPath(""), "");
    #ifdef _WIN32
    POTHOS_TEST_EQUAL(expandPath("%POTHOS_TEST_EXPAND%\\a.json"), "/opt/x\\a.json");
    POTHOS_TEST_EQUAL(expandPath("%POTHOS_SURELY_UNSET_VARIABLE%a"), "a");
    #else
    POTHOS_TEST_EQUAL(expandPath("$POTHOS_TEST_EXPAND/a.json"), "/opt/x/a.json");
    POTHOS_TEST_EQUAL(expandPath("${POTHOS_TEST_EXPAND}b/a.json"), "/opt/xb/a.json");
    POTHOS_TEST_EQUAL(expandPath("$POTHOS_SURELY_UNSET_VARIABLE/a"), "/a");
    const char *home = std::getenv("HOME");
    if (home != nullptr)
    {
        POTHOS_TEST_EQUAL(expandPath("~/a.json"), std::string(home) + "/a.json");
        POTHOS_TEST_EQUAL(expandPath("~"), std::string(home));
    }
    POTHOS_TEST_EQUAL(expandPath("a~b"), "a~b");
    #endif
}
