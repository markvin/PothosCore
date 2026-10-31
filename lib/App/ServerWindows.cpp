// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/Server.hpp"
#include "Util/FileSystem.hpp"
#include "Platform/Process.hpp"
#include <Pothos/Exception.hpp>
#include <Pothos/System/Logger.hpp>
#include <chrono>
#include <fstream>
#include <mutex>
#include <system_error>
#include <thread>
#include <windows.h>

namespace {

//manual reset: every waiter wakes up, a request before the wait is not lost
HANDLE terminationEvent = nullptr;

//set when main() returns: the close handler waits for it
HANDLE shutdownDoneEvent = nullptr;

//Windows ends the process at most 5 s after a console close
constexpr DWORD CloseWaitMs = 5000;

BOOL WINAPI onConsoleControl(const DWORD type)
{
    switch (type)
    {
    //not LOGOFF/SHUTDOWN: a service receives them whenever any user logs off
    //(SERVICE_CONTROL_SHUTDOWN covers the shutdown of a service)
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        SetEvent(terminationEvent);
        return TRUE;
    case CTRL_CLOSE_EVENT:
        //Windows ends the process when the handler returns: wait for a clean shutdown
        SetEvent(terminationEvent);
        if (shutdownDoneEvent != nullptr) WaitForSingleObject(shutdownDoneEvent, CloseWaitMs);
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
        shutdownDoneEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        SetConsoleCtrlHandler(&onConsoleControl, TRUE);
    });
}

void Pothos::App::shutdownDone(void)
{
    if (shutdownDoneEvent != nullptr) SetEvent(shutdownDoneEvent);
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
    if (not file) throw Pothos::FileException("PidFile", "cannot write " + Pothos::Util::Detail::pathUtf8(_path));
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
    auto text = std::system_category().message(int(error));
    if (error == ERROR_ACCESS_DENIED) text += " (run PothosUtil as an administrator)";
    throw Pothos::SystemException(what, text);
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

//the stop request (dispatcher thread) and the end of main (service thread)
//both report: no report may come after SERVICE_STOPPED
std::mutex statusMutex;
bool reportedStopped = false;

void reportState(const DWORD state, const int exitCode = 0)
{
    std::lock_guard<std::mutex> lock(statusMutex);
    if (reportedStopped) return;
    if (state == SERVICE_STOPPED) reportedStopped = true;
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
    //a service has no console: an error goes to the log, the exit code is
    //the one of PothosUtil for errors (64)
    constexpr int ExitError = 64;
    serviceExitCode = ExitError;
    try {serviceExitCode = serviceMainFunction();}
    catch (const Pothos::Exception &ex)
    {
        POTHOS_LOG_ERROR("PothosUtil.Service", "{}", ex.displayText());
    }
    catch (const std::exception &ex)
    {
        POTHOS_LOG_ERROR("PothosUtil.Service", "{}", ex.what());
    }
    catch (...)
    {
        POTHOS_LOG_ERROR("PothosUtil.Service", "unknown exception");
    }
    Pothos::System::Logger::flush();
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
    ScHandle service(OpenServiceW(scm.handle, wideName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
    if (service.handle == nullptr) throwLastError("unregisterService(" + name + ")");

    //DeleteService only marks a running service: stop it first (at most 10 s)
    SERVICE_STATUS status{};
    if (QueryServiceStatus(service.handle, &status) and status.dwCurrentState != SERVICE_STOPPED)
    {
        ControlService(service.handle, SERVICE_CONTROL_STOP, &status); //on an error, the delete still marks it
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (QueryServiceStatus(service.handle, &status) and status.dwCurrentState != SERVICE_STOPPED and
            std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
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
