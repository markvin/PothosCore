// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//CPU feature detection behind getSupportedSIMDFeatureSet (internal, not installed)

#pragma once
#include <string>
#include <vector>

namespace Pothos {
namespace System {
namespace Detail {

/*!
 * The SIMD feature names this processor supports, fastest first:
 * x86_avx512vl, x86_avx512dq, x86_avx512bw, x86_avx512f, x86_fma4, x86_fma3,
 * x86_avx2, x86_avx, x86_popcnt, x86_sse4_1, x86_ssse3, x86_sse3, x86_sse2;
 * neonfltsp, neon (ARM); power_vsx_207, power_vsx_206, power_altivec (PowerPC).
 * x86_xop and mips_msa are not detected (xsimd has no support for them).
 */
std::vector<std::string> detectSIMDFeatures(void);

} //namespace Detail
} //namespace System
} //namespace Pothos
