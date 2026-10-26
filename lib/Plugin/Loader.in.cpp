// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin/Loader.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Plugin.hpp>
#include <Poco/Logger.h>
#include <filesystem>
#include <system_error>
#include <future>

static std::vector<std::filesystem::path> getModulePaths(const std::filesystem::path &path)
{
    poco_debug(Poco::Logger::get("Pothos.PluginLoader.load"), path.string());

    std::vector<std::filesystem::path> paths;

    std::error_code ec;
    if (not std::filesystem::exists(path, ec)) return paths;
    else if (std::filesystem::is_regular_file(path, ec) and (path.extension() == ".@MODULE_EXT@"))
    {
        paths.push_back(path);
    }
    else if (std::filesystem::is_directory(path, ec))
    {
        for (const auto &entry : std::filesystem::directory_iterator(path, ec))
        {
            auto subpaths = getModulePaths(std::filesystem::absolute(entry.path()));
            paths.insert(paths.end(), subpaths.begin(), subpaths.end());
        }
    }

    return paths;
}

std::vector<Pothos::PluginModule> Pothos::PluginLoader::loadModules(void)
{
    const auto searchPaths = Pothos::System::getPothosModuleSearchPaths();

    //traverse the search paths and spawn futures
    std::vector<std::future<Pothos::PluginModule>> futures;
    for (const auto &searchPath : searchPaths)
    {
        for (const auto &path : getModulePaths(searchPath))
        {
            futures.push_back(std::async(std::launch::async, &Pothos::PluginModule::safeLoad, path.string()));
        }
    }

    //wait for completion of future module load
    std::vector<PluginModule> modules;
    for (auto &future : futures)
    {
        POTHOS_EXCEPTION_TRY
        {
            modules.push_back(future.get());
        }
        POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
        {
            poco_error(Poco::Logger::get("Pothos.PluginLoader.load"), ex.displayText());
        }
    }
    return modules;
}
