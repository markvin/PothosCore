// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/SIMD.hpp>
#include "System/SIMDFeatures.hpp"
#include <algorithm>
#include <string>
#include <vector>

namespace {

//! Every name Pothos reports, fastest first (the order of the old implementation)
const std::vector<std::string> &knownFeatures(void)
{
    static const std::vector<std::string> names{
        "x86_avx512vl", "x86_avx512dq", "x86_avx512bw", "x86_avx512f", "x86_xop", "x86_fma4", "x86_fma3",
        "x86_avx2", "x86_avx", "x86_popcnt", "x86_sse4_1", "x86_ssse3", "x86_sse3", "x86_sse2",
        "neonfltsp", "neon", "power_vsx_207", "power_vsx_206", "power_altivec", "mips_msa"};
    return names;
}

} //namespace

POTHOS_TEST_BLOCK("/system/tests", test_simd_feature_names)
{
    const auto features = Pothos::System::getSupportedSIMDFeatureSet();
    std::size_t last = 0;
    for (const auto &name : features)
    {
        const auto it = std::ranges::find(knownFeatures(), name);
        POTHOS_TEST_TRUE(it != knownFeatures().end());
        const auto index = std::size_t(it - knownFeatures().begin());
        POTHOS_TEST_TRUE(index >= last); //fastest first
        last = index;
    }
    #if defined(__x86_64__) || defined(_M_X64)
    POTHOS_TEST_TRUE(std::ranges::find(features, "x86_sse2") != features.end());
    #elif defined(__aarch64__) || defined(_M_ARM64)
    POTHOS_TEST_TRUE(std::ranges::find(features, "neon") != features.end());
    #endif
    POTHOS_TEST_TRUE(features == Pothos::System::Detail::detectSIMDFeatures());
}

POTHOS_TEST_BLOCK("/system/tests", test_simd_optimal_key)
{
    using Pothos::System::getOptimalSIMDFeatureSetKey;
    const auto features = Pothos::System::getSupportedSIMDFeatureSet();
    POTHOS_TEST_EQUAL(getOptimalSIMDFeatureSetKey({}), "fallback");
    POTHOS_TEST_EQUAL(getOptimalSIMDFeatureSetKey({"no_such_arch"}), "fallback");
    if (features.empty()) return;

    //keys are sorted slow to fast: the last supported one wins
    std::string all;
    for (auto it = features.rbegin(); it != features.rend(); ++it) all += (all.empty()? "" : "__") + *it;
    POTHOS_TEST_EQUAL(getOptimalSIMDFeatureSetKey({features.back(), all, "no_such_arch"}), all);
    POTHOS_TEST_EQUAL(getOptimalSIMDFeatureSetKey({features.back(), all + "__no_such_arch"}), features.back());
}
