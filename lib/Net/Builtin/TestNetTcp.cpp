// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include "Net/TcpStream.hpp"
#include "Net/TcpServer.hpp"
#include "Net/Address.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace Pothos::Net;
using namespace std::chrono_literals;

namespace {

//! Answers every line with the same line
void echoLines(TcpStream &s)
{
    std::string line;
    while (std::getline(s, line)) s << line << std::endl;
}

//! A port on which nothing listens (bound once, then released)
std::uint16_t closedPort(void)
{
    TcpServer server("127.0.0.1", 0, [](TcpStream &){});
    return server.port();
}

} //namespace

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_echo)
{
    std::string peer;
    std::mutex mutex;
    TcpServer server("127.0.0.1", 0, [&](TcpStream &s)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            peer = s.peerHost();
        }
        echoLines(s);
    });
    POTHOS_TEST_TRUE(server.port() != 0);
    POTHOS_TEST_EQUAL(server.host(), "127.0.0.1");

    auto client = TcpStream::connect("127.0.0.1", server.port(), 1s);
    for (const std::string msg : {"hello", "", "world with spaces"})
    {
        *client << msg << std::endl;
        std::string reply;
        POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));
        POTHOS_TEST_EQUAL(reply, msg);
    }
    std::lock_guard<std::mutex> lock(mutex);
    POTHOS_TEST_EQUAL(peer, "127.0.0.1");
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_large_payload)
{
    //the handler reads a size, then the bytes, and sends them back reversed
    TcpServer server("127.0.0.1", 0, [](TcpStream &s)
    {
        std::size_t size = 0;
        s.read(reinterpret_cast<char *>(&size), sizeof(size));
        std::string data(size, '\0');
        s.read(data.data(), std::streamsize(size));
        if (not s) return;
        std::string reversed(data.rbegin(), data.rend());
        s.write(reversed.data(), std::streamsize(reversed.size()));
        s.flush();
    });

    std::string data(16*1024*1024, '\0');
    std::mt19937 rng(42);
    for (auto &ch : data) ch = char(rng());

    auto client = TcpStream::connect("127.0.0.1", server.port(), 1s);
    const std::size_t size = data.size();
    client->write(reinterpret_cast<const char *>(&size), sizeof(size));
    client->write(data.data(), std::streamsize(data.size()));
    client->flush();
    std::string back(size, '\0');
    client->read(back.data(), std::streamsize(size));
    POTHOS_TEST_TRUE(bool(*client));
    POTHOS_TEST_TRUE(back == std::string(data.rbegin(), data.rend()));
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_dual_stack)
{
    const std::string host = ipv6Supported()? "::" : "0.0.0.0";
    TcpServer server(host, 0, echoLines);
    std::vector<std::string> clients{"127.0.0.1"};
    if (ipv6Supported()) clients.push_back("::1");
    for (const auto &addr : clients)
    {
        auto client = TcpStream::connect(addr, server.port(), 1s);
        *client << addr << std::endl;
        std::string reply;
        POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));
        POTHOS_TEST_EQUAL(reply, addr);
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_concurrent)
{
    std::atomic<std::size_t> maxActive(0), lastActive(99);
    std::mutex countMutex;
    TcpServer server("127.0.0.1", 0, echoLines, [&](const std::size_t active)
    {
        std::lock_guard<std::mutex> lock(countMutex);
        if (active > maxActive) maxActive = active;
        lastActive = active;
    });

    std::atomic<int> failures(0);
    std::vector<std::thread> threads;
    for (int t = 0; t < 16; t++)
    {
        threads.emplace_back([&, t]
        {
            try
            {
                auto client = TcpStream::connect("127.0.0.1", server.port(), 2s);
                for (int i = 0; i < 100; i++)
                {
                    const auto msg = std::to_string(t) + ":" + std::to_string(i);
                    *client << msg << std::endl;
                    std::string reply;
                    if (not std::getline(*client, reply) or reply != msg) failures++;
                }
            }
            catch (...) {failures++;}
        });
    }
    for (auto &thread : threads) thread.join();
    POTHOS_TEST_EQUAL(failures.load(), 0);

    //the handlers finish after the clients disconnect
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (lastActive != 0 and std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(10ms);
    POTHOS_TEST_TRUE(maxActive >= 2);
    POTHOS_TEST_EQUAL(lastActive.load(), 0);
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_server_peer_abort)
{
    std::atomic<int> finished(0);
    TcpServer server("127.0.0.1", 0, [&](TcpStream &s)
    {
        echoLines(s);
        finished++;
    });
    {
        auto client = TcpStream::connect("127.0.0.1", server.port(), 1s);
        *client << "half a li" << std::flush; //no newline, then the client goes away
    }
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (finished == 0 and std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(10ms);
    POTHOS_TEST_EQUAL(finished.load(), 1);

    auto client = TcpStream::connect("127.0.0.1", server.port(), 1s);
    *client << "next" << std::endl;
    std::string reply;
    POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));
    POTHOS_TEST_EQUAL(reply, "next");
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_server_shutdown)
{
    std::optional<TcpServer> server(std::in_place, "127.0.0.1", 0, echoLines); //the handler blocks in getline
    auto client = TcpStream::connect("127.0.0.1", server->port(), 1s);
    *client << "ping" << std::endl;
    std::string reply;
    POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));

    const auto start = std::chrono::steady_clock::now();
    server.reset();
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 2s);

    //the server side closed: the client sees end of stream
    POTHOS_TEST_TRUE(not std::getline(*client, reply));
}

POTHOS_TEST_BLOCK("/net/tests", test_net_connect_timeout)
{
    //refused: fails fast with an IO error
    const auto port = closedPort();
    auto start = std::chrono::steady_clock::now();
    POTHOS_TEST_THROWS(TcpStream::connect("127.0.0.1", port, 2s), Pothos::IOException);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 1s);

    //unroutable: fails no later than the timeout (+ slack), timeout or network error
    start = std::chrono::steady_clock::now();
    POTHOS_TEST_THROWS(TcpStream::connect("10.255.255.1", 9, 200ms), Pothos::Exception);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 1200ms);
}
