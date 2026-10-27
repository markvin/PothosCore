// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/Server.hpp"
#include "Platform/Process.hpp"
#include <Pothos/Exception.hpp>
#include <fstream>
#include <mutex>
#include <windows.h>

namespace {

//manual reset: every waiter wakes up, a request before the wait is not lost
HANDLE terminationEvent = nullptr;

BOOL WINAPI onConsoleControl(const DWORD type)
{
    switch (type)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        SetEvent(terminationEvent);
        return TRUE;
    default: return FALSE;
    }
}

} //namespace

void Pothos::App::installTerminationHandlers(void)
{
    static std::once_flag once;
    std::call_once(once, []
    {
        const auto name = Pothos::Platform::terminationEventName(int(GetCurrentProcessId()));
        terminationEvent = CreateEventW(nullptr, TRUE, FALSE, name.c_str());
        if (terminationEvent == nullptr) throw Pothos::SystemException("installTerminationHandlers", "CreateEventW failed");
        SetConsoleCtrlHandler(&onConsoleControl, TRUE);
    });
}

void Pothos::App::waitForTerminationRequest(void)
{
    installTerminationHandlers();
    WaitForSingleObject(terminationEvent, INFINITE);
}

Pothos::App::PidFile::PidFile(std::filesystem::path path):
    _path(std::move(path))
{
    std::ofstream file(_path, std::ios::trunc);
    file << GetCurrentProcessId() << std::endl;
    if (not file) throw Pothos::FileException("PidFile", "cannot write " + _path.string());
}

Pothos::App::PidFile::~PidFile(void)
{
    std::error_code ec;
    std::filesystem::remove(_path, ec);
}
