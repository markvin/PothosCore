// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Paths.hpp>
#include <Pothos/Plugin/Module.hpp>
#include <Pothos/Plugin/Registry.hpp>
#include <Pothos/Plugin/Exception.hpp>
#include <Pothos/Object.hpp> //pulls in full Object implementation
#include "Platform/DynamicLibrary.hpp"
#include <optional>
#include <Poco/Logger.h>
#include <mutex>

/***********************************************************************
 * Disabler for windows error messages
 **********************************************************************/
#ifdef _WIN32

#include <windows.h>

BOOL DL_SetThreadErrorMode(DWORD dwNewMode, LPDWORD lpOldMode);

struct ErrorMessageDisableGuard
{
    ErrorMessageDisableGuard(void)
    {
        DL_SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX, &oldMode);
    }
    ~ErrorMessageDisableGuard(void)
    {
        DL_SetThreadErrorMode(oldMode, nullptr);
    }
    DWORD oldMode;
};

#else
struct ErrorMessageDisableGuard
{
    ErrorMessageDisableGuard(void){}
    ~ErrorMessageDisableGuard(void){}
};
#endif

/***********************************************************************
 * Locking for plugin attach helper
 **********************************************************************/
void registrySetActiveModuleLoading(const Pothos::PluginModule &module);

std::vector<std::string> getPluginPaths(const Pothos::PluginModule &module);

static std::mutex &getModuleMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

//where to store the version during module load
static thread_local std::string *currentModuleVersion(nullptr);

/***********************************************************************
 * Shared implementation for module data
 **********************************************************************/
struct Pothos::PluginModule::Impl
{
    std::optional<Pothos::Platform::DynamicLibrary> sharedLibrary;
    std::string path;
    std::vector<std::string> pluginPaths;
    std::string version;
};

/***********************************************************************
 * Module implementation
 **********************************************************************/
const Pothos::PluginModule &Pothos::PluginModule::null(void)
{
    static PluginModule nullModule;
    return nullModule;
}

Pothos::PluginModule::PluginModule(void)
{
    return;
}

Pothos::PluginModule::PluginModule(const std::string &path):
    _impl(new Impl())
{
    _impl->path = path;
    poco_debug(Poco::Logger::get("Pothos.PluginModule.load"), path);
    try
    {
        std::lock_guard<std::mutex> lock(getModuleMutex());
        registrySetActiveModuleLoading(*this);
        currentModuleVersion = &(_impl->version);
        ErrorMessageDisableGuard emdg;
        _impl->sharedLibrary.emplace(path);
        currentModuleVersion = nullptr;
        registrySetActiveModuleLoading(PluginModule());
        _impl->pluginPaths = ::getPluginPaths(*this);
    }
    catch(const Pothos::LibraryLoadException &ex)
    {
        currentModuleVersion = nullptr;
        registrySetActiveModuleLoading(PluginModule());
        throw PluginModuleError("Pothos::PluginModule("+path+")", ex.displayText());
    }
}

Pothos::PluginModule::~PluginModule(void)
{
    if (not _impl) return; //no private data
    if (_impl.use_count() != 1) return; //this is not the last copy
    if (not _impl->sharedLibrary) return; //module not loaded

    poco_debug(Poco::Logger::get("Pothos.PluginModule.unload"), _impl->sharedLibrary->path().string());
    for (const auto &pluginPath : this->getPluginPaths())
    {
        PluginRegistry::remove(pluginPath);
    }
    _impl->sharedLibrary.reset();
}

std::string Pothos::PluginModule::getFilePath(void) const
{
    if (not _impl) return "";
    return _impl->path;
}

const std::vector<std::string> &Pothos::PluginModule::getPluginPaths(void) const
{
    return _impl->pluginPaths;
}

Pothos::PluginModule::operator bool(void) const
{
    return bool(_impl);
}

std::string Pothos::PluginModule::getVersion(void) const
{
    if (not _impl) return "";
    return _impl->version;
}

Pothos::ModuleVersion::ModuleVersion(const std::string &version)
{
    if (currentModuleVersion != nullptr) *currentModuleVersion = version;
}

#include <Pothos/Managed.hpp>

static auto managedPluginModule = Pothos::ManagedClass()
    .registerConstructor<Pothos::PluginModule>()
    .registerConstructor<Pothos::PluginModule, const std::string &>()
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::PluginModule, getFilePath))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::PluginModule, getPluginPaths))
    .commit("Pothos/PluginModule");
