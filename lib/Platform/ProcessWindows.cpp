// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/Process.hpp"
#include <Pothos/Exception.hpp>
#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <mutex>
#include <windows.h>

static std::string lastErrorText(void)
{
    const DWORD err = GetLastError();
    char *msg = nullptr;
    const DWORD n = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    std::string text = (n == 0 or msg == nullptr) ? "error " + std::to_string(err) : std::string(msg, n);
    if (msg != nullptr) LocalFree(msg);
    while (not text.empty() and (text.back() == '\n' or text.back() == '\r')) text.pop_back();
    return text;
}

/***********************************************************************
 * Pipe handles are created non-inheritable. During a launch, the child's
 * ends are made inheritable under this lock and closed right after, so
 * concurrent launches never leak each other's pipe ends.
 **********************************************************************/
static std::mutex &spawnMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

/***********************************************************************
 * Pipe
 **********************************************************************/
struct Pothos::Platform::Pipe::Impl
{
    HANDLE handles[2] = {nullptr, nullptr};

    ~Impl(void)
    {
        for (auto &h : handles) if (h != nullptr) CloseHandle(h);
    }

    void closeEnd(const int i)
    {
        if (handles[i] != nullptr) CloseHandle(handles[i]);
        handles[i] = nullptr;
    }
};

Pothos::Platform::Pipe::Pipe(void):
    _impl(std::make_shared<Impl>())
{
    if (not CreatePipe(&_impl->handles[0], &_impl->handles[1], nullptr, 0))
        throw Pothos::SystemException("Pothos::Platform::Pipe()", lastErrorText());
}

size_t Pothos::Platform::Pipe::read(char *buf, const size_t len)
{
    const HANDLE h = _impl->handles[0];
    if (h == nullptr) return 0;
    DWORD n = 0;
    if (ReadFile(h, buf, DWORD(len), &n, nullptr)) return size_t(n);
    const DWORD err = GetLastError();
    if (err == ERROR_BROKEN_PIPE or err == ERROR_INVALID_HANDLE) return 0; //end of stream
    throw Pothos::SystemException("Pothos::Platform::Pipe::read()", lastErrorText());
}

size_t Pothos::Platform::Pipe::write(const char *buf, const size_t len)
{
    const HANDLE h = _impl->handles[1];
    if (h == nullptr) throw Pothos::SystemException("Pothos::Platform::Pipe::write()", "write end closed");
    size_t done = 0;
    while (done < len)
    {
        DWORD n = 0;
        if (not WriteFile(h, buf+done, DWORD(len-done), &n, nullptr))
            throw Pothos::SystemException("Pothos::Platform::Pipe::write()", lastErrorText());
        done += n;
    }
    return len;
}

void Pothos::Platform::Pipe::closeRead(void)
{
    _impl->closeEnd(0);
}

void Pothos::Platform::Pipe::closeWrite(void)
{
    _impl->closeEnd(1);
}

void Pothos::Platform::Pipe::close(void)
{
    this->closeRead();
    this->closeWrite();
}

intptr_t Pothos::Platform::Pipe::readHandle(void) const
{
    return _impl->handles[0] == nullptr ? -1 : reinterpret_cast<intptr_t>(_impl->handles[0]);
}

intptr_t Pothos::Platform::Pipe::writeHandle(void) const
{
    return _impl->handles[1] == nullptr ? -1 : reinterpret_cast<intptr_t>(_impl->handles[1]);
}

/***********************************************************************
 * Process
 **********************************************************************/
struct Pothos::Platform::Process::Impl
{
    HANDLE process = nullptr;
    DWORD pid = 0;

    ~Impl(void)
    {
        if (process != nullptr) CloseHandle(process);
    }
};

//environment block: inherited variables, replaced or extended by env
static std::wstring makeEnvironmentBlock(const std::map<std::string, std::string> &env)
{
    std::map<std::wstring, std::wstring> vars; //key: upper case name
    std::map<std::wstring, std::wstring> names;
    const auto upper = [](std::wstring s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](const wchar_t c){return wchar_t(std::towupper(c));});
        return s;
    };
    wchar_t *block = GetEnvironmentStringsW();
    for (const wchar_t *p = block; p != nullptr and *p != L'\0'; p += wcslen(p)+1)
    {
        const std::wstring entry(p);
        const auto eq = entry.find(L'=', 1); //names like "=C:" start with '='
        if (eq == std::wstring::npos) continue;
        const auto name = entry.substr(0, eq);
        vars[upper(name)] = entry.substr(eq+1);
        names[upper(name)] = name;
    }
    if (block != nullptr) FreeEnvironmentStringsW(block);
    for (const auto &pair : env)
    {
        const auto name = Pothos::Platform::utf8ToWide(pair.first);
        vars[upper(name)] = Pothos::Platform::utf8ToWide(pair.second);
        names[upper(name)] = name;
    }
    std::wstring out;
    for (const auto &pair : vars) out += names[pair.first] + L"=" + pair.second + L'\0';
    out += L'\0';
    return out;
}

Pothos::Platform::Process Pothos::Platform::Process::launch(const std::filesystem::path &exe, const ProcessOptions &opts)
{
    auto cmdLine = buildWindowsCommandLine(exe, opts.args);
    std::wstring envBlock;
    if (not opts.env.empty()) envBlock = makeEnvironmentBlock(opts.env);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = (opts.in != nullptr)? reinterpret_cast<HANDLE>(opts.in->readHandle()) : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = (opts.out != nullptr)? reinterpret_cast<HANDLE>(opts.out->writeHandle()) : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = (opts.err != nullptr)? reinterpret_cast<HANDLE>(opts.err->writeHandle()) : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION pi{};
    BOOL ok = FALSE;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(spawnMutex());
        for (const HANDLE h : {si.hStdInput, si.hStdOutput, si.hStdError})
        {
            if (h != nullptr and h != INVALID_HANDLE_VALUE) SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        }
        ok = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
            envBlock.empty()? 0 : CREATE_UNICODE_ENVIRONMENT,
            envBlock.empty()? nullptr : envBlock.data(), nullptr, &si, &pi);
        if (not ok) error = lastErrorText();
        //only the pipe ends need their inheritance cleared again
        if (opts.in != nullptr) SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, 0);
        if (opts.out != nullptr) SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, 0);
        if (opts.err != nullptr) SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, 0);
    }

    //the child owns its ends now: close the parent's copies
    if (opts.in != nullptr) opts.in->closeRead();
    if (opts.out != nullptr) opts.out->closeWrite();
    if (opts.err != nullptr) opts.err->closeWrite();

    if (not ok) throw Pothos::SystemException("Pothos::Platform::Process::launch("+exe.string()+")", error);

    CloseHandle(pi.hThread);
    auto impl = std::make_shared<Impl>();
    impl->process = pi.hProcess;
    impl->pid = pi.dwProcessId;
    Process proc;
    proc._impl = impl;
    return proc;
}

int Pothos::Platform::Process::id(void) const
{
    return int(_impl->pid);
}

int Pothos::Platform::Process::wait(void)
{
    if (WaitForSingleObject(_impl->process, INFINITE) != WAIT_OBJECT_0)
        throw Pothos::SystemException("Pothos::Platform::Process::wait()", lastErrorText());
    DWORD code = 0;
    GetExitCodeProcess(_impl->process, &code);
    return int(code);
}

std::optional<int> Pothos::Platform::Process::tryWait(void)
{
    if (WaitForSingleObject(_impl->process, 0) != WAIT_OBJECT_0) return std::nullopt;
    DWORD code = 0;
    GetExitCodeProcess(_impl->process, &code);
    return int(code);
}

void Pothos::Platform::Process::requestTermination(void)
{
    if (this->tryWait()) return;
    //PothosUtil waits for the named event PothosUtilTerminate<pid>;
    //fall back to terminating the process
    const auto name = terminationEventName(int(_impl->pid));
    const HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
    if (event != nullptr)
    {
        SetEvent(event);
        CloseHandle(event);
        return;
    }
    TerminateProcess(_impl->process, 1);
}

void Pothos::Platform::Process::kill(void)
{
    if (not this->tryWait()) TerminateProcess(_impl->process, 1);
}

void Pothos::Platform::requestSelfTermination(void)
{
    const auto name = terminationEventName(int(GetCurrentProcessId()));
    const HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
    if (event != nullptr)
    {
        SetEvent(event);
        CloseHandle(event);
        return;
    }
    std::_Exit(1); //no termination handler is waiting
}
