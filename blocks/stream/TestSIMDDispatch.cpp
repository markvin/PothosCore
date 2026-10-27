// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "StreamBlocks_SIMD.hpp" //generated

#include <Pothos/Testing.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

//
// The SIMD kernels that the dispatchers choose for this CPU give the results
// of the scalar code, also for the tail that does not fill a register
// (longer than four registers of the widest architecture, plus three)
//

static constexpr size_t TestLength = 4*64+3;

template <typename T>
static std::vector<T> testValues(const bool withNaN)
{
    std::vector<T> values;
    for (size_t i = 0; i < TestLength; ++i)
    {
        values.push_back(T(i*7 % 23) - T(11));
        if constexpr (std::is_floating_point_v<T>) values.back() += T(0.25)*T(i % 4);
    }
    if constexpr (std::is_floating_point_v<T>)
    {
        values[1] = std::numeric_limits<T>::infinity();
        values[5] = -std::numeric_limits<T>::infinity();
        values[9] = -T(0);
        values[13] = std::numeric_limits<T>::denorm_min();
        values[TestLength-1] = -std::numeric_limits<T>::infinity();
        if (withNaN)
        {
            values[3] = std::numeric_limits<T>::quiet_NaN();
            values[TestLength-2] = std::numeric_limits<T>::quiet_NaN();
        }
    }
    return values;
}

template <typename T>
static void testClamp(void)
{
    const auto in = testValues<T>(false);
    const T lo = std::is_signed_v<T>? T(-5) : T(2), hi = T(7);
    std::vector<T> out(in.size());
    PothosBlocksSIMD::clampDispatch<T>()(in.data(), out.data(), lo, hi, in.size());
    for (size_t i = 0; i < in.size(); ++i) POTHOS_TEST_EQUAL(out[i], std::clamp(in[i], lo, hi));
}

template <typename T>
static void testMinMax(void)
{
    const auto in0 = testValues<T>(false);
    auto in1 = in0;
    std::reverse(in1.begin(), in1.end());
    const T *ins[] = {in0.data(), in1.data()};
    std::vector<T> mins(in0.size()), maxs(in0.size());
    PothosBlocksSIMD::minmaxDispatch<T>()(ins, mins.data(), maxs.data(), 2, in0.size());
    for (size_t i = 0; i < in0.size(); ++i)
    {
        POTHOS_TEST_EQUAL(mins[i], std::min(in0[i], in1[i]));
        POTHOS_TEST_EQUAL(maxs[i], std::max(in0[i], in1[i]));
    }
}

template <typename T>
static void testRound(void)
{
    const auto in = testValues<T>(false);
    std::vector<T> out(in.size());
    PothosBlocksSIMD::ceilDispatch<T>()(in.data(), out.data(), in.size());
    for (size_t i = 0; i < in.size(); ++i) POTHOS_TEST_EQUAL(out[i], std::ceil(in[i]));
    PothosBlocksSIMD::floorDispatch<T>()(in.data(), out.data(), in.size());
    for (size_t i = 0; i < in.size(); ++i) POTHOS_TEST_EQUAL(out[i], std::floor(in[i]));
    PothosBlocksSIMD::truncDispatch<T>()(in.data(), out.data(), in.size());
    for (size_t i = 0; i < in.size(); ++i) POTHOS_TEST_EQUAL(out[i], std::trunc(in[i]));
}

template <typename T>
static void testIsX(void)
{
    const auto in = testValues<T>(true);
    std::vector<std::int8_t> out(in.size());
    const auto check = [&](auto dispatch, auto scalar)
    {
        std::fill(out.begin(), out.end(), std::int8_t(-1));
        dispatch(in.data(), out.data(), in.size());
        for (size_t i = 0; i < in.size(); ++i) POTHOS_TEST_EQUAL(int(out[i]), scalar(in[i])? 1 : 0);
    };
    check(PothosBlocksSIMD::isfiniteDispatch<T>(), [](T x){return std::isfinite(x);});
    check(PothosBlocksSIMD::isinfDispatch<T>(), [](T x){return std::isinf(x);});
    check(PothosBlocksSIMD::isnanDispatch<T>(), [](T x){return std::isnan(x);});
    check(PothosBlocksSIMD::isnormalDispatch<T>(), [](T x){return std::isnormal(x);});
    check(PothosBlocksSIMD::isnegativeDispatch<T>(), [](T x){return std::signbit(x);});
}

POTHOS_TEST_BLOCK("/blocks/tests", test_simd_dispatch_matches_scalar)
{
    testClamp<float>();
    testClamp<double>();
    testClamp<std::int16_t>();
    testClamp<std::int32_t>();
    testClamp<std::uint8_t>();
    testMinMax<float>();
    testMinMax<double>();
    testMinMax<std::int32_t>();
    testMinMax<std::uint16_t>();
    testRound<float>();
    testRound<double>();
    testIsX<float>();
    testIsX<double>();
}
