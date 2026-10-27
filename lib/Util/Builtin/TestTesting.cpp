// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Object.hpp>
#include <string>
#include <string_view>

namespace
{
    struct NonConstEqual
    {
        bool operator==(NonConstEqual &) {return true;}
    };
}

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

    //a C string against a non-string compares as before (by address)
    const char *ptr = a;
    POTHOS_TEST_EQUAL(null, nullptr);
    POTHOS_TEST_NOT_EQUAL(ptr, nullptr);
    const void *voidPtr = a;
    POTHOS_TEST_EQUAL(ptr, voidPtr);
    //Object against a C string compiles (the result is Object's conversion, unchanged)
    const Pothos::Object obj(ptr);
    [[maybe_unused]] const auto objectCompare = [&]{POTHOS_TEST_EQUAL(obj, ptr);};

    //a char array without a terminator is read only within its bounds
    const char unterminated[3] = {'a', 'b', 'c'};
    POTHOS_TEST_EQUAL(unterminated, "abc");

    //types with a non-const comparison operator
    NonConstEqual x, y;
    POTHOS_TEST_EQUAL(x, y);
}
