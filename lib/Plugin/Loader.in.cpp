// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin/Loader.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/System/Logger.hpp>
#include <filesystem>
#include <system_error>
#include <future>
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8

static std::vector<std::filesystem::path> getModulePaths(const std::filesystem::path &path)
{
    POTHOS_LOG_DEBUG("Pothos.PluginLoader.load", "{}", Pothos::Util::Detail::pathUtf8(path));

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
        for (const auto &path : getModulePaths(Pothos::Util::Detail::utf8Path(searchPath)))
        {
            futures.push_back(std::async(std::launch::async, &Pothos::PluginModule::safeLoad, Pothos::Util::Detail::pathUtf8(path)));
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
            POTHOS_LOG_ERROR("Pothos.PluginLoader.load", "{}", ex.displayText());
        }
    }
    return modules;
}
