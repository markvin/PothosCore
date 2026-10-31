// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Plugin.hpp>
#include <Pothos/System/Paths.hpp>
#include <Pothos/Exception.hpp>
#include "Platform/DynamicLibrary.hpp"
#include "Platform/Environment.hpp"
#include "Util/FileSystem.hpp"
#include "Util/TempFile.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

//A std::filesystem error in the JIT factory (here: a source that does not
//exist) becomes a Pothos::Exception that is saved for the next calls.
POTHOS_TEST_BLOCK("/framework/tests", test_jit_missing_source)
{
    using Pothos::Util::Detail::utf8Path;
    const auto id = std::to_string(Pothos::Platform::currentProcessId());
    const std::string target = "pothos_test_jit_missing_" + id;
    const std::string factory = "/pothos_test/jit_missing_" + id;

    //an empty doc source: the loader does not read the missing source
    Pothos::Util::Detail::TempFile confFile;
    const auto dir = confFile.path().parent_path();
    Pothos::Util::Detail::TempFile docFile;

    //a non-empty output file: the factory checks the times of the sources
    const auto outPath = utf8Path(Pothos::System::getUserDataPath()) / "modules" /
        utf8Path(target + Pothos::Platform::DynamicLibrary::suffix());
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream(outPath, std::ios::binary) << "not a library";

    std::map<std::string, std::string> config;
    config["confFilePath"] = Pothos::Util::Detail::pathUtf8(confFile.path());
    config["target"] = target;
    config["sources"] = Pothos::Util::Detail::pathUtf8(dir / utf8Path("pothos_no_such_source_" + id + ".cpp"));
    config["doc_sources"] = Pothos::Util::Detail::pathUtf8(docFile.path());
    config["factories"] = factory;

    const auto loader = Pothos::PluginRegistry::get("/framework/conf_loader/jit_compiler").getObject().extract<Pothos::Callable>();
    const auto entries = loader.call<std::vector<Pothos::PluginPath>>(config);
    const auto call = Pothos::PluginRegistry::get(Pothos::PluginPath("/blocks", factory)).getObject().extract<Pothos::Callable>();
    POTHOS_TEST_THROWS(call.call(static_cast<const Pothos::Object *>(nullptr), size_t(0)), Pothos::FileException);
    POTHOS_TEST_THROWS(call.call(static_cast<const Pothos::Object *>(nullptr), size_t(0)), Pothos::FileException); //saved

    for (const auto &entry : entries) Pothos::PluginRegistry::remove(entry);
    std::error_code ec;
    std::filesystem::remove(outPath, ec);
}
