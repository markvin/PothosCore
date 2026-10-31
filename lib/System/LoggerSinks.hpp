// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal logger API: records, sinks and configuration
//(not installed, not part of the public API)

#pragma once
#include <Pothos/System/Logger.hpp>
#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace Pothos {
namespace System {
namespace Detail {

//! The record of the public API (Pothos/System/Logger.hpp)
using LogRecord = Pothos::System::LogRecord;

/*!
 * The destination of log records (console, file, syslog forwarding...).
 * Exactly one sink is active; an empty sink discards records.
 * A sink is only ever called by one thread at a time.
 */
using LogSink = std::function<void(const LogRecord &)>;

//! Replace the active sink (pending records are written to the old one first)
void setLogSink(LogSink sink);

//! The active sink
LogSink getLogSink(void);

//! Write a record to the active sink without level filtering
void deliver(const LogRecord &record);

/*!
 * Format a record with a pattern:
 * %Y year, %m month, %d day, %H hour, %M minute, %S second, %i milliseconds,
 * %s source, %t text, %p level name, %q level letter, %P process id,
 * %T thread id, %% a percent sign. Unknown tokens are kept as is.
 */
std::string formatLogRecord(const LogRecord &record, std::string_view pattern, const bool utc);

//! "Fatal" ... "Trace"
std::string_view levelName(const LogLevel level);

//! Guess the level of free text from keywords in its first characters
LogLevel guessLevel(std::string_view message);

/*!
 * A sink for a POTHOS_LOG_CHANNEL name: "console", "color", "file" (path), "null".
 * Returns an empty sink for "null" or an unknown name.
 */
LogSink makeChannelSink(const std::string &channel, const std::string &filePath, const std::string &pattern, const bool utc);

/*!
 * Configure logging from POTHOS_LOG_LEVEL, POTHOS_LOG_CHANNEL, POTHOS_LOG_FILE,
 * POTHOS_LOG_FORMAT, POTHOS_LOG_TIMEZONE, POTHOS_LOG_ASYNC and POTHOS_SYSLOG_ADDR.
 * An invalid level selects "information" and logs one warning.
 * \param sinkOverride when set, used instead of the configured channel (testing)
 * \param atLibraryLoad true in the static block of the library: on Windows a
 *        host name in POTHOS_SYSLOG_ADDR is not resolved then (a thread does not
 *        start under the loader lock); setupDefaultLogging() resolves it
 */
void configureFromEnvironment(LogSink sinkOverride = nullptr, bool atLibraryLoad = false);

//! Stop the asynchronous writer; later messages are written synchronously
void shutdownLogging(void);

//! Allow the asynchronous writer again after shutdownLogging() (testing)
void restartLogging(void);

} //namespace Detail
} //namespace System
} //namespace Pothos
