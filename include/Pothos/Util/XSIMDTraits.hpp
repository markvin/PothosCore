// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#pragma once

#ifndef XSIMD_TRAITS_HPP
#ifdef _MSC_VER
#pragma message("Warning: Included without <xsimd/types/xsimd_traits.hpp>")
#else
#warning Included without <xsimd/types/xsimd_traits.hpp>
#endif
#endif

#include <complex>
#include <cstdint>
#include <type_traits>

namespace Pothos {
namespace Util {

//
// This header is to be included by XSIMD implementation files, after
// <xsimd/xsimd.hpp>. The compile flags used for the file determine the xsimd
// architecture (xsimd::default_arch), and with it which types have SIMD
// registers. This allows us to use the SFINAE structs below to determine
// whether to build SIMD implementations for a given function or fall back
// on to a default implementation.
//
// The core does not install xsimd: the module that uses this header must
// find its own xsimd (find_package(xsimd)) and link it. In a file that
// pothos_multiarch compiles, include <Pothos/Util/XSIMDCopy.hpp> instead.
//

namespace Detail {

//! The scalar types that the traits consider (complex types use their element type)
template <typename T>
constexpr bool IsXSIMDScalar =
    std::is_same_v<T, std::int8_t> or std::is_same_v<T, std::int16_t> or
    std::is_same_v<T, std::int32_t> or std::is_same_v<T, std::int64_t> or
    std::is_same_v<T, std::uint8_t> or std::is_same_v<T, std::uint16_t> or
    std::is_same_v<T, std::uint32_t> or std::is_same_v<T, std::uint64_t> or
    std::is_same_v<T, float> or std::is_same_v<T, double>;

template <typename T>
constexpr bool hasXSIMDRegister(void)
{
#if defined(XSIMD_TRAITS_HPP) && !defined(XSIMD_NO_SUPPORTED_ARCHITECTURE)
    if constexpr (IsXSIMDScalar<T>) return xsimd::has_simd_register<T>::value;
#endif
    return false;
}

} //namespace Detail

template <typename T>
struct XSIMDTraits
{
    static constexpr bool IsSupported = Detail::hasXSIMDRegister<T>();
};

template <typename T>
struct XSIMDTraits<std::complex<T>>
{
    static constexpr bool IsSupported = Detail::hasXSIMDRegister<T>();
};

template <typename T, typename Ret>
using EnableIfXSIMDSupports = typename std::enable_if<XSIMDTraits<T>::IsSupported, Ret>::type;

template <typename T, typename Ret>
using EnableIfXSIMDDoesNotSupport = typename std::enable_if<!XSIMDTraits<T>::IsSupported, Ret>::type;

} // namespace Util
} // namespace Pothos
