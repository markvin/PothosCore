// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Util/Templates.hpp>
#include <functional>
#include <type_traits>
#include <utility>

//the public names are aliases of the standard ones
static_assert(std::is_same_v<Pothos::Util::index_sequence<0, 1>, std::index_sequence<0, 1>>);
static_assert(std::is_same_v<Pothos::Util::make_index_sequence<3>, std::make_index_sequence<3>>);
static_assert(std::is_same_v<Pothos::Util::index_sequence_for<int, char>, std::index_sequence_for<int, char>>);
static_assert(std::is_same_v<Pothos::Util::special_decay_t<std::reference_wrapper<int>>, int &>);
static_assert(std::is_same_v<Pothos::Util::special_decay_t<const int &>, int>);
static_assert(std::is_same_v<Pothos::Util::unwrap_refwrapper<std::reference_wrapper<int>>::type, int &>);
static_assert(std::is_same_v<Pothos::Util::unwrap_refwrapper<int>::type, int>);

POTHOS_TEST_BLOCK("/util/tests", test_templates_aliases)
{
    //checked at compile time by the static asserts above
    POTHOS_TEST_EQUAL(Pothos::Util::make_index_sequence<3>::size(), 3u);
}
