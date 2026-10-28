// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSD-3-Clause

//One copy per SIMD architecture (PothosGenerateSIMDSources)
#include <Pothos/Util/XSIMDCopy.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace PothosBlocksSIMD { namespace POTHOS_SIMD_NAMESPACE {

//! Element-wise minimum and maximum across numInputs inputs (numInputs > 0)
template <typename T>
void minmax(const T** inPtrs, T* minOut, T* maxOut, size_t numInputs, size_t len)
{
    size_t i = 0;
    if constexpr (Pothos::Util::XSIMDTraits<T>::IsSupported)
    {
        using Batch = xsimd::batch<T>;
        for (; i + Batch::size <= len; i += Batch::size)
        {
            auto regMin = Batch::load_unaligned(inPtrs[0]+i);
            auto regMax = regMin;
            for (size_t input = 1; input < numInputs; ++input)
            {
                const auto regIn = Batch::load_unaligned(inPtrs[input]+i);
                regMin = xsimd::min(regIn, regMin);
                regMax = xsimd::max(regIn, regMax);
            }
            regMin.store_unaligned(minOut+i);
            regMax.store_unaligned(maxOut+i);
        }
    }
    for (; i < len; ++i)
    {
        minOut[i] = maxOut[i] = inPtrs[0][i];
        for (size_t input = 1; input < numInputs; ++input)
        {
            minOut[i] = std::min(minOut[i], inPtrs[input][i]);
            maxOut[i] = std::max(maxOut[i], inPtrs[input][i]);
        }
    }
}

#define MINMAX(T) template void minmax<T>(const T**, T*, T*, size_t, size_t);
MINMAX(std::int8_t)
MINMAX(std::int16_t)
MINMAX(std::int32_t)
MINMAX(std::int64_t)
MINMAX(std::uint8_t)
MINMAX(std::uint16_t)
MINMAX(std::uint32_t)
MINMAX(std::uint64_t)
MINMAX(float)
MINMAX(double)

}}
