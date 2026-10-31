// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "System/LoggerSinks.hpp"
#include "Util/FileSystem.hpp"
#include <Pothos/Exception.hpp>
#include "Util/StringUtils.hpp"
#include <cstdio>
#include <ctime>
#include <format>
#include <fstream>
#include <memory>
#ifdef _WIN32
#include "Platform/Process.hpp" //utf8ToWide
#include <io.h> //_isatty
#include <windows.h>
#else
#include <unistd.h> //isatty
#endif

using namespace Pothos::System;

std::string_view Detail::levelName(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::Fatal: return "Fatal";
    case LogLevel::Critical: return "Critical";
    case LogLevel::Error: return "Error";
    case LogLevel::Warning: return "Warning";
    case LogLevel::Notice: return "Notice";
    case LogLevel::Information: return "Information";
    case LogLevel::Debug: return "Debug";
    case LogLevel::Trace: return "Trace";
    }
    return "Unknown";
}

LogLevel Detail::guessLevel(std::string_view message)
{
    const auto searchStr = Pothos::Util::Detail::toLower(message.substr(0, 25));
    if (searchStr.find("fatal") != std::string::npos) return LogLevel::Fatal;
    if (searchStr.find("critical") != std::string::npos) return LogLevel::Critical;
    if (searchStr.find("error") != std::string::npos) return LogLevel::Error;
    if (searchStr.find("warning") != std::string::npos) return LogLevel::Warning;
    if (searchStr.find("debug") != std::string::npos) return LogLevel::Debug;
    if (searchStr.find("trace") != std::string::npos) return LogLevel::Trace;
    return LogLevel::Information;
}

std::string Detail::formatLogRecord(const LogRecord &r, std::string_view pattern, const bool utc)
{
    const auto t = std::chrono::system_clock::to_time_t(r.time);
    std::tm tm{};
    #ifdef _WIN32
    if (utc) gmtime_s(&tm, &t); else localtime_s(&tm, &t);
    #else
    if (utc) gmtime_r(&t, &tm); else localtime_r(&t, &tm);
    #endif
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(r.time.time_since_epoch()).count() % 1000;

    std::string out;
    for (size_t i = 0; i < pattern.size(); i++)
    {
        if (pattern[i] != '%' or i+1 == pattern.size())
        {
            out.push_back(pattern[i]);
            continue;
        }
        const char token = pattern[++i];
        switch (token)
        {
        case 'Y': out += std::format("{:04}", tm.tm_year+1900); break;
        case 'm': out += std::format("{:02}", tm.tm_mon+1); break;
        case 'd': out += std::format("{:02}", tm.tm_mday); break;
        case 'H': out += std::format("{:02}", tm.tm_hour); break;
        case 'M': out += std::format("{:02}", tm.tm_min); break;
        case 'S': out += std::format("{:02}", tm.tm_sec); break;
        case 'i': out += std::format("{:03}", ms < 0 ? ms+1000 : ms); break;
        case 's': out += r.source; break;
        case 't': out += r.text; break;
        case 'p': out += levelName(r.level); break;
        case 'q': out += levelName(r.level).front(); break;
        case 'P': out += std::to_string(r.pid); break;
        case 'T': out += std::to_string(r.threadId); break;
        case '%': out += '%'; break;
        default: out += '%'; out += token; break;
        }
    }
    return out;
}

/***********************************************************************
 * Console: written with C stdio to stderr, so that forwarding the
 * standard iostreams into the logger can never feed back into it.
 **********************************************************************/
static bool stderrIsTerminal(void)
{
    #ifdef _WIN32
    if (not _isatty(_fileno(stderr))) return false;
    //enable ANSI escape sequences on the Windows console
    const HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (not GetConsoleMode(h, &mode)) return false;
    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    #else
    return isatty(fileno(stderr)) != 0;
    #endif
}

static const char *levelColor(const LogLevel level)
{
    switch (level)
    {
    case LogLevel::Fatal:
    case LogLevel::Critical: return "\033[1;31m"; //bold red
    case LogLevel::Error: return "\033[31m"; //red
    case LogLevel::Warning: return "\033[33m"; //yellow
    case LogLevel::Debug:
    case LogLevel::Trace: return "\033[90m"; //gray
    default: return nullptr;
    }
}

Detail::LogSink Detail::makeChannelSink(const std::string &channel, const std::string &filePath, const std::string &pattern, const bool utc)
{
    if (channel == "console" or channel == "color")
    {
        const bool color = (channel == "color") and stderrIsTerminal();
        return [pattern, utc, color](const LogRecord &r)
        {
            const auto line = formatLogRecord(r, pattern, utc);
            const char *esc = color ? levelColor(r.level) : nullptr;
            const auto text = (esc != nullptr)? std::string(esc) + line + "\033[0m\n" : line + "\n";
            #ifdef _WIN32
            //a Windows console shows UTF-8 bytes in its own code page: write UTF-16
            //(files and pipes get the UTF-8 bytes)
            const HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
            DWORD mode = 0;
            if (handle != INVALID_HANDLE_VALUE and GetConsoleMode(handle, &mode))
            {
                std::fflush(stderr);
                const auto wide = Pothos::Platform::utf8ToWide(text);
                DWORD written = 0;
                WriteConsoleW(handle, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
                return;
            }
            #endif
            std::fputs(text.c_str(), stderr);
            std::fflush(stderr);
        };
    }
    if (channel == "file" and not filePath.empty())
    {
        auto file = std::make_shared<std::ofstream>(Pothos::Util::Detail::utf8Path(filePath), std::ios::app);
        if (not file->is_open()) throw Pothos::FileException("Pothos logger", "cannot open the log file " + filePath);
        return [file, pattern, utc](const LogRecord &r)
        {
            *file << formatLogRecord(r, pattern, utc) << '\n' << std::flush;
        };
    }
    return LogSink(); //null or unknown channel
}
