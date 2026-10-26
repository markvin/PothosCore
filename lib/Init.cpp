// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Init.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/System/Paths.hpp>
#include <Pothos/Plugin.hpp>
#include <format>
#include "Util/FileSystem.hpp"
#include <filesystem>

namespace fs = std::filesystem;
using Pothos::Util::Detail::canAccess;
using Pothos::Util::Detail::Access;

//from lib/Framework/ConfLoader.cpp
std::vector<Pothos::PluginPath> Pothos_ConfLoader_loadConfFiles(void);

/***********************************************************************
 * Singleton for initialization once per process
 **********************************************************************/
namespace Pothos {

    class InitSingleton
    {
    public:
        InitSingleton(void);
        static InitSingleton &instance(void);
        void load(void);
        void unload(void);

    private:
        std::vector<Pothos::PluginModule> modules;
        std::vector<Pothos::PluginPath> confLoadedPaths;
    };

} //namespace Pothos

Pothos::InitSingleton &Pothos::InitSingleton::instance(void)
{
    static Pothos::InitSingleton inst;
    return inst;
}

void Pothos::InitSingleton::load(void)
{
    if (not modules.empty()) return;
    modules = PluginLoader::loadModules();
    confLoadedPaths = Pothos_ConfLoader_loadConfFiles();
}

void Pothos::InitSingleton::unload(void)
{
    for (const auto &path : confLoadedPaths)
    {
        Pothos::PluginRegistry::remove(path);
    }
    confLoadedPaths.clear();
    modules.clear();
}

/***********************************************************************
 * Actual init implementation
 **********************************************************************/
Pothos::InitSingleton::InitSingleton(void)
{
    //check the root
    const fs::path rootPath(System::getRootPath());
    if (not fs::is_directory(rootPath)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos installation FAIL - \"{}\" does not exist",
        System::getRootPath()
    ));

    //check for the util executable
    const fs::path utilExecutablePath(System::getPothosUtilExecutablePath());
    if (not fs::exists(utilExecutablePath)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos installation FAIL - \"{}\" does not exist",
        System::getPothosUtilExecutablePath()
    ));
    if (not canAccess(utilExecutablePath, Access::Execute)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos installation FAIL - \"{}\" not executable",
        System::getPothosUtilExecutablePath()
    ));

    //check for the runtime library
    const fs::path runtimeLibraryPath(System::getPothosRuntimeLibraryPath());
    if (not fs::is_regular_file(runtimeLibraryPath)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos installation FAIL - \"{}\" does not exist",
        System::getPothosRuntimeLibraryPath()
    ));

    //check the user data directory
    const fs::path userDataPath(System::getUserDataPath());
    std::error_code ec;
    if (not fs::exists(userDataPath)) fs::create_directories(userDataPath, ec);
    if (not fs::exists(userDataPath) or not canAccess(userDataPath, Access::Write)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos user directory FAIL - \"{}\" permissions error",
        System::getUserDataPath()
    ));

    //check the user config directory
    const fs::path userConfigPath(System::getUserConfigPath());
    if (not fs::exists(userConfigPath)) fs::create_directories(userConfigPath, ec);
    if (not fs::exists(userConfigPath) or not canAccess(userConfigPath, Access::Write)) throw Exception(
        "Pothos::init()", std::format(
        "Pothos user directory FAIL - \"{}\" permissions error",
        System::getUserConfigPath()
    ));

    //check the development environment (includes)
    if (not fs::exists(System::getPothosDevIncludePath())) throw Exception(
        "Pothos::init()", std::format(
        "Pothos development include directory FAIL - \"{}\" does not exist",
        System::getPothosDevIncludePath()
    ));

    //check the development environment (libraries)
    if (not fs::exists(System::getPothosDevLibraryPath())) throw Exception(
        "Pothos::init()", std::format(
        "Pothos development library directory FAIL - \"{}\" does not exist",
        System::getPothosDevLibraryPath()
    ));
}

void Pothos::init(void)
{
    Pothos::InitSingleton::instance().load();
}

void Pothos::deinit(void)
{
    Pothos::InitSingleton::instance().unload();
}

Pothos::ScopedInit::ScopedInit(void)
{
    Pothos::init();
}

Pothos::ScopedInit::~ScopedInit(void)
{
    Pothos::deinit();
}
