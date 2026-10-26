// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include "Platform/Environment.hpp"
#include <optional>
#include <regex>
#include <string>

using namespace Pothos::Platform;

POTHOS_TEST_BLOCK("/platform/tests", test_env_roundtrip)
{
    const auto name = "POTHOS_TEST_ENV_" + std::to_string(currentProcessId());
    setEnv(name, "a b=c");
    POTHOS_TEST_TRUE(getEnv(name) == std::optional<std::string>("a b=c"));
    unsetEnv(name);
    POTHOS_TEST_TRUE(getEnv(name) == std::nullopt);
}

POTHOS_TEST_BLOCK("/platform/tests", test_env_missing)
{
    POTHOS_TEST_TRUE(getEnv("POTHOS_SURELY_UNSET_VARIABLE") == std::nullopt);
}

POTHOS_TEST_BLOCK("/platform/tests", test_node_id)
{
    //stable per host: every call (and process) gets the same value
    const auto id = nodeId();
    POTHOS_TEST_TRUE(not id.empty());
    POTHOS_TEST_EQUAL(id, nodeId());
    //a MAC address, or the host name when the machine has no network adapter
    const bool isMac = std::regex_match(id, std::regex("([0-9a-f]{2}:){5}[0-9a-f]{2}"));
    POTHOS_TEST_TRUE(isMac or id == nodeName());
}

POTHOS_TEST_BLOCK("/platform/tests", test_host_info)
{
    POTHOS_TEST_TRUE(not osName().empty());
    POTHOS_TEST_TRUE(not osVersion().empty());
    POTHOS_TEST_TRUE(not osArchitecture().empty());
    POTHOS_TEST_TRUE(not nodeName().empty());
    POTHOS_TEST_TRUE(processorCount() >= 1);
    POTHOS_TEST_TRUE(currentProcessId() > 0);
}
