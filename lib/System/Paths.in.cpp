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
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8: the std::string paths are UTF-8
#include <algorithm>
#include <iterator>

static std::string absoluteString(const std::filesystem::path &path)
{
    return Pothos::Util::Detail::pathUtf8(std::filesystem::absolute(path));
}

//the user's home directory (HOME, or USERPROFILE on Windows)
static std::filesystem::path homePath(void)
{
    #ifdef _WIN32
    const auto home = Pothos::Platform::getEnv("USERPROFILE");
    #else
    const auto home = Pothos::Platform::getEnv("HOME");
    #endif
    return home ? Pothos::Util::Detail::utf8Path(*home) : std::filesystem::current_path();
}

std::string Pothos::System::getRootPath(void)
{
    if (const auto root = Pothos::Platform::getEnv("@POTHOS_ROOT_ENV@"))
        return absoluteString(Pothos::Util::Detail::utf8Path(*root));

    // Get the path to the current dynamic linked library.
    // The path to this library can be used to determine
    // the installation root without prior knowledge.
    #ifdef _WIN32
    wchar_t path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR) &Pothos::System::getRootPath, &hm))
    {
        const DWORD size = GetModuleFileNameW(hm, path, MAX_PATH);
        if (size != 0)
        {
            const std::filesystem::path libPath(std::wstring(path, size));
            return Pothos::Util::Detail::pathUtf8(libPath.parent_path().parent_path());
        }
    }
    #endif

    //assume that the root path is always in UNIX style
    return absoluteString(Pothos::Util::Detail::utf8Path("@POTHOS_ROOT@"));
}

std::string Pothos::System::getDataPath(void)
{
    return absoluteString(Pothos::Util::Detail::utf8Path(getRootPath()) / "@CMAKE_INSTALL_DATADIR@" / "Pothos");
}

std::string Pothos::System::getUserDataPath(void)
{
    std::filesystem::path dataPath;
    if (const auto xdg = Pothos::Platform::getEnv("XDG_DATA_HOME"))
    {
        dataPath = Pothos::Util::Detail::utf8Path(*xdg);
    }
    else if (const auto appData = Pothos::Platform::getEnv("@POTHOS_APPDATA_ENV@"))
    {
        dataPath = Pothos::Util::Detail::utf8Path(*appData);
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
        configPath = Pothos::Util::Detail::utf8Path(*xdg);
    }
    else if (const auto appData = Pothos::Platform::getEnv("@POTHOS_APPDATA_ENV@"))
    {
        configPath = Pothos::Util::Detail::utf8Path(*appData);
    }
    else
    {
        configPath = homePath() / ".config";
    }
    return absoluteString(configPath / "Pothos");
}

std::string Pothos::System::getPothosUtilExecutablePath(void)
{
    auto utilPath = Pothos::Util::Detail::utf8Path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_BINDIR@" / "PothosUtil";
    #ifdef _WIN32
    utilPath.replace_extension("exe");
    #endif

    return absoluteString(utilPath);
}

std::string Pothos::System::getPothosRuntimeLibraryPath(void)
{
    auto dllPath = Pothos::Util::Detail::utf8Path(Pothos::System::getRootPath());
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
    return absoluteString(Pothos::Util::Detail::utf8Path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_INCLUDEDIR@");
}

std::string Pothos::System::getPothosDevLibraryPath(void)
{
    return absoluteString(Pothos::Util::Detail::utf8Path(Pothos::System::getRootPath()) / "@CMAKE_INSTALL_LIBDIR@");
}

std::vector<std::string> Pothos::System::getPothosModuleSearchPaths()
{
    std::vector<std::filesystem::path> searchPaths;

    //the default search path
    searchPaths.push_back(Pothos::Util::Detail::utf8Path(Pothos::System::getPothosDevLibraryPath()) / "Pothos" / ("modules" + Pothos::System::getAbiVersion()));

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
        searchPaths.push_back(Pothos::Util::Detail::utf8Path(pluginPath));
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
