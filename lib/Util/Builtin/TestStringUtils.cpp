// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Util/StringUtils.hpp"
#include <string>
#include <vector>

using namespace Pothos::Util::Detail;

POTHOS_TEST_BLOCK("/util/tests", test_string_trim)
{
    POTHOS_TEST_EQUAL(std::string(trim("  a b \t\n")), "a b");
    POTHOS_TEST_EQUAL(std::string(trim("")), "");
    POTHOS_TEST_EQUAL(std::string(trim("   ")), "");
}

POTHOS_TEST_BLOCK("/util/tests", test_string_case)
{
    POTHOS_TEST_EQUAL(toLower("AbC1"), "abc1");
    POTHOS_TEST_EQUAL(toUpper("abc"), "ABC");
    POTHOS_TEST_TRUE(iequals("Port:", "port:"));
    POTHOS_TEST_TRUE(not iequals("a", "ab"));
}

POTHOS_TEST_BLOCK("/util/tests", test_string_split)
{
    const std::vector<std::string> portLine{"Port:", "1234"};
    POTHOS_TEST_TRUE(split("Port: 1234", " ") == portLine);

    const std::vector<std::string> trimmed{"a", "b"};
    POTHOS_TEST_TRUE(split(" a , ,b ", ",") == trimmed);

    const std::vector<std::string> keepEmpty{"a", "", "b"};
    POTHOS_TEST_TRUE(split("a,,b", ",", true, false) == keepEmpty);

    POTHOS_TEST_TRUE(split("", ",").empty());
}

POTHOS_TEST_BLOCK("/util/tests", test_string_replace_all)
{
    POTHOS_TEST_EQUAL(replaceAll("a.b.c", ".", "::"), "a::b::c");
    POTHOS_TEST_EQUAL(replaceAll("aaa", "a", "aa"), "aaaaaa");
}
