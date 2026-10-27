// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Server modes of PothosUtil: termination requests, daemon, pid file (internal, not installed)

#pragma once
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Pothos {
namespace App {

/*!
 * Catch termination requests from now on (idempotent): SIGINT, SIGTERM and
 * SIGQUIT on Unix; Ctrl-C, Ctrl-Break, console close and the named event
 * Platform::terminationEventName(pid) on Windows. Call it early so that a
 * request that arrives before waitForTerminationRequest() is not lost.
 */
void installTerminationHandlers(void);

//! Block until a termination request arrives (installs the handlers)
void waitForTerminationRequest(void);

#ifndef _WIN32
/*!
 * Start exe with args as a daemon: a new session, stdin/stdout/stderr on
 * /dev/null. The new process starts from exec, so threads that the library
 * started in this process (logger, network) do not haunt it as they would
 * after a plain fork. The caller (the original process) then exits.
 * \throws Pothos::SystemException when fork fails
 */
void spawnDaemon(const std::filesystem::path &exe, const std::vector<std::string> &args);
#endif

#ifdef _WIN32
struct ServiceOptions
{
    std::string name = "PothosUtil";
    std::string displayName; //!< the name when empty
    std::string description;
    std::string startup = "manual"; //!< "auto" or "manual"
    std::vector<std::string> args; //!< options of the service process (after --run-service=NAME)
};

/*!
 * Register this executable as a Windows service started as
 * "<exe>" --run-service=NAME <args>.
 * \throws Pothos::SystemException with the Win32 error (access denied without elevation)
 */
void registerService(const ServiceOptions &opts);

//! \throws Pothos::SystemException with the Win32 error
void unregisterService(const std::string &name);

/*!
 * Run main as the service NAME under the service control manager: the
 * service reports running, a stop or shutdown request becomes a termination
 * request (waitForTerminationRequest returns), the exit code of main is reported.
 * \throws Pothos::SystemException when not started by the service control manager
 */
int runAsService(const std::string &name, std::function<int()> main);
#endif

//! Writes the process id to a file, removes the file on destruction
class PidFile
{
public:
    //! \throws Pothos::FileException when the file cannot be written
    explicit PidFile(std::filesystem::path path);
    ~PidFile(void);
    PidFile(const PidFile &) = delete;
    PidFile &operator=(const PidFile &) = delete;

private:
    std::filesystem::path _path;
};

} //namespace App
} //namespace Pothos
