// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Proxy/Environment.hpp>
#include <Pothos/Proxy/Handle.hpp>
#include <Pothos/Proxy/Exception.hpp>
#include <shared_mutex>
#include <Pothos/Util/TypeInfo.hpp>
#include <Pothos/Callable.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/System/Logger.hpp>
#include <functional>
#include <iostream>
#include <cassert>
#include <mutex>
#include <map>

/***********************************************************************
 * Global map structure for conversions
 **********************************************************************/
static std::shared_mutex &getMapToLocalMutex(void)
{
    static std::shared_mutex lock;
    return lock;
}

static std::shared_mutex &getMapToProxyMutex(void)
{
    static std::shared_mutex lock;
    return lock;
}

typedef std::map<size_t, Pothos::Plugin> ConvertMapType;

static ConvertMapType &getConvertToLocalMap(void)
{
    static ConvertMapType map;
    return map;
}

static ConvertMapType &getConvertToProxyMap(void)
{
    static ConvertMapType map;
    return map;
}

/***********************************************************************
 * hash functions
 **********************************************************************/
static size_t hashIt(const std::string &name, const std::type_info &type)
{
    return type.hash_code() ^ std::hash<std::string>{}(name);
}

static size_t hashIt(const std::string &name, const std::string &type)
{
    return std::hash<std::string>{}(type) ^ std::hash<std::string>{}(name);
}

/***********************************************************************
 * Validate registry entries
 **********************************************************************/
static bool isConvertToLocal(const Pothos::Plugin &plugin)
{
    if (plugin.getObject().type() != typeid(Pothos::ProxyConvertPair)) return false;
    const auto &pair = plugin.getObject().extract<Pothos::ProxyConvertPair>();
    const auto &callable = pair.second;
    if (callable.getNumArgs() != 1) return false;
    if (callable.type(0) != typeid(Pothos::Proxy)) return false;
    return true;
}

static bool isConvertToProxy(const Pothos::Plugin &plugin)
{
    if (plugin.getObject().type() != typeid(Pothos::Callable)) return false;
    const auto &callable = plugin.getObject().extract<Pothos::Callable>();
    if (callable.getNumArgs() != 2) return false;
    if (callable.type(-1) != typeid(Pothos::Proxy)) return false;
    if (callable.type(0) != typeid(Pothos::ProxyEnvironment::Sptr)) return false;
    return true;
}

/***********************************************************************
 * Conversion registration handling
 **********************************************************************/
static void handlePluginEvent(const Pothos::Plugin &plugin, const std::string &event)
{
    POTHOS_LOG_DEBUG("Pothos.Proxy.handlePluginEvent", "plugin {}, event {}", plugin.toString(), event);

    const auto &nodes = plugin.getPath().listNodes();
    assert(nodes.size() > 2);
    assert(nodes[0] == "proxy");
    assert(nodes[1] == "converters");
    const auto &name = nodes[2];

    POTHOS_EXCEPTION_TRY
    {
        if (isConvertToLocal(plugin))
        {
            const auto &pair = plugin.getObject().extract<Pothos::ProxyConvertPair>();
            std::lock_guard<std::shared_mutex> lock(getMapToLocalMutex());
            if (event == "add")
            {
                getConvertToLocalMap()[hashIt(name, pair.first)] = plugin;
            }
            if (event == "remove")
            {
                getConvertToLocalMap()[hashIt(name, pair.first)] = Pothos::Plugin();
            }
        }
        else if (isConvertToProxy(plugin))
        {
            const auto &callable = plugin.getObject().extract<Pothos::Callable>();
            std::lock_guard<std::shared_mutex> lock(getMapToProxyMutex());
            if (event == "add")
            {
                getConvertToProxyMap()[hashIt(name, callable.type(1))] = plugin;
            }
            if (event == "remove")
            {
                getConvertToProxyMap()[hashIt(name, callable.type(1))] = Pothos::Plugin();
            }
        }
        else
        {
            throw Pothos::Exception("unknown format for plugin");
        }
    }
    POTHOS_EXCEPTION_CATCH(const Pothos::Exception &ex)
    {
        POTHOS_LOG_ERROR("Pothos.Proxy.handlePluginEvent", "exception {}, plugin {}, event {}", ex.displayText(), plugin.toString(), event);
    }
}

/***********************************************************************
 * Register event handler
 **********************************************************************/
pothos_static_block(pothosProxyConvertRegister)
{
    Pothos::PluginRegistry::addCall("/proxy/converters", &handlePluginEvent);
}

/***********************************************************************
 * The conversion implementation
 **********************************************************************/
Pothos::Proxy Pothos::ProxyEnvironment::convertObjectToProxy(const Pothos::Object &local)
{
    //find the plugin in the map, it will be null if not found
    std::shared_lock<std::shared_mutex> lock(getMapToProxyMutex());
    const size_t h = hashIt(this->getName(), local.type());
    auto it = getConvertToProxyMap().find(h);

    //thow an error when the conversion is not supported
    if (it == getConvertToProxyMap().end()) throw Pothos::ProxyEnvironmentConvertError(
        "Pothos::ProxyEnvironment::convertObjectToProxy()",
        std::format("doesnt support Object of type {} to {} environment",
        local.getTypeString(), this->getName()));

    const auto &callable = it->second.getObject().extract<Pothos::Callable>();
    Pothos::Object args[2];
    args[0] = Pothos::Object(this->shared_from_this());
    args[1] = local;
    return std::move(callable.opaqueCall(args, 2).ref<Pothos::Proxy>());
}

Pothos::Object Pothos::ProxyEnvironment::convertProxyToObject(const Pothos::Proxy &proxy_)
{
    //make sure proxy is in this environment
    Pothos::Proxy proxy = proxy_;
    if (proxy.getEnvironment() != this->shared_from_this())
    {
        proxy = this->convertObjectToProxy(proxy.toObject());
    }

    //find the plugin in the map, it will be null if not found
    std::shared_lock<std::shared_mutex> lock(getMapToLocalMutex());
    const size_t h = hashIt(this->getName(), proxy.getHandle()->getClassName());
    auto it = getConvertToLocalMap().find(h);

    //thow an error when the conversion is not supported
    if (it == getConvertToLocalMap().end()) throw Pothos::ProxyEnvironmentConvertError(
        "Pothos::ProxyEnvironment::convertProxyToObject()",
        std::format("doesnt support environment {} type {} to Object",
        this->getName(), std::string(proxy.getHandle()->getClassName())));

    const auto &callable = it->second.getObject().extract<Pothos::ProxyConvertPair>().second;
    Pothos::Object args[1];
    args[0] = Pothos::Object(proxy);
    return callable.opaqueCall(args, 1);
}
