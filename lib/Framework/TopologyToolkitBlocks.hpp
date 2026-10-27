// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <Pothos/Proxy.hpp>
#include <Pothos/Framework/Exception.hpp>
#include <format>
#include <string>
#include <utility>

/*!
 * Make a block that the topology inserts on its own (a copier for a domain
 * crossing, network blocks between hosts). These blocks come from the blocks
 * toolkit (PothosBlocks), which is not part of the core: when the block is not
 * installed in the registry's environment, throw a TopologyConnectError
 * that says so instead of a bare plugin registry error.
 */
template <typename... ArgsType>
Pothos::Proxy makeToolkitBlock(const Pothos::Proxy &registry, const std::string &path, const std::string &purpose, ArgsType&&... args)
{
    const bool exists = registry.call("doesBlockExist", path);
    if (not exists) throw Pothos::TopologyConnectError("Pothos::Topology::commit()", std::format(
        "{} needs the block {} from the blocks toolkit (PothosBlocks), "
        "which is not installed in the {} environment", purpose, path, registry.getEnvironment()->getName()));
    return registry.call(path, std::forward<ArgsType>(args)...);
}
