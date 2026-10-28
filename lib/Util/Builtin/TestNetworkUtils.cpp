// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Util/Network.hpp>
#include <chrono>
#include <string>

POTHOS_TEST_BLOCK("/util/tests", test_util_parse_uri)
{
    const auto uri = Pothos::Util::parseUri("tcp://[::1]:1234");
    POTHOS_TEST_EQUAL(uri.scheme, "tcp");
    POTHOS_TEST_EQUAL(uri.host, "::1");
    POTHOS_TEST_EQUAL(uri.port, 1234);
    POTHOS_TEST_EQUAL(Pothos::Util::parseUri("udp://localhost").port, 0);
    POTHOS_TEST_EQUAL(Pothos::Util::toString(uri), "tcp://[::1]:1234");

    for (const auto bad : {"localhost:80", "tcp://", "tcp://h:99999", "tcp://h:80/path", "tcp://::1:80"})
    {
        POTHOS_TEST_THROWS(Pothos::Util::parseUri(bad), Pothos::InvalidArgumentException);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_util_resolve_host)
{
    const auto timeout = std::chrono::seconds(5);
    POTHOS_TEST_EQUAL(Pothos::Util::resolveHost("127.0.0.1", timeout), "127.0.0.1");
    POTHOS_TEST_TRUE(not Pothos::Util::resolveHost("localhost", timeout).empty());

    //an unknown name fails: a resolver error, or a timeout on a slow resolver
    POTHOS_TEST_THROWS(Pothos::Util::resolveHost("no-such-host.invalid", timeout), Pothos::Exception);
}
