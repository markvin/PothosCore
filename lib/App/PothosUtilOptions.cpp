// Copyright (c) 2013-2018 Josh Blum
//                    2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/PothosUtilOptions.hpp"

using Pothos::App::Arg;

std::vector<Pothos::App::OptionSpec> Pothos::App::pothosUtilOptions(void)
{
    return {
        {"help", 'h', Arg::None, "", "display argument help information"},
        {"system-info", 0, Arg::None, "", "display system information"},
        {"module-info", 0, Arg::Optional, "modulePath",
            "Display a summary of loaded modules.\n"
            "Or specify an optional module path to print the list of associated plugins. "
            "Or specify the special value of 'builtin' to see Builtin library plugins."},
        {"list-modules", 0, Arg::None, "", "list all loaded modules"},
        {"device-info", 0, Arg::Optional, "deviceType", "display device information"},
        {"plugin-tree", 0, Arg::Optional, "pluginPath", "display plugin tree"},
        {"proxy-server", 0, Arg::Optional, "URI", "run the proxy server, tcp://bindHost:bindPort"},
        {"load-module", 0, Arg::Required, "modulePath", "test load a library module"},
        {"run-topology", 0, Arg::Required, "inputFile", "run a topology from a JSON description"},
        {"run-duration", 0, Arg::Required, "runDuration", "run the topology for the duration in seconds"},
        {"idle-time", 0, Arg::Required, "idleTime",
            "The maximum allowed idle time in seconds.\n"
            "The topology will exit after all flows remain idle for the specified time in seconds. "
            "Use this option with --run-duration to specify a timeout to wait for idle to occur. "
            "PothosUtil will return an error code if the timeout is reached before idle occurs."},
        {"var", 0, Arg::Required, "name:value",
            "Specify an arbitrary keyword + value variable\n"
            "using the format --var=name:value\n"
            "Multiple instances of --var are possible.\n"
            "Use with --run-topology to overload globals.", true},
        {"self-tests", 0, Arg::Optional, "pluginPath", "run all plugin self tests"},
        {"self-test1", 0, Arg::Required, "pluginPath", "run a particular plugin self test"},
        {"num-trials", 0, Arg::Required, "numTrials", "how many times to run each self test"},
        {"success-code", 0, Arg::Required, "successCode", "the success status return code (default 0)"},
        {"require-active", 0, Arg::None, "", "proxy server shuts off without active clients"},
        {"output", 0, Arg::Required, "outputFile",
            "Specify an output file (used by various options)\n"
            "Use with --run-topology to dump JSON statistics."},
        {"doc-parse", 0, Arg::None, "", "parse specified files for documentation markup"},
        {"proxy-environment-info", 0, Arg::Optional, "proxyEnvName", "display information for a given proxy environment"},
        {"simd-features", 0, Arg::None, "", "print available SIMD features"},
        {"generate-simd-dispatchers", 0, Arg::Required, "inputFile", "Generate SIMD dispatchers based on a given JSON file"},
        {"simd-arches", 0, Arg::Required, "simdArches", "A comma-delimited list of SIMD arches, as generated at CMake-time"},
        {"type-conversions", 0, Arg::Optional, "type", "Print types convertible to and from a given type"},
        #ifndef _WIN32
        {"daemon", 0, Arg::None, "", "Run as a daemon (with --proxy-server)."},
        {"umask", 0, Arg::Required, "mask", "Set the umask of the process (octal, e.g. 027)."},
        {"pidfile", 0, Arg::Required, "path", "Write the process ID to the given file."},
        #else
        {"register-service", 0, Arg::Optional, "name",
            "Register PothosUtil with the other options of this command line as a Windows service "
            "(default name PothosUtil)."},
        {"unregister-service", 0, Arg::Optional, "name", "Remove a Windows service registered with --register-service."},
        {"display-name", 0, Arg::Required, "name", "The display name of the service (with --register-service)."},
        {"description", 0, Arg::Required, "text", "The description of the service (with --register-service)."},
        {"startup", 0, Arg::Required, "mode", "The startup mode of the service: auto or manual (with --register-service)."},
        {"run-service", 0, Arg::Required, "name", "Run as the given service (used by the service control manager).", false, true},
        #endif
    };
}
