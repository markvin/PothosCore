// Copyright (c) 2013-2018 Josh Blum
//                    2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include "Util/FileSystem.hpp"
#include "App/PothosUtilOptions.hpp"
#include "App/Server.hpp"
#include "Platform/Environment.hpp"
#include <Pothos/System.hpp>
#include <Pothos/System/Paths.hpp>
#include <Pothos/Exception.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <format>
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

    /*!
     * The parsed command line as arguments again, without the given options:
     * the command line of the daemon (without --daemon) or of a service
     * (without the service options). Options that take a value keep the '='.
     */
    std::vector<std::string> argsWithout(const std::vector<std::string> &excluded) const
    {
        const auto specs = Pothos::App::pothosUtilOptions();
        std::vector<std::string> out;
        for (const auto &[name, value] : _config.given())
        {
            if (std::ranges::find(excluded, name) != excluded.end()) continue;
            const auto spec = std::ranges::find(specs, name, &Pothos::App::OptionSpec::name);
            const bool takesValue = spec != specs.end() and spec->arg != Pothos::App::Arg::None;
            const bool emptyOptional = spec != specs.end() and spec->arg == Pothos::App::Arg::Optional and value.empty();
            out.push_back((takesValue and not emptyOptional)? "--" + name + "=" + value : "--" + name);
        }
        if (not _config.positional().empty()) out.push_back("--");
        out.insert(out.end(), _config.positional().begin(), _config.positional().end());
        return out;
    }

    //! Commands that wait for a termination request (they catch Ctrl-C and SIGTERM)
    bool isServerMode(void) const
    {
        return _config.has("proxy-server") or
            (_config.has("run-topology") and not _config.has("run-duration") and not _config.has("idle-time"));
    }

    using Command = void (PothosUtilBase::*)(const std::string &, const std::string &);

    //! The commands that take a value (in command line order)
    static const std::map<std::string, Command> &valueCommands(void)
    {
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
        return commands;
    }

    //! The flag commands (only the first one runs)
    static constexpr std::array<const char *, 4> FlagCommands{"doc-parse", "device-info", "run-topology", "simd-features"};

    //! True when the command line has a command for runCommands()
    bool hasCommand(void) const
    {
        for (const auto &[name, value] : _config.given())
        {
            if (valueCommands().contains(name)) return true;
        }
        return std::ranges::any_of(FlagCommands, [this](const char *name){return _config.has(name);});
    }

    //! Run the commands: those with a value in command line order, then the first flag command
    void runCommands(void)
    {
        const auto &commands = valueCommands();
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

//! Run f; any exception is printed and becomes the usage exit code
template <typename Fn>
static int runGuarded(Fn &&f)
{
    try
    {
        return f();
    }
    catch (const Pothos::Exception &ex)
    {
        std::cerr << ex.displayText() << std::endl;
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "unknown exception" << std::endl;
    }
    std::cout << std::endl;
    return ExitUsage;
}

int main(int argc, char *argv[])
{
    //last at the return: the logs are written, then a waiting console close
    //handler (Windows) lets the process end
    struct ShutdownGuard
    {
        ~ShutdownGuard(void)
        {
            Pothos::System::Logger::flush();
            Pothos::App::shutdownDone();
        }
    } shutdownGuard;
    PothosUtil app;
    const int parsed = runGuarded([&]{app.parse(Pothos::Platform::commandLineArguments(argc, argv)); return 0;});
    if (parsed != 0) return parsed;

    if (argc <= 1 or app.config().has("help"))
    {
        app.displayHelp();
        return 0;
    }

    return runGuarded([&app]
    {
        const auto &config = app.config();
        #ifdef _WIN32
        if (config.has("register-service"))
        {
            Pothos::App::ServiceOptions opts;
            if (not config.getString("register-service").empty()) opts.name = config.getString("register-service");
            if (not app.hasCommand()) throw Pothos::InvalidArgumentException("--register-service=" + opts.name,
                "give the command of the service, for example --proxy-server=tcp://0.0.0.0:16415");
            opts.displayName = config.getString("display-name");
            opts.description = config.getString("description");
            opts.startup = config.getString("startup", "manual");
            opts.args = app.argsWithout({"register-service", "unregister-service", "display-name", "description", "startup", "run-service"});
            Pothos::App::registerService(opts);
            std::cout << "Registered the service " << opts.name << std::endl;
            return 0;
        }
        if (config.has("unregister-service"))
        {
            const auto name = config.getString("unregister-service");
            Pothos::App::unregisterService(name.empty()? "PothosUtil" : name);
            std::cout << "Removed the service " << (name.empty()? "PothosUtil" : name) << std::endl;
            return 0;
        }
        if (config.has("run-service")) return Pothos::App::runAsService(config.getString("run-service"), [&app]
        {
            Pothos::System::Logger::setupDefaultLogging();
            app.runCommands();
            return app.config().getInt("success-code", 0);
        });
        #else
        //the daemon is a new PothosUtil process (exec): see spawnDaemon
        const auto umaskValue = parseUmask(config);
        if (config.has("daemon"))
        {
            if (config.has("pidfile")) Pothos::App::PidFile::check(Pothos::Util::Detail::utf8Path(config.getString("pidfile")));
            auto args = app.argsWithout({"daemon", "umask"});
            args.push_back("--umask=" + std::format("{:o}", umaskValue.value_or(027)));
            Pothos::App::spawnDaemon(Pothos::System::getPothosUtilExecutablePath(), args);
            return 0;
        }
        if (umaskValue) ::umask(static_cast<mode_t>(*umaskValue));
        #endif

        //only the commands that wait catch termination requests, and before
        //the pid file exists: a request in between must not leave it behind
        if (app.isServerMode()) Pothos::App::installTerminationHandlers();
        std::optional<Pothos::App::PidFile> pidFile;
        if (config.has("pidfile")) pidFile.emplace(Pothos::Util::Detail::utf8Path(config.getString("pidfile")));

        Pothos::System::Logger::setupDefaultLogging();
        app.runCommands();
        return config.getInt("success-code", 0);
    });
}
