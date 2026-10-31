// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal platform layer: process environment and host information
//(not installed, not part of the public API)

#pragma once
#include <optional>
#include <string>
#include <vector>

namespace Pothos {
namespace Platform {

//! The value of an environment variable, or nullopt when it is not set
std::optional<std::string> getEnv(const std::string &name);

//! Set an environment variable (visible to getenv and to child processes)
void setEnv(const std::string &name, const std::string &value);

//! Remove an environment variable
void unsetEnv(const std::string &name);

//! Operating system name: "Linux", "Darwin", "Windows NT", ...
std::string osName(void);

//! Operating system version (kernel release on Unix)
std::string osVersion(void);

//! Hardware architecture: "x86_64", "arm64", "AMD64", ...
std::string osArchitecture(void);

//! Host name of this machine
std::string nodeName(void);

/*!
 * A stable identifier of this machine: the MAC address (xx:xx:xx:xx:xx:xx)
 * of the first non-loopback network adapter in name order,
 * or the host name when there is no such adapter.
 */
std::string nodeId(void);

//! Number of logical processors (at least 1)
unsigned processorCount(void);

//! Identifier of the current process
int currentProcessId(void);

/*!
 * The command line arguments of this process after the program name, as
 * UTF-8. On Windows, argv uses the ANSI code page; this function reads the
 * wide command line instead. On Unix, it copies argv.
 */
std::vector<std::string> commandLineArguments(int argc, char *argv[]);

} //namespace Platform
} //namespace Pothos
