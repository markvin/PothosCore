// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <Pothos/System.hpp>
#include <Pothos/Util/Compiler.hpp>
#include <Pothos/Util/FileLock.hpp>
#include <Pothos/Util/BlockDescription.hpp>
#include <Poco/Logger.h>
#include "Util/FileSystem.hpp"
#include <filesystem>
#include "Util/StringUtils.hpp"
#include "Platform/DynamicLibrary.hpp"
#include <memory>
#include <mutex>
#include <cctype>
#include <exception>
#include <map>

static Poco::Logger &sourceLoaderLogger(void)
{
    static Poco::Logger &logger(Poco::Logger::get("Pothos.JITCompiler"));
    return logger;
}

/*!
 * Shared data structure for a JIT entry:
 * An entry has multiple factories but only one compilation unit.
 */
struct RegistryJITResult
{
    std::mutex mutex; //!< process-wide mutex for this compilation unit

    Pothos::PluginModule pluginModule; //!< plugin module holds result

    std::vector<Pothos::PluginPath> factories; //!< factories created by this module

    std::exception_ptr exception; //!< saved exception thrown while compiling

    Pothos::Util::CompilerArgs compilerArgs; //! Compiler arguments

    std::string target; //!< unique target name

    void removeRegistrations(void)
    {
        for (const auto &factoryPath : this->factories)
        {
            Pothos::PluginRegistry::remove(factoryPath);
        }
        factories.clear();
    }

    ~RegistryJITResult(void)
    {
        this->removeRegistrations();
    }
};

/***********************************************************************
 * Helper to manage recompile
 **********************************************************************/
static void compilationHelper(
    RegistryJITResult *handle,
    const std::filesystem::path &outFile)
{
    //check if we need to recompile
    bool recompile = false;
    if (std::filesystem::exists(outFile) and std::filesystem::file_size(outFile) != 0)
    {
        const auto lastTimeCompiled = std::filesystem::last_write_time(outFile);
        const auto devLib = Pothos::System::getPothosDevLibraryPath();
        if (std::filesystem::last_write_time(devLib) > lastTimeCompiled) recompile = true;
        for (const auto &source : handle->compilerArgs.sources)
        {
            if (std::filesystem::last_write_time(source) > lastTimeCompiled) recompile = true;
        }
    }
    else recompile = true;
    if (not recompile) return;

    //compiler instance
    const auto compiler = Pothos::Util::Compiler::make();

    //compile
    sourceLoaderLogger().information("Compile sources for %s...", handle->target);
    const auto tmpOutput = compiler->compileCppModule(handle->compilerArgs);
    Pothos::Util::Detail::moveFile(tmpOutput, outFile);
    sourceLoaderLogger().information("Wrote %s", outFile.string());
}

/***********************************************************************
 * The JIT compiler factory compiles sources on demand
 * and replaces its plugin registration with the compiled on
 **********************************************************************/
static Pothos::Object opaqueJITCompilerFactory(
    RegistryJITResult *handle,
    const Pothos::PluginPath &pluginPath,
    const Pothos::Object *args,
    const size_t numArgs)
{
    //local mutex lock for the registry
    std::lock_guard<std::mutex> lock(handle->mutex);

    //re-throw if a previous call failed
    if (handle->exception) std::rethrow_exception(handle->exception);

    //determine output file
    auto outPath = std::filesystem::path(Pothos::System::getUserDataPath()) / "modules";
    std::filesystem::create_directories(outPath);
    outPath /= handle->target + Pothos::Platform::DynamicLibrary::suffix();

    //file lock for compilation atomicity across processes
    Pothos::Util::FileLock outputFileLock(outPath.string());
    std::lock_guard<Pothos::Util::FileLock> fileLock(outputFileLock);

    //compile if changed
    try
    {
        compilationHelper(handle, outPath);
    }
    catch (const Pothos::Exception &ex)
    {
        handle->exception = std::current_exception();
        sourceLoaderLogger().error(ex.message());
        throw;
    }

    //load the module, only once for all registered entries
    if (not handle->pluginModule)
    {
        //remove all registrations before loading
        handle->removeRegistrations();

        //load the newly compiled library with plugin module
        //the plugin module now owns the factory path entries
        handle->pluginModule = Pothos::PluginModule(outPath.string());
    }

    //the actual function from the compiled module
    const auto plugin = Pothos::PluginRegistry::get(pluginPath);
    const auto &call = plugin.getObject().extract<Pothos::Callable>();
    return call.opaqueCall(args, numArgs);
}

/***********************************************************************
 * Register factory functions that will compile the source
 **********************************************************************/
static std::vector<Pothos::PluginPath> JITCompilerLoader(const std::map<std::string, std::string> &config)
{
    std::vector<Pothos::PluginPath> entries;
        const std::string tokSep(" \t");

    //create handle to hold shared compilation result
    std::shared_ptr<RegistryJITResult> handle(new RegistryJITResult());

    //config file path set by caller
    const auto confFilePathIt = config.find("confFilePath");
    if (confFilePathIt == config.end() or confFilePathIt->second.empty())
        throw Pothos::Exception("missing confFilePath");
    const auto rootDir = std::filesystem::path(confFilePathIt->second).parent_path();

    //get the target (config basename unless specified)
    const auto targetIt = config.find("target");
    if (targetIt != config.end()) handle->target = targetIt->second;
    else handle->target = std::filesystem::path(confFilePathIt->second).stem().string();
    if (handle->target.empty()) throw Pothos::Exception("target empty");

    //load the compiler args
    handle->compilerArgs = Pothos::Util::CompilerArgs::defaultDevEnv();

    //load the includes: make absolute to the config dir
    const auto includesIt = config.find("includes");
    if (includesIt != config.end()) for (const auto &include :
        Pothos::Util::Detail::split(includesIt->second, tokSep))
    {
        const auto absPath = Pothos::Util::Detail::makeAbsolute(include, rootDir);
        handle->compilerArgs.includes.push_back(absPath.string());
    }

    //load the libraries: make absolute to the config dir
    const auto librariesIt = config.find("libraries");
    if (librariesIt != config.end()) for (const auto &library :
        Pothos::Util::Detail::split(librariesIt->second, tokSep))
    {
        const auto absPath = Pothos::Util::Detail::makeAbsolute(library, rootDir);
        handle->compilerArgs.libraries.push_back(absPath.string());
    }

    //load the sources: make absolute to the config dir
    const auto sourcesIt = config.find("sources");
    if (sourcesIt != config.end()) for (const auto &source :
        Pothos::Util::Detail::split(sourcesIt->second, tokSep))
    {
        const auto absPath = Pothos::Util::Detail::makeAbsolute(source, rootDir);
        handle->compilerArgs.sources.push_back(absPath.string());
    }

    //load the flags:
    const auto flagsIt = config.find("flags");
    if (flagsIt != config.end()) for (const auto &flag :
        Pothos::Util::Detail::split(flagsIt->second, tokSep))
    {
        handle->compilerArgs.flags.push_back(flag);
    }

    //doc sources: scan sources unless doc sources are specified
    std::vector<std::string> docSources;
    const auto docSourcesIt = config.find("doc_sources");
    if (docSourcesIt != config.end()) for (const auto &docSource :
        Pothos::Util::Detail::split(docSourcesIt->second, tokSep))
    {
        const auto absPath = Pothos::Util::Detail::makeAbsolute(docSource, rootDir);
        docSources.push_back(absPath.string());
    }
    else docSources = handle->compilerArgs.sources;

    //load the factories: use this when providing no block description
    const auto factoriesIt = config.find("factories");
    if (factoriesIt != config.end()) for (const auto &factory :
        Pothos::Util::Detail::split(factoriesIt->second, tokSep))
    {
        handle->factories.push_back(Pothos::PluginPath("/blocks", factory));
    }

    //generate JSON block descriptions
    Pothos::Util::BlockDescriptionParser parser;
    for (const auto &source : docSources) parser.feedFilePath(source);

    //store block paths in handle, and store doc paths
    for (const auto &factory : parser.listFactories())
    {
        const auto pluginPath = Pothos::PluginPath("/blocks/docs", factory);
        Pothos::PluginRegistry::add(pluginPath, parser.getJSONObject(factory));
        entries.push_back(pluginPath);
        handle->factories.push_back(Pothos::PluginPath("/blocks", factory));
    }

    //register for all factory paths
    //the handle retains ownership of the factory calls
    for (const auto &pluginPath : handle->factories)
    {
        const auto factory = Pothos::Callable(&opaqueJITCompilerFactory)
            .bind(handle.get(), 0)
            .bind(pluginPath, 1);
        Pothos::PluginRegistry::addCall(pluginPath, factory);
    }

    //store the handle in the registry
    const auto pluginPath = Pothos::PluginPath("/framework/conf_loader/jit_compiler/handles").join(handle->target);
    Pothos::PluginRegistry::add(pluginPath, handle);
    entries.push_back(pluginPath);
    return entries;
}

/***********************************************************************
 * loader registration
 **********************************************************************/
pothos_static_block(pothosFrameworkRegisterJITCompilerLoader)
{
    Pothos::PluginRegistry::addCall("/framework/conf_loader/jit_compiler", &JITCompilerLoader);
}
