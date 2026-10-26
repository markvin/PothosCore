// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Util/TempFile.hpp"
#include <filesystem>
#include <string>

POTHOS_TEST_BLOCK("/util/tests", test_unique_temp_path)
{
    const auto p0 = Pothos::Util::Detail::uniqueTempPath(".cpp");
    const auto p1 = Pothos::Util::Detail::uniqueTempPath(".cpp");
    POTHOS_TEST_TRUE(p0 != p1);
    POTHOS_TEST_TRUE(not std::filesystem::exists(p0));
    POTHOS_TEST_EQUAL(p0.extension().string(), ".cpp");
    POTHOS_TEST_TRUE(p0.parent_path() == std::filesystem::temp_directory_path());
}

POTHOS_TEST_BLOCK("/util/tests", test_temp_file)
{
    std::filesystem::path p0, p1;
    {
        Pothos::Util::Detail::TempFile t0, t1;
        p0 = t0.path();
        p1 = t1.path();
        POTHOS_TEST_TRUE(std::filesystem::exists(p0));
        POTHOS_TEST_TRUE(std::filesystem::exists(p1));
        POTHOS_TEST_TRUE(p0 != p1);
        POTHOS_TEST_TRUE(p0.parent_path() == std::filesystem::temp_directory_path());
    }
    POTHOS_TEST_TRUE(not std::filesystem::exists(p0));
    POTHOS_TEST_TRUE(not std::filesystem::exists(p1));
}
