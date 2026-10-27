// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <bit>
#include <concepts>

namespace BlocksByteOrder
{
    //! Convert an integer between host and network (big endian) byte order
    template <std::integral T>
    constexpr T toNetwork(const T value)
    {
        if constexpr (std::endian::native == std::endian::little) return std::byteswap(value);
        else return value;
    }

    template <std::integral T>
    constexpr T fromNetwork(const T value)
    {
        return toNetwork(value);
    }
}
