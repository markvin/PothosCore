// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSD-3-Clause

//One copy per SIMD architecture (PothosGenerateSIMDSources)
#include "SIMD/XSIMDCopy.hpp"

#include <cmath>
#include <cstddef>

namespace PothosBlocksSIMD { namespace POTHOS_SIMD_NAMESPACE {

#define ROUND_FUNC(fcn) \
    template <typename T> \
    void fcn(const T* in, T* out, size_t len) \
    { \
        size_t i = 0; \
        if constexpr (Pothos::Util::XSIMDTraits<T>::IsSupported) \
        { \
            using Batch = xsimd::batch<T>; \
            for (; i + Batch::size <= len; i += Batch::size) \
            { \
                xsimd::fcn(Batch::load_unaligned(in+i)).store_unaligned(out+i); \
            } \
        } \
        for (; i < len; ++i) out[i] = std::fcn(in[i]); \
    } \
    template void fcn<float>(const float*, float*, size_t); \
    template void fcn<double>(const double*, double*, size_t);

ROUND_FUNC(ceil)
ROUND_FUNC(floor)
ROUND_FUNC(trunc)

}}
