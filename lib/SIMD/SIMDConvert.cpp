// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//One copy of this file is compiled per SIMD architecture by pothos_multiarch
//(PothosConfigSIMD.cmake) with that architecture's flags; each copy lives in
//its own POTHOS_SIMD_NAMESPACE and SIMDConvertDispatch.cpp picks one at runtime.

#if !defined POTHOS_SIMD_NAMESPACE
#error Must define POTHOS_SIMD_NAMESPACE to build this file
#endif

#include "SIMD/SIMDConvertTypes.hpp"

//xsimd builds AVX-512 as the chain F -> CD -> DQ -> BW and rejects DQ/BW/VL
//without CD, which the Pothos AVX-512 copies do not enable (-mavx512cd):
//then hide the extensions above F while xsimd reads its configuration, it
//uses its AVX-512F kernels (GCC restores the macros after <immintrin.h>)
#if defined(__AVX512F__) && !defined(__AVX512CD__)
#undef __AVX512DQ__
#undef __AVX512BW__
#undef __AVX512VL__
#endif

//every copy gets its own xsimd namespace: identical inline xsimd functions
//compiled with different ISA flags must not be merged by the linker (an AVX
//version could otherwise run in the SSE copy on a CPU without AVX)
#define POTHOS_SIMD_CONCAT_(a, b) a##b
#define POTHOS_SIMD_CONCAT(a, b) POTHOS_SIMD_CONCAT_(a, b)
#define xsimd POTHOS_SIMD_CONCAT(xsimd_, POTHOS_SIMD_NAMESPACE)
#include <xsimd/xsimd.hpp>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace PothosSIMDConvert { namespace POTHOS_SIMD_NAMESPACE {

namespace {

template <typename T>
struct IsComplex : std::false_type {};

template <typename T>
struct IsComplex<std::complex<T>> : std::true_type {};

//! The fixed width type with the representation of T (char, long and long long map onto intN_t)
template <typename T>
using Canonical = std::conditional_t<std::is_floating_point_v<T>, T,
    std::conditional_t<std::is_signed_v<T>,
        std::conditional_t<sizeof(T) == 1, std::int8_t, std::conditional_t<sizeof(T) == 2, std::int16_t,
            std::conditional_t<sizeof(T) == 4, std::int32_t, std::int64_t>>>,
        std::conditional_t<sizeof(T) == 1, std::uint8_t, std::conditional_t<sizeof(T) == 2, std::uint16_t,
            std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>>>>>;

#ifdef XSIMD_NO_SUPPORTED_ARCHITECTURE
template <typename In, typename Out>
constexpr bool CanBatch = false;
#else
//! A register conversion: both types fill a register with the same number of
//! elements; float to integer stays scalar (static_cast semantics, see header)
template <typename In, typename Out>
constexpr bool CanBatch =
    not (std::is_floating_point_v<In> and std::is_integral_v<Out>) and
    xsimd::batch<In>::size == xsimd::batch<Out>::size;
#endif

template <typename In, typename Out>
void convertScalar(const In *in, Out *out, const std::size_t n)
{
    //a plain loop: the compiler vectorizes it with the flags of this copy
    for (std::size_t i = 0; i < n; i++) out[i] = static_cast<Out>(in[i]);
}

template <typename In, typename Out>
void convertCanonical(const In *in, Out *out, const std::size_t n)
{
    if constexpr (std::is_same_v<In, Out>)
    {
        if (n != 0) std::memcpy(out, in, n*sizeof(In));
    }
    else if constexpr (CanBatch<In, Out>)
    {
        constexpr std::size_t size = xsimd::batch<In>::size;
        const std::size_t vectorEnd = n - n % size;
        for (std::size_t i = 0; i < vectorEnd; i += size)
        {
            const auto reg = xsimd::batch<In>::load_unaligned(in + i);
            xsimd::batch_cast<Out>(reg).store_unaligned(out + i);
        }
        convertScalar(in + vectorEnd, out + vectorEnd, n - vectorEnd);
    }
    else convertScalar(in, out, n);
}

} //namespace

template <typename InType, typename OutType>
void convert(const void *in, void *out, const std::size_t n)
{
    if constexpr (IsComplex<InType>::value)
    {
        //the real and imaginary parts are an interleaved scalar buffer
        using In = Canonical<typename InType::value_type>;
        using Out = Canonical<typename OutType::value_type>;
        convertCanonical(static_cast<const In *>(in), static_cast<Out *>(out), 2*n);
    }
    else
    {
        using In = Canonical<InType>;
        using Out = Canonical<OutType>;
        convertCanonical(static_cast<const In *>(in), static_cast<Out *>(out), n);
    }
}

//! The xsimd architecture this copy was built for (checked by the self-tests)
const char *xsimdArch(void)
{
    #ifdef XSIMD_NO_SUPPORTED_ARCHITECTURE
    return "none";
    #else
    return xsimd::default_arch::name();
    #endif
}

#define POTHOS_SIMD_CONVERT_INSTANTIATE(In, Out) template void convert<In, Out>(const void *, void *, std::size_t);
POTHOS_SIMD_CONVERT_PAIRS(POTHOS_SIMD_CONVERT_INSTANTIATE)

}} //namespace PothosSIMDConvert::POTHOS_SIMD_NAMESPACE
