// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System.hpp>
#include <Pothos/Plugin.hpp>
#include "Util/StringUtils.hpp"
#include "Platform/Environment.hpp"
#include <Pothos/System/Logger.hpp>
#include <filesystem>
#include <system_error>
#include <format>
#include "Util/PropertyFile.hpp"
#include <future>
#include <map>
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8


std::string Pothos_FileRealPath(const std::string &path);

/***********************************************************************
 * Load a config file by iterating through sections and running action
 **********************************************************************/
static std::vector<Pothos::PluginPath> loadConfFile(const std::string &path)
{
    POTHOS_LOG_DEBUG("Pothos.ConfLoader", "loading {}", path);

    //parse the configuration file (key = value properties)
    std::map<std::string, std::string> configMap;
    try
    {
        configMap = Pothos::Util::Detail::loadPropertyFile(path);
    }
    catch (const Pothos::Exception &ex)
    {
        throw Pothos::Exception("ConfLoader("+path+")", ex.displayText());
    }

    //and other config file parameters
    configMap["confFilePath"] = path;

    //handle the loader
    POTHOS_EXCEPTION_TRY
    {
        //get the loader
        const auto loaderIt = configMap.find("loader");
        if (loaderIt == configMap.end()) throw Pothos::Exception(
            std::format("{} does not specify a loader", path));
        const std::string loader(loaderIt->second);
        const auto loaderPath = Pothos::PluginPath("/framework/conf_loader").join(loader);
        if (not Pothos::PluginRegistry::exists(loaderPath)) throw Pothos::Exception(
            std::format("{} loader {} does not exist", path, loader));

        //call the loader
        const auto plugin = Pothos::PluginRegistry::get(loaderPath);
        const auto &loaderFcn = plugin.getObject().extract<Pothos::Callable>();
        return loaderFcn.call(configMap);
    }
    POTHOS_EXCEPTION_CATCH (const Pothos::Exception &ex)
    {
        throw Pothos::Exception("ConfLoader("+path+")", ex);
    }
}

/***********************************************************************
 * Traverse a path for configuration files
 **********************************************************************/
static std::vector<std::filesystem::path> getConfFilePaths(const std::filesystem::path &path)
{
    POTHOS_LOG_DEBUG("Pothos.ConfLoader", "traversing {}", Pothos::Util::Detail::pathUtf8(path));

    std::vector<std::filesystem::path> paths;

    std::error_code ec;
    if (not std::filesystem::exists(path, ec)) return paths;
    else if (std::filesystem::is_regular_file(path, ec) and (path.extension() == ".conf"))
    {
        paths.push_back(Pothos_FileRealPath(Pothos::Util::Detail::pathUtf8(path)));
    }
    else if (std::filesystem::is_directory(path, ec))
    {
        for (const auto &entry : std::filesystem::directory_iterator(path, ec))
        {
            auto subpaths = getConfFilePaths(std::filesystem::absolute(entry.path()));
            paths.insert(paths.end(), subpaths.begin(), subpaths.end());
        }
    }

    return paths;
}

/***********************************************************************
 * Entry point: traversal and loading of configuration files
 **********************************************************************/
std::vector<Pothos::PluginPath> Pothos_ConfLoader_loadConfFiles(void)
{
    //the default search path
    std::vector<std::filesystem::path> searchPaths;
    searchPaths.push_back(Pothos::Util::Detail::utf8Path(Pothos::System::getRootPath()) / "share" / "Pothos" / "modules");

    //support /usr/local module installs when the install prefix is /usr
    if (Pothos::System::getRootPath() == "/usr")
    {
        searchPaths.push_back("/usr/local/share/Pothos/modules");
    }

    //the user's home config path
    searchPaths.push_back(Pothos::Util::Detail::utf8Path(Pothos::System::getUserConfigPath()) / "modules");

    //separator for search paths
    #ifdef _WIN32
    const std::string sep(";");
    #else
    const std::string sep(":");
    #endif

    //check the environment's search path
    const auto confPaths = Pothos::Platform::getEnv("POTHOS_CONF_PATH").value_or("");
    for (const auto &confPath : Pothos::Util::Detail::split(confPaths, sep, false, false))
    {
        if (confPath.empty()) continue;
        searchPaths.push_back(Pothos::Util::Detail::utf8Path(confPath));
    }

    //traverse the search paths and spawn futures
    std::vector<std::future<std::vector<Pothos::PluginPath>>> futures;
    for (const auto &searchPath : searchPaths)
    {
        for (const auto &path : getConfFilePaths(std::filesystem::absolute(searchPath)))
        {
            futures.push_back(std::async(std::launch::async, &loadConfFile, Pothos::Util::Detail::pathUtf8(path)));
        }
    }

    //wait for completion of future module load
    std::vector<Pothos::PluginPath> entries;
    for (auto &future : futures)
    {
        try
        {
            auto subEntries = future.get();
            entries.insert(entries.end(), subEntries.begin(), subEntries.end());
        }
        catch(const Pothos::Exception &ex)
        {
            POTHOS_LOG_ERROR("Pothos.ConfLoader", "{}", ex.message());
        }
    }
    return entries;
}
