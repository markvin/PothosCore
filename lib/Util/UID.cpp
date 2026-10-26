// Copyright (c) 2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Util/UID.hpp>
#include "Platform/Environment.hpp"
#include <format>
#include <atomic>

static std::atomic<unsigned long long> count;

Pothos::Util::UID::UID(void):
    _uid(std::format(
        "pothos://{}/{}/{}/{}",
        Pothos::Platform::nodeName(),
        Pothos::Platform::nodeId(),
        Pothos::Platform::currentProcessId(),
        count.fetch_add(1, std::memory_order_relaxed)))
{
    return;
}

#include <Pothos/Managed.hpp>

static auto managedUID = Pothos::ManagedClass()
    .registerConstructor<Pothos::Util::UID>()
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::Util::UID, uid))
    .commit("Pothos/Util/UID");
