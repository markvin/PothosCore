// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <xsimd/xsimd.hpp>
#include <Pothos/Util/XSIMDTraits.hpp>
#include <complex>
#include <cstdint>
#include <string>

using Pothos::Util::XSIMDTraits;

POTHOS_TEST_BLOCK("/util/tests", test_xsimd_traits)
{
    //the baseline instruction set of x86-64 (SSE2) and arm64 (NEON) has registers
    //for every arithmetic type: the traits follow the xsimd architecture
    #ifndef XSIMD_NO_SUPPORTED_ARCHITECTURE
    POTHOS_TEST_TRUE(XSIMDTraits<float>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<double>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<std::int8_t>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<std::uint16_t>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<std::int32_t>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<std::uint64_t>::IsSupported);
    POTHOS_TEST_TRUE(XSIMDTraits<std::complex<float>>::IsSupported);
    #endif
    POTHOS_TEST_TRUE(not XSIMDTraits<std::string>::IsSupported);
    POTHOS_TEST_TRUE(not XSIMDTraits<bool>::IsSupported);
}
