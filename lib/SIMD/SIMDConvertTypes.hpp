// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//The types simdConvertBuffer is instantiated for (internal)

#pragma once
#include <complex>
#include <cstdint>

//X(InType, OutType) for every scalar pair and every complex pair
#define POTHOS_SIMD_CONVERT_OUT(X, T) \
    X(T, char) X(T, std::int8_t) X(T, std::int16_t) X(T, std::int32_t) X(T, long) X(T, long long) \
    X(T, std::uint8_t) X(T, std::uint16_t) X(T, std::uint32_t) X(T, unsigned long) X(T, unsigned long long) \
    X(T, float) X(T, double)

#define POTHOS_SIMD_CONVERT_COMPLEX_OUT(X, T) \
    X(std::complex<T>, std::complex<char>) X(std::complex<T>, std::complex<std::int8_t>) \
    X(std::complex<T>, std::complex<std::int16_t>) X(std::complex<T>, std::complex<std::int32_t>) \
    X(std::complex<T>, std::complex<long>) X(std::complex<T>, std::complex<long long>) \
    X(std::complex<T>, std::complex<std::uint8_t>) X(std::complex<T>, std::complex<std::uint16_t>) \
    X(std::complex<T>, std::complex<std::uint32_t>) X(std::complex<T>, std::complex<unsigned long>) \
    X(std::complex<T>, std::complex<unsigned long long>) X(std::complex<T>, std::complex<float>) \
    X(std::complex<T>, std::complex<double>)

#define POTHOS_SIMD_CONVERT_IN(X, T) POTHOS_SIMD_CONVERT_OUT(X, T) POTHOS_SIMD_CONVERT_COMPLEX_OUT(X, T)

#define POTHOS_SIMD_CONVERT_PAIRS(X) \
    POTHOS_SIMD_CONVERT_IN(X, char) POTHOS_SIMD_CONVERT_IN(X, std::int8_t) POTHOS_SIMD_CONVERT_IN(X, std::int16_t) \
    POTHOS_SIMD_CONVERT_IN(X, std::int32_t) POTHOS_SIMD_CONVERT_IN(X, long) POTHOS_SIMD_CONVERT_IN(X, long long) \
    POTHOS_SIMD_CONVERT_IN(X, std::uint8_t) POTHOS_SIMD_CONVERT_IN(X, std::uint16_t) POTHOS_SIMD_CONVERT_IN(X, std::uint32_t) \
    POTHOS_SIMD_CONVERT_IN(X, unsigned long) POTHOS_SIMD_CONVERT_IN(X, unsigned long long) \
    POTHOS_SIMD_CONVERT_IN(X, float) POTHOS_SIMD_CONVERT_IN(X, double)
