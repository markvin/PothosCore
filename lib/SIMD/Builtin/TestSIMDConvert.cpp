// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/SIMD.hpp>
#include "SIMD/SIMDConvert.hpp"
#include "SIMDConvertArches.hpp" //generated
#include <algorithm>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#define POTHOS_SIMD_CONVERT_DECLARE(ns) \
    namespace PothosSIMDConvert { namespace ns { \
        template <typename InType, typename OutType> void convert(const void *, void *, std::size_t); \
        const char *xsimdArch(void); }}
POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_DECLARE)

namespace {

using ConvertFcn = void (*)(const void *, void *, std::size_t);

//! The conversion of one copy (arch key), nullptr when the key is unknown
template <typename In, typename Out>
ConvertFcn copyConvert(const std::string &key)
{
    #define POTHOS_SIMD_CONVERT_MATCH(ns) if (key == #ns) return &PothosSIMDConvert::ns::convert<In, Out>;
    POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_MATCH)
    return nullptr;
}

const std::vector<std::size_t> &testLengths(void)
{
    static const std::vector<std::size_t> lengths{0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 33, 63, 65, 1000};
    return lengths;
}

//! Random input values for which static_cast<Out> is defined
template <typename In, typename Out>
std::vector<In> randomInput(const std::size_t n, std::mt19937_64 &rng)
{
    std::vector<In> v(n);
    if constexpr (std::is_integral_v<In>)
    {
        std::uniform_int_distribution<long long> dist(
            static_cast<long long>(std::numeric_limits<In>::min()),
            std::is_signed_v<In>? static_cast<long long>(std::numeric_limits<In>::max()) : std::numeric_limits<long long>::max());
        for (auto &x : v) x = static_cast<In>(dist(rng) ^ static_cast<long long>(rng())); //all bit patterns of In
        if (n > 0) v[0] = std::numeric_limits<In>::min();
        if (n > 1) v[1] = std::numeric_limits<In>::max();
    }
    else
    {
        //floating point: within the range of Out (float to integer is undefined outside it)
        double lo = -1e30, hi = 1e30;
        if constexpr (std::is_integral_v<Out>)
        {
            lo = std::is_signed_v<Out>? std::max(-1e18, double(std::numeric_limits<Out>::min())) : 0.0;
            hi = std::min(1e18, double(std::numeric_limits<Out>::max()) * 0.99);
        }
        std::uniform_real_distribution<double> dist(lo, hi);
        for (auto &x : v) x = static_cast<In>(dist(rng));
    }
    return v;
}

template <typename In, typename Out>
bool convertMatchesScalar(std::mt19937_64 &rng, const std::string &key)
{
    const ConvertFcn scalar = key.empty()? &simdConvertBuffer<In, Out> : copyConvert<In, Out>(key);
    const ConvertFcn complex = key.empty()? &simdConvertBuffer<std::complex<In>, std::complex<Out>> : copyConvert<std::complex<In>, std::complex<Out>>(key);
    for (const auto n : testLengths())
    {
        const auto in = randomInput<In, Out>(n, rng);
        std::vector<Out> expected(n), actual(n);
        for (std::size_t i = 0; i < n; i++) expected[i] = static_cast<Out>(in[i]);
        scalar(in.data(), actual.data(), n);
        if (n != 0 and std::memcmp(expected.data(), actual.data(), n*sizeof(Out)) != 0) return false;

        //complex pairs convert the real and imaginary parts
        std::vector<std::complex<In>> cin(n);
        for (std::size_t i = 0; i < n; i++) cin[i] = std::complex<In>(in[i], in[n-1-i]);
        std::vector<std::complex<Out>> cexp(n), cact(n);
        for (std::size_t i = 0; i < n; i++) cexp[i] = std::complex<Out>(static_cast<Out>(cin[i].real()), static_cast<Out>(cin[i].imag()));
        complex(cin.data(), cact.data(), n);
        if (n != 0 and std::memcmp(cexp.data(), cact.data(), n*sizeof(std::complex<Out>)) != 0) return false;
    }
    return true;
}

using Types = std::tuple<char, std::int8_t, std::int16_t, std::int32_t, long, long long,
    std::uint8_t, std::uint16_t, std::uint32_t, unsigned long, unsigned long long, float, double>;

template <typename In, std::size_t... I>
void checkAllOut(std::mt19937_64 &rng, const std::string &key, std::index_sequence<I...>, std::vector<std::string> &failures)
{
    ((convertMatchesScalar<In, std::tuple_element_t<I, Types>>(rng, key)? void() :
        failures.push_back(key + ": " + typeid(In).name() + " -> " + typeid(std::tuple_element_t<I, Types>).name())), ...);
}

//! key empty: simdConvertBuffer itself, otherwise that copy
template <std::size_t... I>
void checkAll(std::mt19937_64 &rng, const std::string &key, std::index_sequence<I...> seq, std::vector<std::string> &failures)
{
    (checkAllOut<std::tuple_element_t<I, Types>>(rng, key, seq, failures), ...);
}

} //namespace

POTHOS_TEST_BLOCK("/framework/tests", test_simd_convert_matches_scalar)
{
    std::mt19937_64 rng(1234);
    std::vector<std::string> failures;
    checkAll(rng, "", std::make_index_sequence<std::tuple_size_v<Types>>(), failures);

    //every copy this CPU can run, not only the chosen one
    #define POTHOS_SIMD_CONVERT_KEY(ns) #ns,
    const std::vector<std::string> keys{POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_KEY)};
    for (const auto &key : keys)
    {
        if (key != "fallback" and Pothos::System::getOptimalSIMDFeatureSetKey({key}) != key) continue;
        std::cout << "checking copy " << key << std::endl;
        checkAll(rng, key, std::make_index_sequence<std::tuple_size_v<Types>>(), failures);
    }
    for (const auto &f : failures) std::cerr << "mismatch: " << f << std::endl;
    POTHOS_TEST_TRUE(failures.empty());
}

POTHOS_TEST_BLOCK("/framework/tests", test_simd_convert_unaligned)
{
    //offset by one byte, guard bytes around the output
    constexpr std::size_t n = 67;
    std::vector<unsigned char> inBytes(1 + n*sizeof(std::int32_t));
    std::vector<unsigned char> outBytes(1 + n*sizeof(float) + 16, 0xA5);
    std::vector<std::int32_t> in(n);
    for (std::size_t i = 0; i < n; i++) in[i] = std::int32_t(i*7919) - 100000;
    std::memcpy(inBytes.data()+1, in.data(), n*sizeof(std::int32_t));

    simdConvertBuffer<std::int32_t, float>(inBytes.data()+1, outBytes.data()+1, n);
    POTHOS_TEST_EQUAL(int(outBytes[0]), 0xA5);
    for (std::size_t i = 0; i < 16; i++) POTHOS_TEST_EQUAL(int(outBytes[1 + n*sizeof(float) + i]), 0xA5);
    for (std::size_t i = 0; i < n; i++)
    {
        float f = 0;
        std::memcpy(&f, outBytes.data()+1+i*sizeof(float), sizeof(float));
        POTHOS_TEST_EQUAL(f, static_cast<float>(in[i]));
    }
}

POTHOS_TEST_BLOCK("/framework/tests", test_simd_convert_dispatch)
{
    const auto key = Pothos::Detail::simdConvertBufferArch();
    std::cout << "SIMD conversion uses " << key << std::endl;
    if (key == "fallback") return;
    const auto supported = Pothos::System::getSupportedSIMDFeatureSet();
    std::size_t pos = 0;
    while (pos <= key.size())
    {
        const auto end = std::min(key.find("__", pos), key.size());
        const auto feature = key.substr(pos, end-pos);
        POTHOS_TEST_TRUE(std::ranges::find(supported, feature) != supported.end());
        pos = end + 2;
    }
}

POTHOS_TEST_BLOCK("/framework/tests", test_simd_convert_copy_flags)
{
    //each copy must really be built for its architecture: a lost compiler flag
    //would leave e.g. the AVX-512 copy as SSE2 code that still wins the dispatch
    #define POTHOS_SIMD_CONVERT_ARCH_NAME(ns) {#ns, PothosSIMDConvert::ns::xsimdArch()},
    const std::vector<std::pair<std::string, std::string>> copies{POTHOS_SIMD_CONVERT_ARCHES(POTHOS_SIMD_CONVERT_ARCH_NAME)};
    for (const auto &[key, arch] : copies)
    {
        std::cout << key << " -> xsimd " << arch << std::endl;
        if (key.find("x86_avx512") != std::string::npos) {POTHOS_TEST_TRUE(arch.starts_with("avx512"));}
        else if (key.find("x86_avx2") != std::string::npos) {POTHOS_TEST_TRUE(arch.find("avx2") != std::string::npos);}
        else if (key.find("x86_avx") != std::string::npos) {POTHOS_TEST_TRUE(arch.find("avx") != std::string::npos);}
    }
}
