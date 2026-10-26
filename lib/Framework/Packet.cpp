// Copyright (c) 2014-2014 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework/Packet.hpp>

Pothos::Packet::Packet(void)
{
    return;
}

#include <Pothos/Managed.hpp>

static auto managedPacket = Pothos::ManagedClass()
    .registerConstructor<Pothos::Packet>()
    .registerField(POTHOS_FCN_TUPLE(Pothos::Packet, payload))
    .registerField(POTHOS_FCN_TUPLE(Pothos::Packet, metadata))
    .registerField(POTHOS_FCN_TUPLE(Pothos::Packet, labels))
    .commit("Pothos/Packet");

#include <Pothos/Object/Serialize.hpp>

namespace Pothos { namespace serialization {
template<class Archive>
void serialize(Archive & ar, Pothos::Packet &t, const unsigned int)
{
    ar & t.payload;
    ar & t.metadata;
    ar & t.labels;
}
}}

POTHOS_OBJECT_SERIALIZE(Pothos::Packet)

#include <Pothos/Object.hpp>
#include <Pothos/Plugin.hpp>
#include <format>

static std::string packetToString(const Pothos::Packet& packet)
{
    return std::format("Pothos::Packet (payload: {}, metadata: {}, labels: {})",
                        Pothos::Object(packet.payload).toString(),
                        Pothos::Object(packet.metadata).toString(),
                        Pothos::Object(packet.labels).toString());
}

pothos_static_block(pothosRegisterPothosPacketToString)
{
    Pothos::registerToStringFunc<Pothos::Packet>(
        "Pothos/Packet",
        &packetToString,
        false /*registerPointerTypes*/);
}
