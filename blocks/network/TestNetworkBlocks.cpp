// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Util/Network.hpp>
#include <asio/connect.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/write.hpp>
#include <chrono>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

//the network blocks are quiet while they wait for flow control: a longer
//idle time than the default, or a slow runner looks done before the last data
static const double NetworkIdle = 0.5;
static const double NetworkIdleTimeout = 20.0;

static void network_test_harness(const std::string &scheme, const bool serverIsSource,
    const std::string &bindHost = Pothos::Util::getWildcardAddr(),
    const std::function<std::string(const std::string &)> &connectAddr = [](const std::string &port){return Pothos::Util::getLoopbackAddr(port);})
{
    std::cout << std::format("network_test_harness: {}:// (serverIsSource? {})",
        scheme, serverIsSource?"true":"false") << std::endl;

    //create server
    auto server_uri = std::format("{}://{}", scheme, bindHost);
    std::cout << "make server " << server_uri << std::endl;
    auto server = Pothos::BlockRegistry::make(
        (serverIsSource)?"/blocks/network_source":"/blocks/network_sink",
        server_uri, "BIND");

    //create client
    std::string client_uri = std::format("{}://{}", scheme, connectAddr(server.call("getActualPort")));
    std::cout << "make client " << client_uri << std::endl;
    auto client = Pothos::BlockRegistry::make(
        (serverIsSource)?"/blocks/network_sink":"/blocks/network_source",
        client_uri, "CONNECT");

    //who is the source/sink?
    auto source = (serverIsSource)? server : client;
    auto sink = (serverIsSource)? client : server;

    //tester blocks
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");

    //create tester topology -- tests for open close
    std::cout << "Open/close repeat test" << std::endl;
    for (size_t i = 0; i < 3; i++)
    {
        Pothos::Topology topology;
        topology.connect(source, 0, collector, 0);
        topology.connect(feeder, 0, sink, 0);
        topology.commit();
    }

    //create tester topology
    Pothos::Topology topology;
    topology.connect(source, 0, collector, 0);
    topology.connect(feeder, 0, sink, 0);

    //create the testplan with large and numerous payloads
    json testPlan;
    testPlan["enableLabels"] = true;
    testPlan["enableMessages"] = true;
    testPlan["minTrials"] = 100;
    testPlan["maxTrials"] = 200;
    testPlan["minSize"] = 512;
    testPlan["maxSize"] = 1048*8;

    //test buffers with labels and messages
    std::cout << "Buffer based test" << std::endl;
    testPlan["enablePackets"] = false;
    testPlan["enableBuffers"] = true;
    auto expected = feeder.call("feedTestPlan", testPlan.dump());
    topology.commit();
    POTHOS_TEST_TRUE(topology.waitInactive(NetworkIdle, NetworkIdleTimeout));
    collector.call("verifyTestPlan", expected);

    //test packets with labels and messages
    std::cout << "Packet based test" << std::endl;
    testPlan["enablePackets"] = true;
    testPlan["enableBuffers"] = false;
    expected = feeder.call("feedTestPlan", testPlan.dump());
    topology.commit();
    POTHOS_TEST_TRUE(topology.waitInactive(NetworkIdle, NetworkIdleTimeout));
    collector.call("verifyTestPlan", expected);

    std::cout << "Done!\n" << std::endl;
}

POTHOS_TEST_BLOCK("/blocks/tests", test_network_blocks)
{
    network_test_harness("tcp", true);
    network_test_harness("tcp", false);
}

POTHOS_TEST_BLOCK("/blocks/tests", test_network_blocks_ipv6)
{
    if (not Pothos::Util::isIPv6Supported())
    {
        std::cout << "IPv6 is not supported on this host, skipped" << std::endl;
        return;
    }
    const auto v6Loopback = [](const std::string &port){return "[::1]:"+port;};
    network_test_harness("tcp", true, "[::1]", v6Loopback);
    network_test_harness("tcp", false, "[::1]", v6Loopback);
}

/***********************************************************************
 * A peer that connects and then stalls: commit() fails (no handshake)
 * within its timeouts instead of waiting for the peer forever
 **********************************************************************/
static bool finishesWithin(const std::chrono::seconds limit, std::function<void(void)> fcn)
{
    //the thread owns everything it uses: it may outlive this call on failure
    auto done = std::make_shared<std::promise<void>>();
    auto result = done->get_future();
    std::thread([fcn = std::move(fcn), done]
    {
        try {fcn();} catch (...) {}
        done->set_value();
    }).detach();
    return result.wait_for(limit) == std::future_status::ready;
}

static void stalledPeerTest(const std::string &bytesBeforeStall)
{
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto sink = Pothos::BlockRegistry::make("/blocks/network_sink", "tcp://127.0.0.1", "BIND");
    const std::string port = sink.call("getActualPort");

    //a raw client that never answers the handshake
    auto context = std::make_shared<asio::io_context>();
    auto peer = std::make_shared<asio::ip::tcp::socket>(*context);
    peer->connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), std::uint16_t(std::stoi(port))));
    if (not bytesBeforeStall.empty()) asio::write(*peer, asio::buffer(bytesBeforeStall));

    auto threw = std::make_shared<bool>(false);
    POTHOS_TEST_TRUE(finishesWithin(std::chrono::seconds(15), [=]
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, sink, 0);
        try {topology.commit();}
        catch (const Pothos::Exception &ex)
        {
            std::cout << "commit: " << ex.displayText() << std::endl;
            *threw = true;
        }
        (void)context; (void)peer; //keep the peer open while committing
    }));
    POTHOS_TEST_TRUE(*threw);
}

POTHOS_TEST_BLOCK("/blocks/tests", test_network_activate_stalled_peer)
{
    std::cout << "silent peer" << std::endl;
    stalledPeerTest("");
    std::cout << "peer stalls inside a header" << std::endl;
    stalledPeerTest("PTH");
}

/***********************************************************************
 * The receiving side stops reading during a transfer: the sending
 * topology still shuts down (the stalled send ends with an error)
 **********************************************************************/
namespace
{
    struct NeverConsumes : Pothos::Block
    {
        NeverConsumes(void)
        {
            this->setupInput(0, "int");
        }

        void work(void) {}
    };
}

POTHOS_TEST_BLOCK("/blocks/tests", test_network_stalled_reader)
{
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto sink = Pothos::BlockRegistry::make("/blocks/network_sink", "tcp://127.0.0.1", "BIND");
    const std::string port = sink.call("getActualPort");
    auto source = Pothos::BlockRegistry::make("/blocks/network_source", "tcp://127.0.0.1:"+port, "CONNECT");
    auto stuck = std::make_shared<NeverConsumes>();

    //one buffer far larger than the socket buffers of the loopback
    feeder.call("feedBuffer", Pothos::BufferChunk("int", 8*1024*1024));

    POTHOS_TEST_TRUE(finishesWithin(std::chrono::seconds(30), [=]
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, sink, 0);
        topology.connect(source, 0, stuck, 0);
        topology.commit();
        std::this_thread::sleep_for(std::chrono::seconds(1)); //the send blocks by now
    }));
}

/***********************************************************************
 * Datagram IO: a stream through UDP on the loopback
 **********************************************************************/
static std::string freeUdpPort(void)
{
    asio::io_context context;
    asio::ip::udp::socket sock(context, asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    return std::to_string(sock.local_endpoint().port());
}

POTHOS_TEST_BLOCK("/blocks/tests", test_datagram_io)
{
    const auto port = freeUdpPort();
    auto receiver = Pothos::BlockRegistry::make("/blocks/datagram_io", "int");
    receiver.call("setupSocket", "udp://127.0.0.1:"+port, "BIND");
    auto sender = Pothos::BlockRegistry::make("/blocks/datagram_io", "int");
    sender.call("setupSocket", "udp://127.0.0.1:"+port, "CONNECT");

    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");
    Pothos::BufferChunk input("int", 1000);
    for (size_t i = 0; i < input.elements(); i++) input.as<int *>()[i] = int(i*3);
    feeder.call("feedBuffer", input);

    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, sender, 0);
        topology.connect(receiver, 0, collector, 0);
        topology.commit();
        //the datagram blocks never go idle: wait for the data instead
        const auto exitTime = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (collector.call<Pothos::BufferChunk>("getBuffer").elements() < input.elements() and
            std::chrono::steady_clock::now() < exitTime)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    const auto output = collector.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(output.elements(), input.elements());
    POTHOS_TEST_EQUALA(output.as<const int *>(), input.as<const int *>(), input.elements());
}
