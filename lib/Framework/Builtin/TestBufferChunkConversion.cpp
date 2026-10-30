// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework/BufferChunk.hpp>
#include <concepts>
#include <cstddef>
#include <type_traits>

//The implicit conversion gives only pointers. A conversion to an integer
//made a BufferChunk look ordered (a < b through int), and the Qt 6 moc
//then failed on reinterpret_cast<int>(size_t) in each slot of the type.
static_assert(std::is_convertible_v<const Pothos::BufferChunk &, const float *>);
static_assert(std::is_convertible_v<const Pothos::BufferChunk &, void *>);
static_assert(not std::is_convertible_v<const Pothos::BufferChunk &, int>);
static_assert(not std::is_convertible_v<const Pothos::BufferChunk &, std::size_t>);
static_assert(not std::totally_ordered<Pothos::BufferChunk>);

POTHOS_TEST_BLOCK("/framework/tests", test_buffer_chunk_conversion)
{
    Pothos::BufferChunk chunk(Pothos::DType(typeid(float)), 4);
    const float *p = chunk;
    POTHOS_TEST_TRUE(p == chunk.as<const float *>());
    void *v = chunk;
    POTHOS_TEST_TRUE(v == chunk.as<void *>());
}
