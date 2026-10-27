// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework/BufferChunk.hpp>
#include <complex>
#include <cstdint>
#include <span>

POTHOS_TEST_BLOCK("/framework/tests", test_buffer_chunk_span)
{
    //the trailing bytes that do not make a whole element are left out
    Pothos::BufferChunk chunk(10);
    const auto words = chunk.asSpan<std::uint32_t>();
    POTHOS_TEST_EQUAL(words.size(), 2u);
    POTHOS_TEST_TRUE(words.data() == chunk.as<std::uint32_t *>());

    //writes through the span land in the chunk
    words[1] = 0xdeadbeef;
    POTHOS_TEST_EQUAL(chunk.as<const std::uint32_t *>()[1], 0xdeadbeefu);

    //read-only view
    Pothos::BufferChunk floats(Pothos::DType(typeid(float)), 3);
    const std::span<const float> view = floats.asSpan<const float>();
    POTHOS_TEST_EQUAL(view.size(), 3u);
    POTHOS_TEST_TRUE(view.data() == floats.as<const float *>());

    //complex elements
    Pothos::BufferChunk cplx(Pothos::DType(typeid(std::complex<float>)), 4);
    POTHOS_TEST_EQUAL(cplx.asSpan<std::complex<float>>().size(), 4u);

    //an empty chunk gives an empty span
    const Pothos::BufferChunk empty;
    POTHOS_TEST_TRUE(empty.asSpan<std::uint32_t>().empty());
}
