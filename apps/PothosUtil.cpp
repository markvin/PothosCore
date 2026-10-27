// Copyright (c) 2013-2018 Josh Blum
//                    2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include "App/PothosUtilOptions.hpp"
#include "App/Server.hpp"
#include <Pothos/System.hpp>
#include <Pothos/Exception.hpp>
#include <charconv>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#ifndef _WIN32
#include <sys/stat.h>
#endif

//! Exit code for a bad command line or a failing command (EX_USAGE)
static constexpr int ExitUsage = 64;

void PothosUtilBase::waitForTerminationRequest(void)
{
    Pothos::App::waitForTerminationRequest();
}

class PothosUtil : public PothosUtilBase
{
public:
    //! Parse the command line, \throws Pothos::Exception for invalid options
    void parse(const std::vector<std::string> &args)
    {
        _config = Pothos::App::parseCommandLine(Pothos::App::pothosUtilOptions(), args);
        for (const auto &value : _config.getAll("var"))
        {
            const auto pos = value.find(":");
            if (pos == std::string::npos) throw Pothos::InvalidArgumentException(
                "Cannot parse --var="+value+", expected --var=name:value format");
            _vars.emplace_back(value.substr(0, pos), value.substr(pos+1));
        }
    }

    void displayHelp(void)
    {
        std::cout << Pothos::App::formatHelp(Pothos::App::pothosUtilOptions(), "PothosUtil OPTIONS",
            "PothosUtil is a helper executable for the Pothos Library; "
            "used both internally by the library, and externally by the user. "
            "The util provides a command-line interface for the self-tests, "
            "and can spawn servers and daemons for remote interfaces.");
    }

    //! Run the commands: those with a value in command line order, then the first flag command
    void runCommands(void)
    {
        using Command = void (PothosUtilBase::*)(const std::string &, const std::string &);
        static const std::map<std::string, Command> commands{
            {"system-info", &PothosUtilBase::printSystemInfo},
            {"module-info", &PothosUtilBase::printModuleInfo},
            {"list-modules", &PothosUtilBase::listModules},
            {"plugin-tree", &PothosUtilBase::printPluginTree},
            {"proxy-server", &PothosUtilBase::proxyServer},
            {"load-module", &PothosUtilBase::loadModule},
            {"self-tests", &PothosUtilBase::selfTests},
            {"self-test1", &PothosUtilBase::selfTestOne},
            {"proxy-environment-info", &PothosUtilBase::printProxyEnvironmentInfo},
            {"generate-simd-dispatchers", &PothosUtilBase::generateSIMDDispatchers},
            {"type-conversions", &PothosUtilBase::printTypeConversions},
        };
        for (const auto &[name, value] : _config.given())
        {
            const auto it = commands.find(name);
            if (it != commands.end()) (this->*(it->second))(name, value);
        }

        if (_config.has("doc-parse")) this->docParse(_config.positional());
        else if (_config.has("device-info")) this->printDeviceInfo();
        else if (_config.has("run-topology")) this->runTopology();
        else if (_config.has("simd-features")) this->printSIMDFeatures();
    }
};

#ifndef _WIN32
static std::optional<unsigned> parseUmask(const Pothos::App::CommandLine &config)
{
    if (not config.has("umask")) return std::nullopt;
    const auto text = config.getString("umask");
    unsigned value = 0;
    const auto result = std::from_chars(text.data(), text.data()+text.size(), value, 8);
    if (text.empty() or result.ec != std::errc() or result.ptr != text.data()+text.size() or value > 0777)
    {
        throw Pothos::InvalidArgumentException("--umask="+text, "expected an octal mask such as 027");
    }
    return value;
}
#endif

int main(int argc, char *argv[])
{
    PothosUtil app;
    try
    {
        app.parse(std::vector<std::string>(argv+1, argv+argc));
    }
    catch (const Pothos::Exception &ex)
    {
        std::cerr << ex.displayText() << std::endl;
        return ExitUsage;
    }

    if (argc <= 1 or app.config().has("help"))
    {
        app.displayHelp();
        return 0;
    }

    try
    {
        //server modes first: fork before Pothos starts any thread
        std::optional<Pothos::App::PidFile> pidFile;
        #ifndef _WIN32
        const auto umaskValue = parseUmask(app.config());
        if (app.config().has("daemon")) Pothos::App::becomeDaemon(umaskValue);
        else if (umaskValue) ::umask(static_cast<mode_t>(*umaskValue));
        if (app.config().has("pidfile")) pidFile.emplace(app.config().getString("pidfile"));
        #endif
        Pothos::App::installTerminationHandlers();

        Pothos::System::Logger::setupDefaultLogging();
        app.runCommands();
        return app.config().getInt("success-code", 0);
    }
    catch (const Pothos::Exception &ex)
    {
        std::cerr << ex.displayText() << std::endl;
        std::cout << std::endl;
    }
    return ExitUsage;
}
