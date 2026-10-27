// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSD-3-Clause

//One copy per SIMD architecture (PothosGenerateSIMDSources)
#include "SIMD/XSIMDCopy.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace PothosBlocksSIMD { namespace POTHOS_SIMD_NAMESPACE {

template <typename T>
void clamp(const T* in, T* out, const T& lo, const T& hi, size_t len)
{
    size_t i = 0;
    if constexpr (Pothos::Util::XSIMDTraits<T>::IsSupported)
    {
        using Batch = xsimd::batch<T>;
        const Batch loReg(lo), hiReg(hi);
        for (; i + Batch::size <= len; i += Batch::size)
        {
            xsimd::clip(Batch::load_unaligned(in+i), loReg, hiReg).store_unaligned(out+i);
        }
    }
    for (; i < len; ++i) out[i] = std::clamp(in[i], lo, hi);
}

#define CLAMP(T) template void clamp<T>(const T*, T*, const T&, const T&, size_t);
CLAMP(std::int8_t)
CLAMP(std::int16_t)
CLAMP(std::int32_t)
CLAMP(std::int64_t)
CLAMP(std::uint8_t)
CLAMP(std::uint16_t)
CLAMP(std::uint32_t)
CLAMP(std::uint64_t)
CLAMP(float)
CLAMP(double)

}}
