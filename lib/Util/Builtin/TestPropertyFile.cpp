// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include "Util/PropertyFile.hpp"
#include "Util/TempFile.hpp"
#include <sstream>
#include <string>

using namespace Pothos::Util::Detail;

POTHOS_TEST_BLOCK("/util/tests", test_property_file_compat)
{
    //the grammar of the Poco PropertyFileConfiguration class
    std::istringstream is(
        "# comment\r\n"
        "! also a comment\n"
        "  a = 1  \n"
        "b: two words\n"
        "c=\n"
        "d\n"
        "e = x\\tz\n"
        "f = line1 \\\n"
        "  line2\n"
        "g = \\\\\n"
        "h = say \\\"hi\\\" \\q\r\n"
        "\n"
        "   \n"
        "a = 3\n");
    const auto map = parseProperties(is);
    const PropertyMap expected{
        {"a", "3"}, {"b", "two words"}, {"c", ""}, {"d", ""}, {"e", "x\tz"},
        {"f", "line1   line2"}, {"g", "\\"}, {"h", "say \"hi\" q"}};
    POTHOS_TEST_TRUE(map == expected);
}

POTHOS_TEST_BLOCK("/util/tests", test_property_file_roundtrip)
{
    const PropertyMap map{
        {"plain", "value"}, {"specials", "a\nb\tc\\d\re\ff"}, {"punct", "k: v = w # x ! y"},
        {"path_to_lib", "/usr/lib/libPothos.so"}, {"empty", ""}};
    std::istringstream is(formatProperties(map));
    POTHOS_TEST_TRUE(parseProperties(is) == map);
    POTHOS_TEST_EQUAL(formatProperties({{"k", "a\tb"}}), "k: a\\tb\n");
}

POTHOS_TEST_BLOCK("/util/tests", test_property_file_io)
{
    TempFile file;
    const PropertyMap map{{"loader", "block_desc"}, {"paths", "/a/b.cpp\n/c/d.cpp"}};
    savePropertyFile(file.path(), map);
    POTHOS_TEST_TRUE(loadPropertyFile(file.path()) == map);
    auto missing = file.path();
    missing += ".missing";
    POTHOS_TEST_THROWS(loadPropertyFile(missing), Pothos::FileException);
}
