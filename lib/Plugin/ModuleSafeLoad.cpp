// Copyright (c) 2013-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/System/Paths.hpp>
#include <Pothos/Plugin/Module.hpp>
#include <Pothos/Plugin/Exception.hpp>
#include <Pothos/Util/FileLock.hpp>
#include "Platform/Process.hpp"
#include <filesystem>
#include <chrono>
#include "Util/PropertyFile.hpp"
#include <mutex>
#include <cctype>
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8

//! The path used to cache the safe loads
static std::string getModuleLoaderCachePath(void)
{
    return Pothos::Util::Detail::pathUtf8(Pothos::Util::Detail::utf8Path(Pothos::System::getUserConfigPath()) / "ModuleLoader.cache");
}

/***********************************************************************
 * named mutex for cache protection
 **********************************************************************/
struct LoaderCacheFileLock : public Pothos::Util::FileLock
{
    LoaderCacheFileLock(void):
        Pothos::Util::FileLock(getModuleLoaderCachePath()+".lock")
    {}
};

static Pothos::Util::FileLock &getLoaderFileLock(void)
{
    static LoaderCacheFileLock lock;
    return lock;
}

static std::mutex &getLoaderMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

/***********************************************************************
 * calls to interface with file cache
 **********************************************************************/
//! Get the string representation of a file's modification time
static std::string getLastModifiedTimeStr(const std::string &path)
{
    const auto modified = std::filesystem::last_write_time(Pothos::Util::Detail::utf8Path(path)).time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(modified).count());
}

//! Escape PropertyFile keys, problem with slashes
static std::string escape(const std::string &in)
{
    std::string out;
    for (const auto ch : in)
    {
        if (std::isalnum(ch)) out.push_back(ch);
        else out.push_back('_');
    }
    return out;
}

//! Was a previous safe-load of this module successful?
static bool previousLoadWasSuccessful(const std::string &modulePath)
{
    std::lock_guard<std::mutex> mutexLock(getLoaderMutex());
    std::lock_guard<Pothos::Util::FileLock> fileLock(getLoaderFileLock());
    Pothos::Util::Detail::PropertyMap cache;
    try {cache = Pothos::Util::Detail::loadPropertyFile(Pothos::Util::Detail::utf8Path(getModuleLoaderCachePath()));} catch(...){}

    const auto cachedLibTime = cache.find(escape(Pothos::System::getPothosRuntimeLibraryPath()));
    const auto cachedModTime = cache.find(escape(modulePath));
    if (cachedLibTime == cache.end() or cachedModTime == cache.end()) return false;
    const auto libTime = getLastModifiedTimeStr(Pothos::System::getPothosRuntimeLibraryPath());
    const auto modTime = getLastModifiedTimeStr(modulePath);
    return (libTime == cachedLibTime->second) and (modTime == cachedModTime->second);
}

//! Mark that the safe load of this module was successful
static void markCurrentLoadSuccessful(const std::string &modulePath)
{
    std::lock_guard<std::mutex> mutexLock(getLoaderMutex());
    std::lock_guard<Pothos::Util::FileLock> fileLock(getLoaderFileLock());
    Pothos::Util::Detail::PropertyMap cache;
    try {cache = Pothos::Util::Detail::loadPropertyFile(Pothos::Util::Detail::utf8Path(getModuleLoaderCachePath()));} catch(...){}

    cache[escape(Pothos::System::getPothosRuntimeLibraryPath())] = getLastModifiedTimeStr(Pothos::System::getPothosRuntimeLibraryPath());
    cache[escape(modulePath)] = getLastModifiedTimeStr(modulePath);
    try {Pothos::Util::Detail::savePropertyFile(Pothos::Util::Detail::utf8Path(getModuleLoaderCachePath()), cache);} catch(...){}
}

/***********************************************************************
 * module safe load implementation
 **********************************************************************/
Pothos::PluginModule Pothos::PluginModule::safeLoad(const std::string &path)
{
    if (previousLoadWasSuccessful(path)) return PluginModule(path);

    const int success = 200;

    //create args
    //the process layer quotes arguments (paths with spaces) as needed
    Pothos::Platform::ProcessOptions opts;
    opts.args = {"--load-module", path, "--success-code", std::to_string(success)};

    //launch: output goes to the null device (a closed pipe would kill the
    //child with a broken pipe as soon as it prints)
    opts.discardOutput = true;
    auto ph = Pothos::Platform::Process::launch(Pothos::Util::Detail::utf8Path(Pothos::System::getPothosUtilExecutablePath()), opts);

    //wait, check error condition, and throw
    if (ph.wait() != success)
    {
        throw Pothos::PluginModuleError("Pothos::PluginModule("+path+")", "failed safe load");
    }

    //it was safe, load into this process now
    markCurrentLoadSuccessful(path);
    return PluginModule(path);
}
