// Copyright (c) 2013-2020 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "TypesHashCombine.hpp"
#include <Pothos/Object/Object.hpp>
#include <Pothos/Object/Exception.hpp>
#include <shared_mutex>
#include <Pothos/Callable.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/Util/CompareTo.hpp>
#include <Pothos/System/Logger.hpp>
#include <format>
#include <mutex>
#include <map>

/***********************************************************************
 * Global map structure for comparisons
 **********************************************************************/
static std::shared_mutex &getMapMutex(void)
{
    static std::shared_mutex lock;
    return lock;
}

//singleton global map for all supported comparisons
typedef std::map<size_t, Pothos::Plugin> CompareMapType;
static CompareMapType &getCompareMap(void)
{
    static CompareMapType map;
    return map;
}

/***********************************************************************
 * Comparison registration handling
 **********************************************************************/
static void handleComparePluginEvent(const Pothos::Plugin &plugin, const std::string &event)
{
    POTHOS_LOG_DEBUG("Pothos.Object.handleComparePluginEvent", "plugin {}, event {}", plugin.toString(), event);
    POTHOS_EXCEPTION_TRY
    {
        //validate the plugin -- if we want to handle it -- check the signature:
        if (plugin.getObject().type() != typeid(Pothos::Callable)) return;
        const auto &call = plugin.getObject().extract<Pothos::Callable>();
        if (call.type(-1) != typeid(int)) return;
        if (call.getNumArgs() != 2) return;

        //extract type info used for map lookup
        const std::type_info &t0 = call.type(0);
        const std::type_info &t1 = call.type(1);

        std::lock_guard<std::shared_mutex> lock(getMapMutex());
        if (event == "add")
        {
            getCompareMap()[typesHashCombine(t0, t1)] = plugin;
        }
        if (event == "remove")
        {
            getCompareMap()[typesHashCombine(t0, t1)] = Pothos::Plugin();
        }
    }
    POTHOS_EXCEPTION_CATCH(const Pothos::Exception &ex)
    {
        POTHOS_LOG_ERROR("Pothos.Object.handleComparePluginEvent", "exception {}, plugin {}, event {}", ex.displayText(), plugin.toString(), event);
    }
}

/***********************************************************************
 * Register event handler
 **********************************************************************/
pothos_static_block(pothosObjectCompareRegister)
{
    Pothos::PluginRegistry::addCall("/object/compare", &handleComparePluginEvent);
}

/***********************************************************************
 * The compare implementation
 **********************************************************************/
int Pothos::Object::compareTo(const Pothos::Object &other) const
{
    //find the plugin in the map, it will be null if not found
    std::shared_lock<std::shared_mutex> lock(getMapMutex());
    auto it = getCompareMap().find(typesHashCombine(this->type(), other.type()));

    //try a number type just in the case that this is possible
    if (it == getCompareMap().end()) try
    {
        return Pothos::Util::compareTo(double(*this), double(other));
    }
    catch(const Pothos::ObjectConvertError &){}

    //thow an error when the compare is not supported
    if (it == getCompareMap().end()) throw Pothos::ObjectCompareError(
        "Pothos::Object::compareTo()",
        std::format("not supported for {}, {}",
        this->toString(), other.toString()));

    Object args[2];
    args[0] = *this;
    args[1] = other;
    const auto &call = it->second.getObject().extract<Pothos::Callable>();
    return call.opaqueCall(args, 2).extract<int>();
}
