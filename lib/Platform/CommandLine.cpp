// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/Process.hpp"
#include <format>

std::wstring Pothos::Platform::utf8ToWide(const std::string &s)
{
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();)
    {
        const auto b0 = static_cast<unsigned char>(s[i]);
        char32_t cp = 0xFFFD;
        size_t len = 1;
        if (b0 < 0x80) cp = b0;
        else if ((b0 >> 5) == 0x6) {cp = b0 & 0x1F; len = 2;}
        else if ((b0 >> 4) == 0xE) {cp = b0 & 0x0F; len = 3;}
        else if ((b0 >> 3) == 0x1E) {cp = b0 & 0x07; len = 4;}
        if (len > 1)
        {
            if (i+len > s.size()) {cp = 0xFFFD; len = 1;}
            else for (size_t j = 1; j < len; j++)
            {
                const auto b = static_cast<unsigned char>(s[i+j]);
                if ((b >> 6) != 0x2) {cp = 0xFFFD; len = 1; break;}
                cp = (cp << 6) | (b & 0x3F);
            }
        }
        i += len;
        if constexpr (sizeof(wchar_t) == 2)
        {
            if (cp >= 0x10000)
            {
                cp -= 0x10000;
                out.push_back(wchar_t(0xD800 + (cp >> 10)));
                out.push_back(wchar_t(0xDC00 + (cp & 0x3FF)));
                continue;
            }
        }
        out.push_back(wchar_t(cp));
    }
    return out;
}

//quote one argument for CommandLineToArgvW / the MSVC runtime
static void appendQuoted(std::wstring &cmd, const std::wstring &arg)
{
    if (not arg.empty() and arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        cmd += arg;
        return;
    }
    cmd.push_back(L'"');
    for (size_t i = 0;; i++)
    {
        size_t backslashes = 0;
        while (i < arg.size() and arg[i] == L'\\') {backslashes++; i++;}
        if (i == arg.size())
        {
            //double the trailing backslashes so the closing quote is not escaped
            cmd.append(backslashes*2, L'\\');
            break;
        }
        if (arg[i] == L'"')
        {
            cmd.append(backslashes*2+1, L'\\');
            cmd.push_back(L'"');
        }
        else
        {
            cmd.append(backslashes, L'\\');
            cmd.push_back(arg[i]);
        }
    }
    cmd.push_back(L'"');
}

std::wstring Pothos::Platform::buildWindowsCommandLine(const std::filesystem::path &exe, const std::vector<std::string> &args)
{
    std::wstring cmd;
    appendQuoted(cmd, utf8ToWide(exe.string()));
    for (const auto &arg : args)
    {
        cmd.push_back(L' ');
        appendQuoted(cmd, utf8ToWide(arg));
    }
    return cmd;
}

std::wstring Pothos::Platform::terminationEventName(const int pid)
{
    return std::format(L"POCOTRM{:08X}", static_cast<unsigned>(pid));
}
