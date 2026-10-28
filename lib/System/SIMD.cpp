// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Object.hpp>
#include <Pothos/System/SIMD.hpp>

#include "System/SIMDFeatures.hpp"
#include <xsimd/config/xsimd_config.hpp> //XSIMD_VERSION_*
#include <xsimd/config/xsimd_cpu_features.hpp>
#include <algorithm>
#include <sstream>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define POTHOS_SIMD_X86
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

//xsimd 14.2 does not detect AVX-512VL (14.3 does): then read CPUID leaf 7,
//subleaf 0, EBX bit 31. The caller also requires AVX-512F, which xsimd
//checks with the state that the OS enables (XCR0).
static bool cpuHasAVX512VL(const xsimd::cpu_features &cpu)
{
#if XSIMD_VERSION_MAJOR * 100 + XSIMD_VERSION_MINOR >= 1403
    return cpu.avx512vl();
#else
    (void)cpu;
#ifdef POTHOS_SIMD_X86
#ifdef _MSC_VER
    int regs[4] = {};
    __cpuidex(regs, 0, 0);
    if (regs[0] < 7) return false;
    __cpuidex(regs, 7, 0);
    const unsigned ebx = static_cast<unsigned>(regs[1]);
#else
    if (__get_cpuid_max(0, nullptr) < 7) return false;
    unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
#endif
    return (ebx & (1u << 31)) != 0;
#else
    return false;
#endif
#endif
}

std::vector<std::string> Pothos::System::Detail::detectSIMDFeatures(void)
{
    const auto cpu = xsimd::cpu_features();
    std::vector<std::string> out;
    const auto add = [&out](const bool supported, const char *name){if (supported) out.emplace_back(name);};

    //x86: a level implies the levels it builds on (the composition Pothos always used)
    const bool sse2 = cpu.sse2(), sse3 = sse2 and cpu.sse3(), ssse3 = sse3 and cpu.ssse3();
    const bool sse4_1 = ssse3 and cpu.sse4_1(), avx = sse4_1 and cpu.avx(), avx2 = avx and cpu.avx2();
    const bool avx512f = avx2 and cpu.avx512f();
    add(avx512f and cpuHasAVX512VL(cpu), "x86_avx512vl");
    add(avx512f and cpu.avx512dq(), "x86_avx512dq");
    add(avx512f and cpu.avx512bw(), "x86_avx512bw");
    add(avx512f, "x86_avx512f");
    add(sse3 and cpu.fma4(), "x86_fma4");
    add(sse3 and cpu.fma3(), "x86_fma3");
    add(avx2, "x86_avx2");
    add(avx, "x86_avx");
    add(cpu.popcnt(), "x86_popcnt");
    add(sse4_1, "x86_sse4_1");
    add(ssse3, "x86_ssse3");
    add(sse3, "x86_sse3");
    add(sse2, "x86_sse2");

    //ARM: NEON (64-bit NEON always has single precision float support)
    const bool neon = cpu.neon64() or cpu.neon();
    add(neon, "neonfltsp");
    add(neon, "neon");

    //PowerPC: xsimd detects VSX (POWER8 and newer), which includes the older levels
    add(cpu.vsx(), "power_vsx_207");
    add(cpu.vsx(), "power_vsx_206");
    add(cpu.vsx(), "power_altivec");
    return out;
}

//
// Pothos-level utility code
//
static const std::string SEPARATOR = "__";

//
// For a given file, PothosConfigSIMD generates copies of it that compile with different sets of SIMD
// flags. Each file exports a copy of the function with a namespace reflecting the supported feature
// sets that correspond to that file. This function separates that namespace into the feature sets
// to be checked against the feature sets supported by the current processor.
//

static std::vector<std::string> _separateSIMDFeatureSetKey(const std::string& featureSetKey)
{
    // "x86_sse2__x86_sse3__x86_ssse3" -> {"x86_sse2","x86_sse3","x86_ssse3"}
    std::string keyCopy(featureSetKey);
    std::vector<std::string> separatedKey;

    size_t pos = 0;
    std::string token;
    while ((pos = keyCopy.find(SEPARATOR)) != std::string::npos)
    {
        token = keyCopy.substr(0, pos);
        separatedKey.emplace_back(token);
        keyCopy.erase(0, pos + SEPARATOR.length());
    }
    separatedKey.emplace_back(keyCopy); // last entry

    return separatedKey;
}

static bool _isSIMDFeatureSetSupported(const std::vector<std::string>& featureSet)
{
    const auto supportedSIMDFeatureSet = Pothos::System::getSupportedSIMDFeatureSet();

    auto unsupportedFeatureIter = std::find_if(
                                      featureSet.begin(),
                                      featureSet.end(),
                                      [&](const std::string& feature)
                                      {
                                          auto supportedFeatureIter =
                                                   std::find(
                                                       supportedSIMDFeatureSet.begin(),
                                                       supportedSIMDFeatureSet.end(),
                                                       feature);

                                          return (supportedFeatureIter == supportedSIMDFeatureSet.end());
                                      });

    return (unsupportedFeatureIter == featureSet.end());
}

static bool _isSIMDFeatureSetSupported(const std::string& featureSetKey)
{
    return _isSIMDFeatureSetSupported(_separateSIMDFeatureSetKey(featureSetKey));
}

//
// Exported functions
//

std::vector<std::string> Pothos::System::getSupportedSIMDFeatureSet()
{
    // Only do this once
    static const auto supportedSIMDFeatureSet = Detail::detectSIMDFeatures();

    return supportedSIMDFeatureSet;
}

POTHOS_API std::string Pothos::System::getOptimalSIMDFeatureSetKey(const std::vector<std::string>& featureSetKeys)
{
    std::vector<std::string> keys;
    std::copy_if(
        featureSetKeys.begin(),
        featureSetKeys.end(),
        std::back_inserter(keys),
        [](const std::string& key)
        {
            return _isSIMDFeatureSetSupported(key);
        });
    if(keys.empty()) return "fallback";
    else             return keys.back(); // Sorted in ascending order of performance
}
