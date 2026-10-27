// Copyright (c) 2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <complex>
#include <cstdlib>
#include <string>
#include <type_traits>

/*!
 * Convert a buffer of InType into OutType element by element, with the same
 * result as static_cast; the fastest copy for this CPU is chosen once.
 * Supported: char, int8..int32, long, long long, the unsigned variants,
 * float, double, and the complex of each (both complex).
 */
template <typename InType, typename OutType>
void simdConvertBuffer(const void*, void*, size_t);

namespace Pothos {
namespace Detail {

//! The key of the copy simdConvertBuffer uses ("fallback" or "x86_avx2__..."), for tests
std::string simdConvertBufferArch(void);

} //namespace Detail
} //namespace Pothos
