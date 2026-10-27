// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Server modes of PothosUtil: termination requests, daemon, pid file (internal, not installed)

#pragma once
#include <filesystem>
#include <optional>

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
 * Detach like Poco's ServerApplication: fork (the parent exits 0), setsid,
 * umask (027 unless given) and stdin/stdout/stderr on /dev/null.
 * Call it before any thread is started: only the calling thread survives fork.
 * \throws Pothos::SystemException when fork fails
 */
void becomeDaemon(std::optional<unsigned> umaskValue);
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
