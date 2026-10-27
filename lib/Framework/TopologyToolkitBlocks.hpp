// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote/Client.hpp>
#include <Pothos/Framework/Exception.hpp>
#include <format>
#include <string>
#include <utility>

//! Where an environment runs, for error messages: this process or a remote host
inline std::string toolkitBlockLocation(const Pothos::ProxyEnvironment::Sptr &env)
{
    if (env->getUniquePid() == Pothos::ProxyEnvironment::getLocalUniquePid()) return "in this process";
    const auto ip = Pothos::RemoteClient::lookupIpFromNodeId(env->getNodeId());
    return std::format("in the remote {} environment on {}", env->getName(),
        ip.empty()? "node "+env->getNodeId() : ip);
}

/*!
 * Make a block that the topology inserts on its own (a copier for a domain
 * crossing, network blocks between hosts). These blocks come from the blocks
 * modules of the core (blocks/, POTHOS_BUILD_BLOCKS): when the block is not
 * installed in the registry's environment, throw a TopologyConnectError
 * that says so instead of a bare plugin registry error.
 */
template <typename... ArgsType>
Pothos::Proxy makeToolkitBlock(const Pothos::Proxy &registry, const std::string &path, const std::string &purpose, ArgsType&&... args)
{
    const bool exists = registry.call("doesBlockExist", path);
    if (not exists) throw Pothos::TopologyConnectError("Pothos::Topology::commit()", std::format(
        "{} needs the block {} from the blocks modules, which are not installed {} "
        "(build the core with POTHOS_BUILD_BLOCKS=ON)",
        purpose, path, toolkitBlockLocation(registry.getEnvironment())));
    return registry.call(path, std::forward<ArgsType>(args)...);
}
