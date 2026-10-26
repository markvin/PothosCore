// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Paths.hpp>
#include <Pothos/System/Version.hpp>
#include <Pothos/System/Exception.hpp>
#include <Poco/Environment.h>
#include <filesystem>
#include <cstdlib>
#include <Poco/Foundation.h>
#include "Util/StringUtils.hpp"
#include <algorithm>
#include <iterator>

static std::string absoluteString(const std::filesystem::path &path)
{
    return std::filesystem::absolute(path).string();
}

//the user's home directory (HOME, or USERPROFILE on Windows)
static std::filesystem::path homePath(void)
{
    #if defined(POCO_OS_FAMILY_WINDOWS)
    const char *home = std::getenv("USERPROFILE");
    #else
    const char *home = std::getenv("HOME");
    #endif
    return home == nullptr ? std::filesystem::current_path() : std::filesystem::path(home);
}

std::string Pothos::System::getRootPath(void)
{
    if (Poco::Environment::has("@POTHOS_ROOT_ENV@"))
        return absoluteString(Poco::Environment::get("@POTHOS_ROOT_ENV@"));

    // Get the path to the current dynamic linked library.
    // The path to this library can be used to determine
    // the installation root without prior knowledge.
    #if defined(POCO_OS_FAMILY_WINDOWS)
    char path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &Pothos::System::getRootPath, &hm))
    {
        const DWORD size = GetModuleFileNameA(hm, path, sizeof(path));
        if (size != 0)
        {
            const std::filesystem::path libPath(std::string(path, size));
            return libPath.parent_path().parent_path().string();
        }
    }
    #endif

    //assume that the root path is always in UNIX style
    return absoluteString(std::filesystem::path("@POTHOS_ROOT@"));
}

std::string Pothos::System::getDataPath(void)
{
    return absoluteString(std::filesystem::path(getRootPath()) / "@CMAKE_INSTALL_DATADIR@" / "Pothos");
}

std::string Pothos::System::getUserDataPath(void)
{
    std::filesystem::path dataPath;
    if (Poco::Environment::has("XDG_DATA_HOME"))
    {
        dataPath = Poco::Environment::get("XDG_DATA_HOME");
    }
    else if (Poco::Environment::has("@APPDATA_ENV@"))
    {
        dataPath = Poco::Environment::get("@APPDATA_ENV@");
    }
    else
    {
        dataPath = homePath() / ".local" / "@CMAKE_INSTALL_DATADIR@";
    }
    return absoluteString(dataPath / "Pothos");
}

std::string Pothos::System::getUserConfigPath(void)
{
    std::filesystem::path configPath;
    if (Poco::Environment::has("XDG_CONFIG_HOME"))
    {
        configPath = Poco::Environment::get("XDG_CONFIG_HOME");
    }
    else if (Poco::Environment::has("@APPDATA_ENV@"))
    {
        configPath = Poco::Environment::get("@APPDATA_ENV@");
    }
    else
    {
        configPath = homePath() / ".config";
    }
    return absoluteString(configPath / "Pothos");
}

std::string Pothos::System::getPothosUtilExecutablePath(void)
{
    auto utilPath = std::filesystem::path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_BINDIR@" / "PothosUtil";
    #if defined(POCO_OS_FAMILY_WINDOWS)
    utilPath.replace_extension("exe");
    #endif

    return absoluteString(utilPath);
}

std::string Pothos::System::getPothosRuntimeLibraryPath(void)
{
    std::filesystem::path dllPath(Pothos::System::getRootPath());
    #if defined(POCO_OS_FAMILY_WINDOWS)
    dllPath /= "@CMAKE_INSTALL_BINDIR@";
    #elif defined(POCO_OS_FAMILY_UNIX)
    dllPath /= "@CMAKE_INSTALL_LIBDIR@";
    #endif
    dllPath /= POTHOS_LIBRARY_NAME;

    return absoluteString(dllPath);
}

std::string Pothos::System::getPothosDevIncludePath(void)
{
    return absoluteString(std::filesystem::path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_INCLUDEDIR@");
}

std::string Pothos::System::getPothosDevLibraryPath(void)
{
    return absoluteString(std::filesystem::path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_LIBDIR@");
}

std::vector<std::string> Pothos::System::getPothosModuleSearchPaths()
{
    std::vector<std::filesystem::path> searchPaths;

    //the default search path
    searchPaths.push_back(std::filesystem::path(Pothos::System::getPothosDevLibraryPath()) / "Pothos" / ("modules" + Pothos::System::getAbiVersion()));

    //support /usr/local module installs when the install prefix is /usr
    if (Pothos::System::getRootPath() == "/usr")
    {
        searchPaths.push_back("/usr/local/@CMAKE_INSTALL_LIBDIR@/Pothos/modules" + Pothos::System::getAbiVersion());
        //when using a multi-arch directory, support single-arch path as well
        static const std::string libdir("@CMAKE_INSTALL_LIBDIR@");
        if (libdir.find("lib/") == 0) //startswith lib/ indicating multi-arch
            searchPaths.push_back("/usr/local/lib/Pothos/modules" + Pothos::System::getAbiVersion());

    }

    //separator for search paths
    #if defined(POCO_OS_FAMILY_WINDOWS)
    const std::string sep(";");
    #else
    const std::string sep(":");
    #endif

    //check the environment's search path
    const auto pluginPaths = Poco::Environment::get("POTHOS_PLUGIN_PATH", "");
    for (const auto &pluginPath : Pothos::Util::Detail::split(pluginPaths, sep, false, false))
    {
        if (pluginPath.empty()) continue;
        searchPaths.push_back(pluginPath);
    }

    std::vector<std::string> searchPathStrings;
    std::transform(
        searchPaths.begin(),
        searchPaths.end(),
        std::back_inserter(searchPathStrings),
        [](const std::filesystem::path &path)
        {
            return absoluteString(path);
        });

    return searchPathStrings;
}
