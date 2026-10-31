///
/// \file System/Logger.hpp
///
/// Logging and logger control.
///
/// \copyright
/// Copyright (c) 2014-2015 Josh Blum
///                    2026 Pothos contributors
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Config.hpp>
#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace Pothos {
namespace System {

/*!
 * Log message severity, most severe first.
 * The values match the syslog-style priorities of the log forwarding.
 */
enum class LogLevel : int
{
    Fatal = 1,
    Critical,
    Error,
    Warning,
    Notice,
    Information,
    Debug,
    Trace,
};

/*!
 * One log message, as the sink and the log listeners get it.
 */
struct LogRecord
{
    //! The severity of the message
    LogLevel level = LogLevel::Information;

    //! The time when the message was logged
    std::chrono::system_clock::time_point time;

    //! The name of the logging component, for example "Pothos.Topology"
    std::string source;

    //! The text of the message
    std::string text;

    //! A hash of the id of the thread that logged the message (0 when unknown)
    unsigned long threadId = 0;

    //! The id of the process that logged the message (0 when unknown)
    int pid = 0;
};

/*!
 * A log listener gets each log record that is written.
 * Refer to Logger::addListener() for the rules.
 */
using LogListener = std::function<void(const LogRecord &)>;

/*!
 * Logger class contains static methods to log messages
 * and to deal with log configuration.
 */
class POTHOS_API Logger
{
public:
    /*!
     * Log a message. The message is plain text, not a format string.
     * Messages at Fatal and Critical level are written before this returns;
     * other messages may be written asynchronously (see flush()).
     * \param level the severity of the message
     * \param source the name of the logging component, for example "Pothos.Topology"
     * \param message the text of the message
     */
    static void log(const LogLevel level, const std::string &source, const std::string &message);

    //! Would a message at this level from this source be logged?
    static bool isEnabled(const LogLevel level, const std::string &source);

    /*!
     * Set the lowest level that is logged.
     * \param source a source name, or empty for the default of every source
     */
    static void setLevel(const LogLevel level, const std::string &source = "");

    //! The lowest level that is logged for a source (empty: the default)
    static LogLevel getLevel(const std::string &source = "");

    //! Parse a level name (fatal ... trace, any case) or number (1 ... 8)
    static std::optional<LogLevel> parseLevel(std::string_view name);

    //! Wait until every message logged so far has been written
    static void flush(void);

    /*!
     * Add a listener that gets each log record that is written.
     *
     * The rules:
     * - The listener gets each record that goes to the sink: the records
     *   that pass the level filter, and the records that the syslog
     *   listener gets from other processes. It gets them also when the
     *   sink is empty (POTHOS_LOG_CHANNEL=null) or when syslog forwarding
     *   replaced the sink.
     * - The listeners are called one at a time, in the order of the
     *   records, from the thread that writes the record (the writer
     *   thread in asynchronous mode). A listener must return quickly.
     * - The logger catches an exception from a listener and ignores it.
     * - A message that a listener logs goes only to the sink, not to the
     *   listeners. Thus a listener that logs causes no deadlock and no
     *   infinite recursion.
     * - After flush() returns, the listeners got all records that were
     *   logged before the call. A flush() in a listener does not wait.
     * - Do not call setupDefaultLogging() or the syslog forwarding
     *   functions in a listener: a change of the sink in a listener
     *   throws std::logic_error.
     * - A listener must not wait for another thread, and must not take a
     *   lock that another thread can hold while it logs: a deadlock can
     *   occur.
     * - Remove the listener before its data is destroyed, also before
     *   the process exits.
     *
     * \param listener the function to call for each record
     * \return an identifier for removeListener(), never 0
     */
    static std::size_t addListener(LogListener listener);

    /*!
     * Remove a listener that addListener() added.
     * After this call returns, the logger does not call the listener
     * again, and no call of it continues in another thread.
     * Thus the owner can delete the data of the listener after this call.
     * Exception: when a listener calls this function, the change applies
     * after the current record, thus the removed listener can still get
     * that record. An unknown identifier does nothing.
     * \param id the identifier from addListener()
     */
    static void removeListener(const std::size_t id);

    /*!
     * Start a listener for syslog messages from other processes.
     * The log messages will be forwarded to the default logger.
     * \return the port that this process is listening
     */
    static std::string startSyslogListener(void);

    /*!
     * Stop the syslog listener service.
     */
    static void stopSyslogListener(void);

    /*!
     * Start syslog forwarding to the given address.
     * \param addr the log destination in host:port format
     */
    static void startSyslogForwarding(const std::string &addr);

    /*!
     * Stop all syslog forwarding setup by startSyslogForwarding().
     */
    static void stopSyslogForwarding(void);

    /*!
     * Redirect standard IO to the logging facility.
     * This allows the user to capture standard IO debug messages
     * from subprocesses and servers that would otherwise be lost.
     * Because the default logging output directs to standard IO,
     * it is recommended to first change the default logging channel.
     * For example, startSyslogForwarding() will direct logs to UDP.
     * \param source the repoted source of the forwarded log messages
     */
    static void forwardStdIoToLogging(const std::string &source);

    /*!
     * Setup default logging for this process.
     * The default logger will log all non-informational logs to the console.
     * The logger can be configured with the following environment vars:
     * POTHOS_LOG_LEVEL - what level to display logs (default notice)
     * POTHOS_LOG_CHANNEL - how to display the logs (default color)
     * POTHOS_LOG_FILE - log file path if POTHOS_LOG_CHANNEL=file specified
     */
    static void setupDefaultLogging(void);

private:
    //! private constructor: we don't make Logger instances
    Logger(void){}
};

/*!
 * Log a message formatted with std::format.
 * The message is only formatted when the level is enabled for the source.
 */
template <typename... Args>
void log(const LogLevel level, const std::string &source, std::format_string<Args...> fmt, Args &&...args)
{
    if (not Logger::isEnabled(level, source)) return;
    Logger::log(level, source, std::format(fmt, std::forward<Args>(args)...));
}

} //namespace System
} //namespace Pothos

/*!
 * Logging macros: POTHOS_LOG_ERROR("My.Source", "value {} out of range", value);
 * The arguments after the source are a std::format format string and its arguments.
 */
#define POTHOS_LOG_FATAL(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Fatal, source, __VA_ARGS__)
#define POTHOS_LOG_CRITICAL(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Critical, source, __VA_ARGS__)
#define POTHOS_LOG_ERROR(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Error, source, __VA_ARGS__)
#define POTHOS_LOG_WARNING(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Warning, source, __VA_ARGS__)
#define POTHOS_LOG_NOTICE(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Notice, source, __VA_ARGS__)
#define POTHOS_LOG_INFORMATION(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Information, source, __VA_ARGS__)
#define POTHOS_LOG_DEBUG(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Debug, source, __VA_ARGS__)
#define POTHOS_LOG_TRACE(source, ...) ::Pothos::System::log(::Pothos::System::LogLevel::Trace, source, __VA_ARGS__)
