// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Chooses, once per type pair, the SIMDConvert.cpp copy for this CPU

#include "SIMD/SIMDConvert.hpp"
#include "SIMD/SIMDConvertTypes.hpp"
#include "SIMDConvertArches.hpp" //generated
#include <Pothos/System/SIMD.hpp>
#include <cstddef>
#include <string>
#include <vector>

using ConvertFcn = void (*)(const void *, void *, std::size_t);

//the copies: one namespace each, all type pairs instantiated there
#define POTHOS_SIMD_CONVERT_DECLARE(ns) \
    namespace PothosSIMDConvert { namespace ns { \
        template <typename InType, typename OutType> void convert(const void *, void *, std::size_t); }}
POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_DECLARE)

namespace {

const std::string &chosenArch(void)
{
    static const std::string key = []
    {
        #define POTHOS_SIMD_CONVERT_KEY(ns) #ns,
        const std::vector<std::string> keys{POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_KEY)};
        return Pothos::System::getOptimalSIMDFeatureSetKey(keys);
    }();
    return key;
}

template <typename InType, typename OutType>
ConvertFcn chooseConvert(void)
{
    const auto &key = chosenArch();
    #define POTHOS_SIMD_CONVERT_MATCH(ns) \
        if (key == #ns) return &PothosSIMDConvert::ns::convert<InType, OutType>;
    POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_MATCH)
    return &PothosSIMDConvert::fallback::convert<InType, OutType>;
}

} //namespace

template <typename InType, typename OutType>
void simdConvertBuffer(const void *in, void *out, size_t bufferLen)
{
    static const ConvertFcn fcn = chooseConvert<InType, OutType>();
    fcn(in, out, bufferLen);
}

#define POTHOS_SIMD_CONVERT_INSTANTIATE(In, Out) template void simdConvertBuffer<In, Out>(const void *, void *, size_t);
POTHOS_SIMD_CONVERT_PAIRS(POTHOS_SIMD_CONVERT_INSTANTIATE)

std::string Pothos::Detail::simdConvertBufferArch(void)
{
    return chosenArch();
}
