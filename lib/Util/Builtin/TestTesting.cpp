// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <string>
#include <string_view>

POTHOS_TEST_BLOCK("/testing/tests", test_testing_string_compare)
{
    //C strings compare by content, not by address
    const char a[] = "abc";
    const std::string b = "abc";
    char mutableCopy[] = "abc";
    POTHOS_TEST_EQUAL(a, "abc");
    POTHOS_TEST_EQUAL(b.c_str(), a);
    POTHOS_TEST_EQUAL(static_cast<const char *>(mutableCopy), a);
    POTHOS_TEST_EQUAL(&mutableCopy[0], b);
    POTHOS_TEST_EQUAL(std::string_view("abc"), a);
    POTHOS_TEST_EQUAL(b, a);
    POTHOS_TEST_NOT_EQUAL(a, "abd");
    POTHOS_TEST_LT(a, "abd");
    POTHOS_TEST_GE(b.c_str(), "abc");

    //a null C string is not equal to any string and is printable
    const char *null = nullptr;
    POTHOS_TEST_NOT_EQUAL(null, "x");
    POTHOS_TEST_NOT_EQUAL(null, "");

    //other types are unchanged
    POTHOS_TEST_EQUAL(1, 1.0);
    POTHOS_TEST_NOT_EQUAL(2, 3);
    POTHOS_TEST_LT(2, 3);
}
