// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/Server.hpp"
#include "Platform/Process.hpp"
#include <Pothos/Exception.hpp>
#include <fstream>
#include <mutex>
#include <system_error>
#include <windows.h>

namespace {

//manual reset: every waiter wakes up, a request before the wait is not lost
HANDLE terminationEvent = nullptr;

BOOL WINAPI onConsoleControl(const DWORD type)
{
    switch (type)
    {
    //not LOGOFF/SHUTDOWN: a service receives them whenever any user logs off
    //(SERVICE_CONTROL_SHUTDOWN covers the shutdown of a service)
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
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

/***********************************************************************
 * Windows service
 **********************************************************************/
namespace {

[[noreturn]] void throwLastError(const std::string &what)
{
    const auto error = GetLastError();
    throw Pothos::SystemException(what, std::system_category().message(int(error)));
}

//! Closes a service control manager handle
struct ScHandle
{
    explicit ScHandle(SC_HANDLE h): handle(h) {}
    ~ScHandle(void) {if (handle != nullptr) CloseServiceHandle(handle);}
    ScHandle(const ScHandle &) = delete;
    ScHandle &operator=(const ScHandle &) = delete;
    SC_HANDLE handle;
};

std::wstring serviceName;
std::function<int()> serviceMainFunction;
SERVICE_STATUS_HANDLE statusHandle = nullptr;
int serviceExitCode = 0;

void reportState(const DWORD state, const int exitCode = 0)
{
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = (state == SERVICE_RUNNING)? (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN) : 0;
    status.dwWin32ExitCode = (exitCode == 0)? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    status.dwServiceSpecificExitCode = DWORD(exitCode);
    status.dwWaitHint = (state == SERVICE_STOP_PENDING)? 10000 : 0;
    SetServiceStatus(statusHandle, &status);
}

DWORD WINAPI onServiceControl(const DWORD control, DWORD, LPVOID, LPVOID)
{
    switch (control)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportState(SERVICE_STOP_PENDING);
        SetEvent(terminationEvent);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE: return NO_ERROR;
    default: return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    statusHandle = RegisterServiceCtrlHandlerExW(serviceName.c_str(), &onServiceControl, nullptr);
    if (statusHandle == nullptr) return;
    reportState(SERVICE_RUNNING);
    serviceExitCode = 1;
    try {serviceExitCode = serviceMainFunction();}
    catch (...) {}
    reportState(SERVICE_STOPPED, serviceExitCode);
}

} //namespace

void Pothos::App::registerService(const ServiceOptions &opts)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (scm.handle == nullptr) throwLastError("registerService(" + opts.name + ")");

    std::wstring exe(MAX_PATH, L'\0');
    while (true)
    {
        const auto n = GetModuleFileNameW(nullptr, exe.data(), DWORD(exe.size()));
        if (n == 0) throwLastError("registerService(" + opts.name + ")");
        if (n < exe.size()) {exe.resize(n); break;}
        exe.resize(exe.size()*2);
    }
    std::vector<std::string> args{"--run-service=" + opts.name};
    args.insert(args.end(), opts.args.begin(), opts.args.end());
    const auto commandLine = Pothos::Platform::buildWindowsCommandLine(exe, args);

    if (opts.startup != "auto" and opts.startup != "manual")
    {
        throw Pothos::InvalidArgumentException("registerService(" + opts.name + ")", "startup must be auto or manual");
    }
    const auto name = Pothos::Platform::utf8ToWide(opts.name);
    const auto displayName = Pothos::Platform::utf8ToWide(opts.displayName.empty()? opts.name : opts.displayName);
    ScHandle service(CreateServiceW(scm.handle, name.c_str(), displayName.c_str(), SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS, (opts.startup == "auto")? SERVICE_AUTO_START : SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL, commandLine.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr));
    if (service.handle == nullptr) throwLastError("registerService(" + opts.name + ")");

    if (not opts.description.empty())
    {
        auto description = Pothos::Platform::utf8ToWide(opts.description);
        SERVICE_DESCRIPTIONW info{description.data()};
        ChangeServiceConfig2W(service.handle, SERVICE_CONFIG_DESCRIPTION, &info);
    }
}

void Pothos::App::unregisterService(const std::string &name)
{
    ScHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (scm.handle == nullptr) throwLastError("unregisterService(" + name + ")");
    const auto wideName = Pothos::Platform::utf8ToWide(name);
    ScHandle service(OpenServiceW(scm.handle, wideName.c_str(), DELETE));
    if (service.handle == nullptr) throwLastError("unregisterService(" + name + ")");
    if (not DeleteService(service.handle)) throwLastError("unregisterService(" + name + ")");
}

int Pothos::App::runAsService(const std::string &name, std::function<int()> main)
{
    installTerminationHandlers();
    serviceName = Pothos::Platform::utf8ToWide(name);
    serviceMainFunction = std::move(main);
    SERVICE_TABLE_ENTRYW table[] = {{serviceName.data(), &serviceMain}, {nullptr, nullptr}};
    if (not StartServiceCtrlDispatcherW(table)) throwLastError("runAsService(" + name + ")");
    return serviceExitCode;
}
