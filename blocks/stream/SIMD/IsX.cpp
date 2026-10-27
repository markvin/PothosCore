// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSD-3-Clause

//One copy per SIMD architecture (PothosGenerateSIMDSources)
#include "SIMD/XSIMDCopy.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace PothosBlocksSIMD { namespace POTHOS_SIMD_NAMESPACE {

namespace detail
{
    //the classifications of the scalar functions, one register at a time
    struct IsFinite
    {
        template <typename T> static bool scalar(const T x) {return std::isfinite(x);}
        template <typename B> static auto simd(const B &x) {return xsimd::isfinite(x);}
    };

    struct IsInf
    {
        template <typename T> static bool scalar(const T x) {return std::isinf(x);}
        template <typename B> static auto simd(const B &x) {return xsimd::isinf(x);}
    };

    struct IsNaN
    {
        template <typename T> static bool scalar(const T x) {return std::isnan(x);}
        template <typename B> static auto simd(const B &x) {return xsimd::isnan(x);}
    };

    struct IsNormal
    {
        template <typename T> static bool scalar(const T x) {return std::isnormal(x);}
        template <typename B> static auto simd(const B &x)
        {
            //not zero, subnormal, infinite or NaN
            using T = typename B::value_type;
            return (xsimd::abs(x) >= B(std::numeric_limits<T>::min())) && xsimd::isfinite(x);
        }
    };

    struct IsNegative
    {
        template <typename T> static bool scalar(const T x) {return std::signbit(x);}
        template <typename B> static auto simd(const B &x)
        {
            //the sign bit, like std::signbit (-0 and negative NaN included)
            using T = typename B::value_type;
            using Int = std::conditional_t<sizeof(T) == 4, std::int32_t, std::int64_t>;
            const auto negative = xsimd::bitwise_cast<Int>(x) < xsimd::batch<Int>(Int(0));
            return xsimd::batch_bool_cast<T>(negative);
        }
    };

    template <typename Class, typename T>
    void classify(const T* in, std::int8_t* out, size_t len)
    {
        size_t i = 0;
        if constexpr (Pothos::Util::XSIMDTraits<T>::IsSupported)
        {
            using Batch = xsimd::batch<T>;
            alignas(64) bool flags[Batch::size];
            for (; i + Batch::size <= len; i += Batch::size)
            {
                Class::simd(Batch::load_unaligned(in+i)).store_aligned(flags);
                for (size_t j = 0; j < Batch::size; ++j) out[i+j] = flags[j]? 1 : 0;
            }
        }
        for (; i < len; ++i) out[i] = Class::scalar(in[i])? 1 : 0;
    }
}

#define ISX_FUNC(fcn, Class) \
    template <typename T> \
    void fcn(const T* in, std::int8_t* out, size_t len) \
    { \
        detail::classify<detail::Class>(in, out, len); \
    } \
    template void fcn<float>(const float*, std::int8_t*, size_t); \
    template void fcn<double>(const double*, std::int8_t*, size_t);

ISX_FUNC(isfinite, IsFinite)
ISX_FUNC(isinf, IsInf)
ISX_FUNC(isnan, IsNaN)
ISX_FUNC(isnormal, IsNormal)
ISX_FUNC(isnegative, IsNegative)

}}
