// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include "Net/TcpStream.hpp"
#include "Net/TcpServer.hpp"
#include "Net/Address.hpp"
#include <atomic>
#include <condition_variable>
#include <stdexcept>
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
    //refused: fails fast with an IO error; Windows retries a refused SYN for
    //about 2 s, so there the timeout may expire first
    const auto port = closedPort();
    auto start = std::chrono::steady_clock::now();
    #ifdef _WIN32
    POTHOS_TEST_THROWS(TcpStream::connect("127.0.0.1", port, 500ms), Pothos::Exception);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 1500ms);
    #else
    POTHOS_TEST_THROWS(TcpStream::connect("127.0.0.1", port, 2s), Pothos::IOException);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 1s);
    #endif

    //unroutable: fails no later than the timeout (+ slack), timeout or network error
    start = std::chrono::steady_clock::now();
    POTHOS_TEST_THROWS(TcpStream::connect("10.255.255.1", 9, 200ms), Pothos::Exception);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < 1200ms);
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_count_ordering)
{
    //the last count delivered after concurrent closes is always 0
    for (int iter = 0; iter < 30; iter++)
    {
        auto last = std::make_shared<std::atomic<std::size_t>>(99);
        TcpServer server("127.0.0.1", 0, echoLines, [last](const std::size_t active){*last = active;});
        std::vector<std::unique_ptr<TcpStream>> clients;
        for (int i = 0; i < 16; i++)
        {
            clients.push_back(TcpStream::connect("127.0.0.1", server.port(), 1s));
            *clients.back() << "x" << std::endl;
            std::string reply;
            std::getline(*clients.back(), reply);
        }
        std::vector<std::thread> closers;
        for (auto &client : clients) closers.emplace_back([&client]{client.reset();});
        for (auto &t : closers) t.join();
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        while (*last != 0 and std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(5ms);
        std::this_thread::sleep_for(20ms); //a stale late delivery would land here
        POTHOS_TEST_EQUAL(last->load(), 0);
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_localhost_to_ipv4_server)
{
    //"localhost" reaches a server bound to IPv4 only, whatever order the resolver returns
    TcpServer server("0.0.0.0", 0, echoLines);
    auto client = TcpStream::connect("localhost", server.port(), 2s);
    *client << "v4" << std::endl;
    std::string reply;
    POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));
    POTHOS_TEST_EQUAL(reply, "v4");
    POTHOS_TEST_TRUE(resolveHost("localhost", 1s).is_v4());
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_callback_exception)
{
    //an exception from a callback on the io_context thread does not end the process
    auto calls = std::make_shared<std::atomic<int>>(0);
    TcpServer server("127.0.0.1", 0, echoLines, [calls](const std::size_t)
    {
        if ((*calls)++ == 0) throw std::runtime_error("callback failure");
    });
    for (int i = 0; i < 2; i++)
    {
        auto client = TcpStream::connect("127.0.0.1", server.port(), 1s);
        *client << "still alive" << std::endl;
        std::string reply;
        POTHOS_TEST_TRUE(bool(std::getline(*client, reply)));
        POTHOS_TEST_EQUAL(reply, "still alive");
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_tcp_server_stuck_handler)
{
    //a handler stuck outside of socket I/O does not hang the server destructor forever
    struct Gate
    {
        std::mutex mutex;
        std::condition_variable cond;
        bool open = false, entered = false;
    };
    auto gate = std::make_shared<Gate>();
    auto server = std::make_unique<TcpServer>("127.0.0.1", 0, [gate](TcpStream &)
    {
        std::unique_lock<std::mutex> lock(gate->mutex);
        gate->entered = true;
        gate->cond.notify_all();
        gate->cond.wait(lock, [&]{return gate->open;});
    });
    auto client = TcpStream::connect("127.0.0.1", server->port(), 1s);
    {
        std::unique_lock<std::mutex> lock(gate->mutex);
        POTHOS_TEST_TRUE(gate->cond.wait_for(lock, 2s, [&]{return gate->entered;}));
    }
    const auto start = std::chrono::steady_clock::now();
    server.reset();
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < TcpServer::ShutdownGrace + 1s);

    //let the detached handler finish
    std::lock_guard<std::mutex> lock(gate->mutex);
    gate->open = true;
    gate->cond.notify_all();
}
