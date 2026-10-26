// Copyright (c) 2013-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Paths.hpp>
#include <Pothos/System/Version.hpp>
#include <Pothos/System/Exception.hpp>
#include "Platform/Environment.hpp"
#include <filesystem>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#endif
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
    #ifdef _WIN32
    const auto home = Pothos::Platform::getEnv("USERPROFILE");
    #else
    const auto home = Pothos::Platform::getEnv("HOME");
    #endif
    return home ? std::filesystem::path(*home) : std::filesystem::current_path();
}

std::string Pothos::System::getRootPath(void)
{
    if (const auto root = Pothos::Platform::getEnv("@POTHOS_ROOT_ENV@"))
        return absoluteString(*root);

    // Get the path to the current dynamic linked library.
    // The path to this library can be used to determine
    // the installation root without prior knowledge.
    #ifdef _WIN32
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
    if (const auto xdg = Pothos::Platform::getEnv("XDG_DATA_HOME"))
    {
        dataPath = *xdg;
    }
    else if (const auto appData = Pothos::Platform::getEnv("@APPDATA_ENV@"))
    {
        dataPath = *appData;
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
    if (const auto xdg = Pothos::Platform::getEnv("XDG_CONFIG_HOME"))
    {
        configPath = *xdg;
    }
    else if (const auto appData = Pothos::Platform::getEnv("@APPDATA_ENV@"))
    {
        configPath = *appData;
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
    #ifdef _WIN32
    utilPath.replace_extension("exe");
    #endif

    return absoluteString(utilPath);
}

std::string Pothos::System::getPothosRuntimeLibraryPath(void)
{
    std::filesystem::path dllPath(Pothos::System::getRootPath());
    #ifdef _WIN32
    dllPath /= "@CMAKE_INSTALL_BINDIR@";
    #else
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
    #ifdef _WIN32
    const std::string sep(";");
    #else
    const std::string sep(":");
    #endif

    //check the environment's search path
    const auto pluginPaths = Pothos::Platform::getEnv("POTHOS_PLUGIN_PATH").value_or("");
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
