// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Managed/Class.hpp>
#include <Pothos/Managed/Exception.hpp>
#include <shared_mutex>
#include <Pothos/Util/TypeInfo.hpp>
#include <Pothos/Callable.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/System/Logger.hpp>
#include <mutex>
#include <map>

/***********************************************************************
 * Global map structure for registry
 **********************************************************************/
static std::shared_mutex &getMapMutex(void)
{
    static std::shared_mutex lock;
    return lock;
}

//singleton global map for all supported classes
typedef std::map<size_t, Pothos::Plugin> ClassMapType;
static ClassMapType &getClassMap(void)
{
    static ClassMapType map;
    return map;
}

/***********************************************************************
 * Conversion registration handling
 **********************************************************************/
static void handlePluginEvent(const Pothos::Plugin &plugin, const std::string &event)
{
    POTHOS_LOG_DEBUG("Pothos.ManagedClass.handlePluginEvent", "plugin {}, event {}", plugin.toString(), event);
    POTHOS_EXCEPTION_TRY
    {
        //validate the plugin -- if we want to handle it -- check the signature:
        if (plugin.getObject().type() != typeid(Pothos::ManagedClass)) return;
        const auto &reg = plugin.getObject().extract<Pothos::ManagedClass>();

        std::lock_guard<std::shared_mutex> lock(getMapMutex());
        if (event == "add")
        {
            getClassMap()[reg.type().hash_code()] = plugin;
            getClassMap()[reg.pointerType().hash_code()] = plugin;
            getClassMap()[reg.sharedType().hash_code()] = plugin;
        }
        if (event == "remove")
        {
            getClassMap()[reg.type().hash_code()] = Pothos::Plugin();
            getClassMap()[reg.pointerType().hash_code()] = Pothos::Plugin();
            getClassMap()[reg.sharedType().hash_code()] = Pothos::Plugin();
        }
    }
    POTHOS_EXCEPTION_CATCH(const Pothos::Exception &ex)
    {
        POTHOS_LOG_ERROR("Pothos.ManagedClass.handlePluginEvent", "exception {}, plugin {}, event {}", ex.displayText(), plugin.toString(), event);
    }
}

/***********************************************************************
 * Register event handler
 **********************************************************************/
pothos_static_block(pothosManagedClassRegister)
{
    Pothos::PluginRegistry::addCall("/managed", &handlePluginEvent);
}

/***********************************************************************
 * The lookup implementation
 **********************************************************************/
Pothos::ManagedClass Pothos::ManagedClass::lookup(const std::type_info &type)
{
    //find the plugin in the map, it will be null if not found
    std::shared_lock<std::shared_mutex> lock(getMapMutex());
    auto it = getClassMap().find(type.hash_code());

    //thow an error when the entry is not found
    if (it == getClassMap().end()) throw ManagedClassLookupError(
        "Pothos::ManagedClass::lookup("+Util::typeInfoToString(type)+")",
        "no registration found");

    //extract the managed class
    return it->second.getObject().extract<Pothos::ManagedClass>();
}
