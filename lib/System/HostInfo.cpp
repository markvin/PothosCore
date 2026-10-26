// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/HostInfo.hpp>
#include <Pothos/Managed.hpp>
#include <Pothos/Object/Serialize.hpp>
#include "Platform/Environment.hpp"

Pothos::System::HostInfo::HostInfo(void)
{
    return;
}

Pothos::System::HostInfo Pothos::System::HostInfo::get(void)
{
    HostInfo info;
    info.osName = Pothos::Platform::osName();
    info.osVersion = Pothos::Platform::osVersion();
    info.osArchitecture = Pothos::Platform::osArchitecture();
    info.nodeName = Pothos::Platform::nodeName();
    info.nodeId = Pothos::Platform::nodeId();
    info.processorCount = Pothos::Platform::processorCount();
    info.pid = std::to_string(Pothos::Platform::currentProcessId());
    return info;
}

#include <Pothos/Managed.hpp>
#include <Pothos/Object/Serialize.hpp>

static auto managedHostInfo = Pothos::ManagedClass()
    .registerConstructor<Pothos::System::HostInfo>()
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::System::HostInfo, get))
    .commit("Pothos/System/HostInfo");

namespace Pothos { namespace serialization {
template <class Archive>
void serialize(Archive &ar, Pothos::System::HostInfo &t, const unsigned int)
{
    ar & t.osName;
    ar & t.osVersion;
    ar & t.osArchitecture;
    ar & t.nodeName;
    ar & t.nodeId;
    ar & t.processorCount;
    ar & t.pid;
}
}}

POTHOS_OBJECT_SERIALIZE(Pothos::System::HostInfo)
