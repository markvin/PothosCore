// Copyright (c) 2014-2017 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "FileUtils.hpp"

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Util/ExceptionForErrorCode.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <thread>

#include <iostream>

using json = nlohmann::json;

static void testBinaryFileBlocksCommon(
    const Pothos::Proxy& fileSource,
    const Pothos::Proxy& fileSink)
{
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");

    //create a test plan
    json testPlan;
    testPlan["enableBuffers"] = true;
    testPlan["minTrials"] = 100;
    testPlan["maxTrials"] = 200;
    testPlan["minSize"] = 512;
    testPlan["maxSize"] = 2048;
    auto expected = feeder.call("feedTestPlan", testPlan.dump());

    //run a topology that sends feeder to file
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, fileSink, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }

    //run a topology that sends file to collector
    {
        Pothos::Topology topology;
        topology.connect(fileSource, 0, collector, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }

    collector.call("verifyTestPlan", expected);
}

//an empty file with a unique name in the system temporary directory,
//removed by the destructor
struct TempFile
{
    TempFile(void)
    {
        static std::atomic<unsigned long long> counter(0);
        std::random_device rd;
        const auto dir = std::filesystem::temp_directory_path();
        do _path = dir / ("pothos-blocks-" + std::to_string(rd()) + "-" + std::to_string(counter++) + ".tmp");
        while (std::filesystem::exists(_path));
        std::ofstream(_path, std::ios::binary);
    }
    ~TempFile(void)
    {
        std::error_code ec;
        std::filesystem::remove(_path, ec);
    }
    TempFile(const TempFile &) = delete;
    TempFile &operator=(const TempFile &) = delete;
    const std::filesystem::path &path(void) const
    {
        return _path;
    }
private:
    std::filesystem::path _path;
};

static void testBinaryFileBlocks(bool optimizeSourceForStandardFile)
{
    TempFile tempFile;

    auto fileSource = Pothos::BlockRegistry::make(
                          "/blocks/binary_file_source",
                          "int",
                          optimizeSourceForStandardFile);
    fileSource.call("setAutoRewind", false);
    fileSource.call("setFilePath", tempFile.path().string());

    auto fileSink = Pothos::BlockRegistry::make("/blocks/binary_file_sink");
    fileSink.call("setFilePath", tempFile.path().string());

    testBinaryFileBlocksCommon(fileSource, fileSink);
}

POTHOS_TEST_BLOCK("/blocks/tests", test_binary_file_blocks)
{
    std::cout << "Testing with standard file optimization..." << std::endl;
    testBinaryFileBlocks(true);

    std::cout << "Testing without standard file optimization..." << std::endl;
    testBinaryFileBlocks(false);
}

POTHOS_TEST_BLOCK("/blocks/tests", test_binary_file_descriptor_blocks)
{
    TempFile tempFile;

    int readFD = openFileForRead(tempFile.path().string().c_str());
    if(readFD < 0) throw Pothos::Util::ErrnoException<Pothos::OpenFileException>();

    int writeFD = openFileForWrite(tempFile.path().string().c_str());
    if(writeFD < 0) throw Pothos::Util::ErrnoException<Pothos::OpenFileException>();

    auto fdSource = Pothos::BlockRegistry::make(
                        "/blocks/binary_filedescriptor_source",
                        "int");
    fdSource.call("setFileDescriptor", readFD);

    auto fdSink = Pothos::BlockRegistry::make("/blocks/binary_filedescriptor_sink");
    fdSink.call("setFileDescriptor", writeFD);

    testBinaryFileBlocksCommon(fdSource, fdSink);
}

POTHOS_TEST_BLOCK("/blocks/tests", test_circular_binary_file_source)
{
    // Generate some random input.
    constexpr size_t maxSize = 2 << 15;

    std::mt19937 rng(std::random_device{}());
    const size_t inputSize = std::uniform_int_distribution<size_t>(1, maxSize-1)(rng);

    Pothos::BufferChunk input("int", inputSize);

    std::uniform_int_distribution<int> randomInt;
    std::generate_n(input.as<int*>(), inputSize, [&]{return randomInt(rng);});

    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");

    TempFile tempFile;

    auto fileSource = Pothos::BlockRegistry::make(
                          "/blocks/binary_file_source",
                          "int",
                          true /*optimizeForStandardFile*/);
    fileSource.call("setFilePath", tempFile.path().string());
    fileSource.call("setAutoRewind", true);

    auto fileSink = Pothos::BlockRegistry::make("/blocks/binary_file_sink");
    fileSink.call("setFilePath", tempFile.path().string());

    feeder.call("feedBuffer", input);

    // Run a topology that sends feeder to file
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, fileSink, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }

    // Run a topology that repeatedly sends file to collector
    {
        Pothos::Topology topology;
        topology.connect(fileSource, 0, collector, 0);
        topology.commit();

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Check that the output is our file contents repeated
    auto output = collector.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_TRUE(output.elements() >= input.elements());

    const auto numRepeats = output.elements() / input.elements();
    const int* inputPtr = reinterpret_cast<const int*>(input.address);
    const int* outputPtr = reinterpret_cast<const int*>(output.address);

    for(size_t i = 0; i < numRepeats; ++i)
    {
        POTHOS_TEST_EQUALA(
            inputPtr,
            outputPtr,
            inputSize);

        outputPtr += inputSize;
    }
}
