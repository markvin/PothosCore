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
 * arm64_neon, arm_neon_flt_sp, arm_neon, neonfltsp, neon (ARM; the arm_* names
 * are the names of the SIMD copies, neon and neonfltsp are the old names);
 * power_vsx_207, power_vsx_206, power_altivec (PowerPC).
 */
std::vector<std::string> detectSIMDFeatures(void);

//! All names that detectSIMDFeatures() can report on any processor
std::vector<std::string> knownSIMDFeatureNames(void);

} //namespace Detail
} //namespace System
} //namespace Pothos
