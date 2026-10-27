// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//metaprogramming with the Pothos::Util names
#include <Pothos/Util/Templates.hpp>
#include <functional>
#include <tuple>
#include <type_traits>

template <typename... Ts>
auto storeArgs(Ts&&... args)
{
    return std::tuple<Pothos::Util::special_decay_t<Ts>...>(std::forward<Ts>(args)...);
}

template <typename Tuple, size_t... Is>
int sumImpl(const Tuple &t, Pothos::Util::index_sequence<Is...>)
{
    return (0 + ... + int(std::get<Is>(t)));
}

template <typename... Ts>
int sum(const std::tuple<Ts...> &t)
{
    return sumImpl(t, Pothos::Util::index_sequence_for<Ts...>());
}

bool apiCompatTemplates(void)
{
    int x = 1;
    auto stored = storeArgs(std::ref(x), 2, 3);
    static_assert(std::is_same<decltype(stored), std::tuple<int &, int, int>>::value, "special_decay_t");
    x = 10;
    return sum(stored) == 15 and Pothos::Util::make_index_sequence<4>::size() == 4;
}
