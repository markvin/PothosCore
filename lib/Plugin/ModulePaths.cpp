// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin/Module.hpp>
#include <Pothos/Plugin/Plugin.hpp>
#include <shared_mutex>
#include <unordered_map>
#include <mutex>
#include <algorithm>

static std::shared_mutex &getModulePathsMutex(void)
{
    static std::shared_mutex lock;
    return lock;
}

typedef std::unordered_map<std::string, std::vector<std::string>> ModulePathsMap;
static ModulePathsMap &getModulePathsMap(void)
{
    static ModulePathsMap map;
    return map;
}

std::vector<std::string> getPluginPaths(const Pothos::PluginModule &module)
{
    std::shared_lock<std::shared_mutex> lock(getModulePathsMutex());
    const auto &map = getModulePathsMap();
    const auto it = map.find(module.getFilePath());
    if (it == map.end()) return {};
    return it->second;
}

void updatePluginAssociation(const std::string &action, const Pothos::Plugin &plugin)
{
    std::lock_guard<std::shared_mutex> lock(getModulePathsMutex());
    auto &v = getModulePathsMap()[plugin.getModule().getFilePath()];
    const auto &path = plugin.getPath().toString();
    if (action == "add")
    {
        v.push_back(path);
    }
    if (action == "remove")
    {
        auto it = std::find(v.begin(), v.end(), path);
        if (it != v.end()) v.erase(it);
    }
}
