// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Util/FileSystem.hpp"
#include "Platform/Environment.hpp"
#include <Pothos/Framework/SharedBuffer.hpp>
#include <Pothos/Util/FileLock.hpp>
#include <filesystem>
#include <fstream>
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

//A directory with a name that is not ASCII: on Windows, the core must give
//the UTF-8 text of a std::string path to the wide API, not to the ANSI API.
POTHOS_TEST_BLOCK("/util/tests", test_non_ascii_paths)
{
    using Pothos::Util::Detail::utf8Path;
    using Pothos::Util::Detail::pathUtf8;
    const std::string name = "pothos_\xD1\x82\xD0\xB5\xD1\x81\xD1\x82_\xC3\xA9_" + std::to_string(Pothos::Platform::currentProcessId());
    const auto dir = std::filesystem::temp_directory_path() / utf8Path(name);
    std::filesystem::create_directories(dir);
    const std::string base = pathUtf8(dir);
    POTHOS_TEST_TRUE(base.find(name) != std::string::npos);

    //the path text goes from UTF-8 to the path and back with no change
    POTHOS_TEST_EQUAL(pathUtf8(utf8Path(base)), base);

    //FileLock opens a file in the directory
    {
        Pothos::Util::FileLock lock(base + "/lock.\xD1\x84");
        lock.lock();
        lock.unlock();
    }
    POTHOS_TEST_TRUE(std::filesystem::exists(utf8Path(base + "/lock.\xD1\x84")));

    //a memory-mapped buffer of a file in the directory
    const std::string dataPath = base + "/data.\xD0\xB1\xD0\xB8\xD0\xBD";
    {
        std::ofstream file(utf8Path(dataPath), std::ios::binary);
        file << "0123456789abcdef";
    }
    {
        const auto buff = Pothos::SharedBuffer::makeFromFileMMap(dataPath, true, false);
        POTHOS_TEST_EQUAL(buff.getLength(), 16);
        POTHOS_TEST_EQUAL(std::string(reinterpret_cast<const char *>(buff.getAddress()), 16), "0123456789abcdef");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
