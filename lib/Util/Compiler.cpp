// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Util/Compiler.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Callable.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/System/Paths.hpp>
#include "Util/TempFile.hpp"
#include <filesystem>
#include <system_error>

Pothos::Util::CompilerArgs::CompilerArgs(void)
{
    return;
}

Pothos::Util::CompilerArgs Pothos::Util::CompilerArgs::defaultDevEnv(void)
{
    Pothos::Util::CompilerArgs args;

    //add devel environment libraries
    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(Pothos::System::getPothosDevLibraryPath(), ec))
    {
        const auto &library = entry.path();
        const auto file = library.filename().string();
        if ((
            library.extension() == ".so" or
            library.extension() == ".lib" or
            library.extension() == ".dylib"
        ) and
        (
            file.find("Poco") != std::string::npos or
            file.find("Pothos") != std::string::npos
        )) args.libraries.push_back(std::filesystem::absolute(library).string());
    }

    //add devel environment includes
    args.includes.push_back(Pothos::System::getPothosDevIncludePath());

    return args;
}

Pothos::Util::Compiler::~Compiler(void)
{
    for (const auto &path : _tempFiles)
    {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
}

Pothos::Util::Compiler::Sptr Pothos::Util::Compiler::make(const std::string &name)
{
    //handle the empty name/automatic case
    if (name.empty())
    {
        for (const auto &factory_name : PluginRegistry::list("/util/compiler"))
        {
            auto compiler = Pothos::Util::Compiler::make(factory_name);
            if (compiler->test()) return compiler;
        }
        throw Pothos::Exception("Pothos::Util::Compiler::make()", "no available or passing factories");
    }

    //locate the compiler factory and make it
    Sptr compiler;
    try
    {
        const auto plugin = Pothos::PluginRegistry::get(Pothos::PluginPath("/util/compiler").join(name));
        const auto &callable = plugin.getObject().extract<Pothos::Callable>();
        compiler = callable.call();
    }
    catch(const Exception &ex)
    {
        throw Pothos::Exception("Pothos::Util::Compiler::make("+name+")", ex);
    }
    return compiler;
}

const std::string &Pothos::Util::Compiler::createTempFile(const std::string &ext)
{
    _tempFiles.push_back(Pothos::Util::Detail::uniqueTempPath(ext).string());
    return _tempFiles.back();
}
