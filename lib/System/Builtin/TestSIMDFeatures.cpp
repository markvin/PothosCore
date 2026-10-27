// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/SIMD.hpp>
#include "System/SIMDFeatures.hpp"
#include <algorithm>
#include <string>
#include <vector>

//temporary cross-check against libsimdpp while it is still in the tree (removed in phase 5a Task 3)
#include <simdpp/simd.h>
#include <simdpp/dispatch/get_arch_gcc_builtin_cpu_supports.h>
#include <simdpp/dispatch/get_arch_linux_cpuinfo.h>
#include <simdpp/dispatch/get_arch_raw_cpuid.h>

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

std::vector<std::string> simdppFeatures(void)
{
    using simdpp::Arch;
#if SIMDPP_HAS_GET_ARCH_RAW_CPUID
    const auto info = simdpp::get_arch_raw_cpuid();
#elif SIMDPP_HAS_GET_ARCH_GCC_BUILTIN_CPU_SUPPORTS
    const auto info = simdpp::get_arch_gcc_builtin_cpu_supports();
#elif SIMDPP_HAS_GET_ARCH_LINUX_CPUINFO
    const auto info = simdpp::get_arch_linux_cpuinfo();
#else
    const auto info = Arch::NONE_NULL;
#endif
    std::vector<std::pair<std::string, Arch>> table;
#if SIMDPP_X86
    const Arch sse2 = Arch::X86_SSE2, sse3 = sse2 | Arch::X86_SSE3, ssse3 = sse3 | Arch::X86_SSSE3;
    const Arch sse4_1 = ssse3 | Arch::X86_SSE4_1, avx = sse4_1 | Arch::X86_AVX, avx2 = avx | Arch::X86_AVX2;
    const Arch avx512f = avx2 | Arch::X86_AVX512F;
    table = {
        {"x86_avx512vl", avx512f | Arch::X86_AVX512VL}, {"x86_avx512dq", avx512f | Arch::X86_AVX512DQ},
        {"x86_avx512bw", avx512f | Arch::X86_AVX512BW}, {"x86_avx512f", avx512f},
        {"x86_fma4", sse3 | Arch::X86_FMA4}, {"x86_fma3", sse3 | Arch::X86_FMA3},
        {"x86_avx2", avx2}, {"x86_avx", avx}, {"x86_popcnt", Arch::X86_POPCNT_INSN},
        {"x86_sse4_1", sse4_1}, {"x86_ssse3", ssse3}, {"x86_sse3", sse3}, {"x86_sse2", sse2}};
#elif SIMDPP_ARM && SIMDPP_64_BITS
    table = {{"neonfltsp", Arch::ARM_NEON | Arch::ARM_NEON_FLT_SP}, {"neon", Arch::ARM_NEON | Arch::ARM_NEON_FLT_SP}};
#elif SIMDPP_ARM
    table = {{"neonfltsp", Arch::ARM_NEON | Arch::ARM_NEON_FLT_SP}, {"neon", Arch::ARM_NEON}};
#endif
    std::vector<std::string> out;
    for (const auto &[name, arch] : table) if (simdpp::test_arch_subset(info, arch)) out.push_back(name);
    return out;
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

POTHOS_TEST_BLOCK("/system/tests", test_simd_features_match_simdpp)
{
    POTHOS_TEST_TRUE(Pothos::System::Detail::detectSIMDFeatures() == simdppFeatures());
}
