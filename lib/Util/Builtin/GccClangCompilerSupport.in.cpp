// Copyright (c) 2014-2021 Josh Blum
//                    2021 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "Util/Builtin/CompilerStdFlags.hpp"

#include <Pothos/Util/Compiler.hpp>
#include <Pothos/Plugin.hpp>
#include "Platform/Process.hpp"
#include "Platform/DynamicLibrary.hpp"
#include <filesystem>
#include <Poco/Logger.h>
#include <fstream>
#include <iostream>

/***********************************************************************
 * gcc/clang compiler wrapper
 **********************************************************************/
class GccClangCompilerSupport : public Pothos::Util::Compiler
{
public:

    GccClangCompilerSupport(void)
    {
        return;
    }

    std::string compiler_executable(void) const
    {
        return "@CMAKE_CXX_COMPILER@";
    }

    bool test(void)
    {
        Pothos::Platform::Pipe outPipe;
        Pothos::Platform::ProcessOptions opts;
        opts.args = {"--version"};
        opts.out = &outPipe;
        opts.err = &outPipe;
        try
        {
            auto proc = Pothos::Platform::Process::launch(compiler_executable(), opts);
            //drain the output so the compiler never blocks on a full pipe
            Pothos::Platform::PipeInputStream is(outPipe);
            for (std::string line; std::getline(is, line);) {}
            return proc.wait() == 0;
        }
        catch (const Pothos::SystemException &)
        {
            return false; //compiler not found
        }
    }

    std::string compileCppModule(const Pothos::Util::CompilerArgs &args);
};

std::string GccClangCompilerSupport::compileCppModule(const Pothos::Util::CompilerArgs &compilerArgs)
{
    //create args
    std::vector<std::string> args;

    //add libraries
    for (const auto &library : compilerArgs.libraries)
    {
        args.push_back(library);
    }

    //add compiler flags
    args.push_back(CPP_STD_FLAG);
#ifndef __GLIBCXX__
    args.push_back("-stdlib=libc++");
#endif
    args.push_back("-shared");
    args.push_back("-fPIC");
    for (const auto &flag : compilerArgs.flags)
    {
        args.push_back(flag);
    }

    //add include paths
    for (const auto &include : compilerArgs.includes)
    {
        args.push_back("-I");
        args.push_back(include);
    }

    //specify OSX sysroot when available
    #ifdef __APPLE__
    #cmakedefine CMAKE_OSX_SYSROOT
    #ifdef CMAKE_OSX_SYSROOT
        args.push_back("-isysroot");
        args.push_back("@CMAKE_OSX_SYSROOT@");
    #endif //CMAKE_OSX_SYSROOT
    #endif //__APPLE__

    //add compiler sources
    args.push_back("-x");
    args.push_back("c++");
    for (const auto &source : compilerArgs.sources)
    {
        args.push_back(source);
    }

    //create temp out file
    const auto outPath = this->createTempFile(Pothos::Platform::DynamicLibrary::suffix());
    args.push_back("-o");
    args.push_back(outPath);

    //launch
    Pothos::Platform::Pipe inPipe, outPipe;
    Pothos::Platform::ProcessOptions opts;
    opts.args = args;
    opts.in = &inPipe;
    opts.out = &outPipe;
    opts.err = &outPipe;
    auto ph = Pothos::Platform::Process::launch(compiler_executable(), opts);

    //read into output buffer until pipe is closed
    Pothos::Platform::PipeInputStream is(outPipe);
    std::string outBuff;
    for (std::string line; std::getline(is, line);) outBuff += line+'\n';

    //handle error case
    if (ph.wait() != 0 or not std::filesystem::exists(outPath))
    {
        std::string cmdStr(compiler_executable());
        for (const auto &arg : args) cmdStr += " " + arg;
        poco_error_f2(Poco::Logger::get("Pothos.GccClangCompilerSupport.compileCppModule"), "\nCommand: %s\nMessage: %s", cmdStr, outBuff);
        throw Pothos::Exception("GccClangCompilerSupport::compileCppModule", outBuff);
    }

    //return output file path
    return outPath;
}

/***********************************************************************
 * factory and registration
 **********************************************************************/
Pothos::Util::Compiler::Sptr makeGccClangCompilerSupport(void)
{
    return Pothos::Util::Compiler::Sptr(new GccClangCompilerSupport());
}

pothos_static_block(pothosUtilRegisterGccClangCompilerSupport)
{
    Pothos::PluginRegistry::addCall("/util/compiler/gcc_clang", &makeGccClangCompilerSupport);
}
