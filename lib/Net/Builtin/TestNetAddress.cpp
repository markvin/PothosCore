// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Util/Network.hpp>
#include "Net/Uri.hpp"
#include "Net/Address.hpp"
#include <chrono>
#include <string>

using namespace Pothos::Net;

POTHOS_TEST_BLOCK("/net/tests", test_net_uri)
{
    auto uri = parseUri("tcp://localhost:16415");
    POTHOS_TEST_EQUAL(uri.scheme, "tcp");
    POTHOS_TEST_EQUAL(uri.host, "localhost");
    POTHOS_TEST_EQUAL(uri.port, 16415);

    uri = parseUri("tcp://[::1]:80");
    POTHOS_TEST_EQUAL(uri.host, "::1");
    POTHOS_TEST_EQUAL(uri.port, 80);

    uri = parseUri("tcp://[::]");
    POTHOS_TEST_EQUAL(uri.host, "::");
    POTHOS_TEST_EQUAL(uri.port, 0);

    uri = parseUri("udp://10.0.0.1:9/");
    POTHOS_TEST_EQUAL(uri.scheme, "udp");
    POTHOS_TEST_EQUAL(uri.host, "10.0.0.1");

    POTHOS_TEST_EQUAL(toString(Uri{"tcp", "::1", 80}), "tcp://[::1]:80");
    POTHOS_TEST_EQUAL(toString(Uri{"tcp", "10.0.0.1", 0}), "tcp://10.0.0.1");
    POTHOS_TEST_EQUAL(toString(parseUri("tcp://[::]:5")), "tcp://[::]:5");

    for (const auto bad : {"localhost:1", "tcp://", "tcp://::1:80", "tcp://h:70000", "tcp://h:x",
        "tcp://h:1/path", "tcp://[::1", "tcp://h:", "://h", "tcp://h:-1"})
    {
        POTHOS_TEST_THROWS(parseUri(bad), Pothos::InvalidArgumentException);
    }
}

POTHOS_TEST_BLOCK("/net/tests", test_net_host_port)
{
    auto hp = parseHostPort("[::1]:514", 1);
    POTHOS_TEST_EQUAL(hp.host, "::1");
    POTHOS_TEST_EQUAL(hp.port, 514);

    hp = parseHostPort("logs.local", 514);
    POTHOS_TEST_EQUAL(hp.host, "logs.local");
    POTHOS_TEST_EQUAL(hp.port, 514);

    hp = parseHostPort("1.2.3.4:9", 514);
    POTHOS_TEST_EQUAL(hp.host, "1.2.3.4");
    POTHOS_TEST_EQUAL(hp.port, 9);

    hp = parseHostPort("[fe80::1]", 7);
    POTHOS_TEST_EQUAL(hp.host, "fe80::1");
    POTHOS_TEST_EQUAL(hp.port, 7);

    for (const auto bad : {"", "::1:80", "h:70000", "h:x", "[::1", "h:"})
    {
        POTHOS_TEST_THROWS(parseHostPort(bad, 1), Pothos::InvalidArgumentException);
    }

    POTHOS_TEST_EQUAL(formatHostPort("::1", 5), "[::1]:5");
    POTHOS_TEST_EQUAL(formatHostPort("127.0.0.1", 5), "127.0.0.1:5");

    POTHOS_TEST_TRUE(isWildcard(""));
    POTHOS_TEST_TRUE(isWildcard("0.0.0.0"));
    POTHOS_TEST_TRUE(isWildcard("::"));
    POTHOS_TEST_TRUE(not isWildcard("::1"));
    POTHOS_TEST_EQUAL(loopbackFor("::"), "::1");
    POTHOS_TEST_EQUAL(loopbackFor("0.0.0.0"), "127.0.0.1");
}

POTHOS_TEST_BLOCK("/net/tests", test_net_resolve)
{
    const auto timeout = std::chrono::seconds(1);
    POTHOS_TEST_TRUE(resolveHost("127.0.0.1", timeout).is_loopback());
    POTHOS_TEST_TRUE(resolveHost("::1", timeout).is_loopback() or not ipv6Supported());
    POTHOS_TEST_TRUE(resolveHost("localhost", timeout).is_loopback());

    //an unknown name fails (resolver error or timeout) within the timeout + slack
    const auto start = std::chrono::steady_clock::now();
    POTHOS_TEST_THROWS(resolveHost("no-such-host.invalid", timeout), Pothos::Exception);
    POTHOS_TEST_TRUE(std::chrono::steady_clock::now() - start < timeout + std::chrono::seconds(1));
}

POTHOS_TEST_BLOCK("/net/tests", test_net_wildcard_addr)
{
    const std::string expected = ipv6Supported()? "[::]:1" : "0.0.0.0:1";
    POTHOS_TEST_EQUAL(Pothos::Util::getWildcardAddr("1"), expected);
    const std::string loopback = ipv6Supported()? "[::1]:1" : "127.0.0.1:1";
    POTHOS_TEST_EQUAL(Pothos::Util::getLoopbackAddr("1"), loopback);
}
