// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//a toolkit self test with the POTHOS_TEST_* macros
#include <Pothos/Testing.hpp>
#include <string>
#include <vector>

POTHOS_TEST_BLOCK("/api_compat/tests", test_macros)
{
    const std::vector<int> v{1, 2, 3};
    POTHOS_TEST_EQUAL(v.size(), 3);
    POTHOS_TEST_NOT_EQUAL(v[0], v[1]);
    POTHOS_TEST_LT(v[0], 2u);
    POTHOS_TEST_GE(v.back(), 3);
    POTHOS_TEST_TRUE(not v.empty());
    POTHOS_TEST_CLOSE(0.1 + 0.2, 0.3, 1e-9);
    POTHOS_TEST_EQUAL(std::string("abc"), "abc");
    POTHOS_TEST_EQUALV(v, std::vector<int>({1, 2, 3}));
    POTHOS_TEST_THROWS(throw std::runtime_error("x"), std::runtime_error);
}
