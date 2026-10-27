// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Remote.hpp>
#include <Pothos/Util/SpinLockRW.hpp>
#include "Net/Address.hpp"
#include "Net/TcpStream.hpp"
#include "Net/Uri.hpp"
#include <chrono>
#include <mutex>
#include <map>
#include <cassert>

/***********************************************************************
 * lookupIpFromNodeId implementation
 **********************************************************************/
static Pothos::Util::SpinLockRW &getLookupMutex(void)
{
    static Pothos::Util::SpinLockRW lock;
    return lock;
}

static std::map<std::string, std::string> &getNodeIdTable(void)
{
    static std::map<std::string, std::string> map;
    return map;
}

std::string Pothos::RemoteClient::lookupIpFromNodeId(const std::string nodeId)
{
    Pothos::Util::SpinLockRW::SharedLock lock(getLookupMutex());
    auto it = getNodeIdTable().find(nodeId);
    if (it == getNodeIdTable().end()) return "";
    return it->second;
}

static void updateNodeIdTable(const Pothos::ProxyEnvironment::Sptr &env, const std::string &ipAddr)
{
    std::lock_guard<Pothos::Util::SpinLockRW> lock(getLookupMutex());
    getNodeIdTable()[env->getNodeId()] = ipAddr;
}

/***********************************************************************
 * RemoteClient implementation
 **********************************************************************/
struct Pothos::RemoteClient::Impl
{
    Impl(const std::string &uriStr, const long timeoutUs):
        uriStr(uriStr)
    {
        try
        {
            const auto uri = Pothos::Net::parseUri(uriStr);
            if (uri.scheme != "tcp") throw InvalidArgumentException("unsupported URI scheme", uri.scheme);

            //for unspecified port -- use the default locator port
            const auto port = (uri.port == 0)? std::uint16_t(std::stoi(RemoteServer::getLocatorPort())) : uri.port;
            stream = Pothos::Net::TcpStream::connect(uri.host, port, std::chrono::microseconds(timeoutUs));
            peerHost = stream->peerHost();
        }
        catch (const Exception &ex)
        {
            throw RemoteClientError("Pothos::RemoteClient("+uriStr+")", ex.displayText());
        }
    }
    std::unique_ptr<Pothos::Net::TcpStream> stream;
    const std::string uriStr;
    std::string peerHost;
};

Pothos::RemoteClient::RemoteClient(void)
{
    assert(not *this);
}

Pothos::RemoteClient::RemoteClient(const std::string &uri, const long timeoutUs):
    _impl(new Impl(uri, timeoutUs))
{
    return;
}

const std::string &Pothos::RemoteClient::getUri(void) const
{
    assert(_impl);
    return _impl->uriStr;
}

Pothos::RemoteClient::operator bool(void) const
{
    return bool(_impl);
}

std::iostream &Pothos::RemoteClient::getIoStream(void) const
{
    assert(_impl);
    return *_impl->stream;
}

Pothos::ProxyEnvironment::Sptr Pothos::RemoteClient::makeEnvironment(const std::string &name, const ProxyEnvironmentArgs &args)
{
    assert(_impl);
    auto env = RemoteClient::makeEnvironment(this->getIoStream(), name, args);
    env->holdRef(Object(*this));
    updateNodeIdTable(env, _impl->peerHost); //update the node id table with this remote host
    return env;
}

#include <Pothos/Managed.hpp>
#include <chrono>

static auto managedRemoteClient = Pothos::ManagedClass()
    .registerConstructor<Pothos::RemoteClient>()
    .registerConstructor<Pothos::RemoteClient, std::string>()
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::RemoteClient, getIoStream))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::RemoteClient, makeEnvironment))
    .registerStaticMethod<Pothos::ProxyEnvironment::Sptr, std::iostream &, const std::string &, const Pothos::ProxyEnvironmentArgs &>(POTHOS_FCN_TUPLE(Pothos::RemoteClient, makeEnvironment))
    .registerStaticMethod<Pothos::ProxyEnvironment::Sptr, std::istream &, std::ostream &, const std::string &, const Pothos::ProxyEnvironmentArgs &>(POTHOS_FCN_TUPLE(Pothos::RemoteClient, makeEnvironment))
    .commit("Pothos/RemoteClient");
