// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal: xsimd for a file that pothos_multiarch (PothosConfigSIMD.cmake)
//compiles once per SIMD architecture, each copy in its own POTHOS_SIMD_NAMESPACE.
//Include it first, instead of <xsimd/xsimd.hpp>. Not installed.

#pragma once

#if !defined POTHOS_SIMD_NAMESPACE
#error Must define POTHOS_SIMD_NAMESPACE to build this file
#endif

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
#include <Pothos/Util/XSIMDTraits.hpp>
