// Copyright (c) 2013-2014 Josh Blum
//               2020,2023 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework/BufferChunk.hpp>
#include <Pothos/Framework/SharedBuffer.hpp>
#include <Pothos/Framework/Exception.hpp>
#include "Util/TempFile.hpp"
#include <filesystem>
#include <random>
#include <cstdlib> //rand
#include <fstream>

POTHOS_TEST_BLOCK("/framework/tests", test_generic_shared_buffer)
{
    auto b0 = Pothos::SharedBuffer::make(1024);
    POTHOS_TEST_NOT_EQUAL(b0.getAddress(), 0);
    POTHOS_TEST_TRUE((b0.getAddress() & 0xf) == 0); //has alignment
    POTHOS_TEST_EQUAL(b0.getLength(), 1024);
    for (size_t i = 0; i < b0.getLength()/sizeof(int); i++)
    {
        int *p = reinterpret_cast<int *>(b0.getAddress());
        const int randNum = std::rand();
        p[i] = randNum;
        POTHOS_TEST_EQUAL(p[i], randNum);
    }

    auto b1 = Pothos::SharedBuffer::make(2048, 0/*node*/); //numa node 0 should always exist
    POTHOS_TEST_NOT_EQUAL(b1.getAddress(), 0);
    POTHOS_TEST_TRUE((b1.getAddress() & 0xf) == 0); //has alignment
    POTHOS_TEST_EQUAL(b1.getLength(), 2048);
    for (size_t i = 0; i < b1.getLength()/sizeof(int); i++)
    {
        int *p = reinterpret_cast<int *>(b1.getAddress());
        const int randNum = std::rand();
        p[i] = randNum;
        POTHOS_TEST_EQUAL(p[i], randNum);
    }

    POTHOS_TEST_THROWS(
        Pothos::SharedBuffer(b1.getAddress() + 512, b1.getLength(), b1),
        Pothos::SharedBufferError);
    auto b2 = Pothos::SharedBuffer(b1.getAddress() + 512, b1.getLength() - 512, b1);

    //test a size zero allocation
    POTHOS_TEST_CHECKPOINT();
    auto bEmpty = Pothos::SharedBuffer::make(0);
    POTHOS_TEST_EQUAL(bEmpty.getLength(), 0);
}

POTHOS_TEST_BLOCK("/framework/tests", test_circular_shared_buffer)
{
    auto b0 = Pothos::SharedBuffer::makeCirc(1024);
    POTHOS_TEST_NOT_EQUAL(b0.getAddress(), 0);
    POTHOS_TEST_TRUE((b0.getAddress() & 0xf) == 0); //has alignment
    POTHOS_TEST_GE(b0.getLength(), 1024);

    const size_t alias = b0.getLength()/sizeof(int);
    for (size_t i = 0; i < b0.getLength()/sizeof(int); i++)
    {
        int *p = reinterpret_cast<int *>(b0.getAddress());
        const int randNum = std::rand();
        p[i] = randNum;
        POTHOS_TEST_EQUAL(p[i], randNum);
        POTHOS_TEST_EQUAL(p[i+alias], randNum);
    }
}

POTHOS_TEST_BLOCK("/framework/tests", test_mmap_shared_buffer)
{
    static constexpr size_t numElems = 1024;
    Pothos::BufferChunk input("int", numElems);

    std::independent_bits_engine<std::mt19937, 8, unsigned> randomBytes{std::random_device{}()};
    auto *inputBytes = reinterpret_cast<unsigned char*>(input.address);
    for (size_t i = 0; i < input.length; i++) inputBytes[i] = static_cast<unsigned char>(randomBytes());

    const Pothos::Util::Detail::TempFile tempFile;
    POTHOS_TEST_TRUE(std::filesystem::exists(tempFile.path()));

    std::ofstream ofile(tempFile.path(), std::ios::binary);
    ofile.write(reinterpret_cast<const char*>(input.address), input.length);
    ofile.close(); //flush the stream buffer to the file before checking its size
    POTHOS_TEST_TRUE(ofile.good());
    POTHOS_TEST_EQUAL(input.length, std::filesystem::file_size(tempFile.path()));

    int newValue = 0;

    {
        auto mmapSharedBuffer = Pothos::SharedBuffer::makeFromFileMMap(
            tempFile.path(),
            true,
            true);
        POTHOS_TEST_NOT_EQUAL(0, mmapSharedBuffer.getAddress());
        POTHOS_TEST_EQUAL(input.length, mmapSharedBuffer.getLength());

        // Make sure the shared buffer's contents match the input.
        auto *mmapIntBuff = reinterpret_cast<int*>(mmapSharedBuffer.getAddress());
        POTHOS_TEST_EQUALA(input.as<int*>(), mmapIntBuff, numElems);

        // This should write to the file.
        newValue = ++mmapIntBuff[0];
    }

    POTHOS_TEST_NOT_EQUAL(input.as<int*>()[0], newValue);

    std::ifstream ifile(tempFile.path(), std::ios::binary);
    Pothos::BufferChunk output("int", numElems);
    ifile.read(output.as<char*>(), output.length);
    POTHOS_TEST_EQUAL(newValue, output.as<int*>()[0]);
}
