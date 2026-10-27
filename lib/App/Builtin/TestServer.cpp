// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/Paths.hpp>
#include "Platform/Process.hpp"
#include "Platform/Environment.hpp"
#include "Util/StringUtils.hpp"
#include "Util/TempFile.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "App/Server.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace Pothos::Platform;
using namespace std::chrono_literals;

namespace {

struct Launched
{
    Process proc;
    Pipe out, err;
};

std::unique_ptr<Launched> launchPothosUtil(const std::vector<std::string> &args)
{
    auto l = std::make_unique<Launched>();
    ProcessOptions opts;
    opts.args = args;
    opts.out = &l->out;
    opts.err = &l->err;
    l->proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    return l;
}

//! Read stdout lines until "Port: N"
std::string readPort(Pipe &out)
{
    PipeInputStream is(out);
    std::string line;
    while (std::getline(is, line))
    {
        const auto tok = Pothos::Util::Detail::split(line, " ", true, false);
        if (tok.size() >= 2 and tok[0] == "Port:") return tok[1];
    }
    return "";
}

std::string readAll(Pipe &pipe)
{
    PipeInputStream is(pipe);
    return std::string(std::istreambuf_iterator<char>(is), std::istreambuf_iterator<char>());
}

std::optional<int> waitFor(Process &proc, const std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::optional<int> code;
    while (not (code = proc.tryWait()) and std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(20ms);
    if (not code) proc.kill();
    return code;
}

} //namespace

POTHOS_TEST_BLOCK("/app/tests", test_app_proxy_server_terminates)
{
    //the termination request of Process (as RemoteServer sends it)
    {
        auto l = launchPothosUtil({"--proxy-server=tcp://127.0.0.1:0"});
        POTHOS_TEST_TRUE(not readPort(l->out).empty());
        l->proc.requestTermination();
        const auto code = waitFor(l->proc, 5s);
        POTHOS_TEST_TRUE(bool(code));
        POTHOS_TEST_EQUAL(*code, 0);
    }
    #ifndef _WIN32
    //Ctrl-C
    {
        auto l = launchPothosUtil({"--proxy-server=tcp://127.0.0.1:0"});
        POTHOS_TEST_TRUE(not readPort(l->out).empty());
        POTHOS_TEST_EQUAL(::kill(l->proc.id(), SIGINT), 0);
        const auto code = waitFor(l->proc, 5s);
        POTHOS_TEST_TRUE(bool(code));
        POTHOS_TEST_EQUAL(*code, 0);
    }
    #endif
}

POTHOS_TEST_BLOCK("/app/tests", test_app_pothosutil_cli)
{
    {
        auto l = launchPothosUtil({"--help"});
        const auto out = readAll(l->out);
        const auto code = waitFor(l->proc, 10s);
        POTHOS_TEST_TRUE(code and *code == 0);
        POTHOS_TEST_TRUE(out.find("--self-tests") != std::string::npos);
        POTHOS_TEST_TRUE(out.find("--proxy-server") != std::string::npos);
    }
    {
        auto l = launchPothosUtil({"--no-such-option"});
        const auto err = readAll(l->err);
        const auto code = waitFor(l->proc, 10s);
        POTHOS_TEST_TRUE(code and *code == 64);
        POTHOS_TEST_TRUE(err.find("--no-such-option") != std::string::npos);
    }
    {
        auto l = launchPothosUtil({"--var=bad", "--system-info"});
        readAll(l->out);
        const auto code = waitFor(l->proc, 10s);
        POTHOS_TEST_TRUE(code and *code == 64);
    }
    {
        auto l = launchPothosUtil({"--load-module", "/no/such/module.so", "--success-code", "7"});
        readAll(l->out);
        const auto code = waitFor(l->proc, 30s);
        POTHOS_TEST_TRUE(code and *code != 0 and *code != 7);
    }
}

POTHOS_TEST_BLOCK("/app/tests", test_app_daemon_pidfile)
{
    #ifdef _WIN32
    std::cout << "skipped: Unix only" << std::endl;
    #else
    Pothos::Util::Detail::TempFile pidFile(false);
    auto l = launchPothosUtil({"--daemon", "--umask=077", "--pidfile=" + pidFile.path().string(), "--proxy-server=tcp://127.0.0.1:0"});
    const auto parentCode = waitFor(l->proc, 10s);
    POTHOS_TEST_TRUE(parentCode and *parentCode == 0);

    //the daemon writes its pid
    pid_t pid = 0;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (pid == 0 and std::chrono::steady_clock::now() < deadline)
    {
        std::ifstream file(pidFile.path());
        if (not (file >> pid)) pid = 0;
        if (pid == 0) std::this_thread::sleep_for(20ms);
    }
    POTHOS_TEST_TRUE(pid > 0);
    POTHOS_TEST_EQUAL(::kill(pid, 0), 0); //alive

    //the umask applies to the files it creates: the pid file itself
    struct stat st{};
    POTHOS_TEST_EQUAL(::stat(pidFile.path().c_str(), &st), 0);
    POTHOS_TEST_EQUAL(int(st.st_mode & 077), 0);

    //terminate: the process goes away and removes the pid file
    POTHOS_TEST_EQUAL(::kill(pid, SIGTERM), 0);
    const auto end = std::chrono::steady_clock::now() + 5s;
    while ((::kill(pid, 0) == 0 or std::filesystem::exists(pidFile.path())) and std::chrono::steady_clock::now() < end)
    {
        std::this_thread::sleep_for(20ms);
    }
    POTHOS_TEST_TRUE(::kill(pid, 0) != 0);
    POTHOS_TEST_TRUE(not std::filesystem::exists(pidFile.path()));
    #endif
}

#ifdef _WIN32
namespace {

//! The state of a service, 0 when it cannot be queried
DWORD serviceState(SC_HANDLE service)
{
    SERVICE_STATUS status{};
    return QueryServiceStatus(service, &status)? status.dwCurrentState : 0;
}

bool waitServiceState(SC_HANDLE service, const DWORD state, const std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (serviceState(service) != state)
    {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(100ms);
    }
    return true;
}

} //namespace
#endif

POTHOS_TEST_BLOCK("/app/tests", test_app_windows_service)
{
    #ifndef _WIN32
    std::cout << "skipped: Windows only" << std::endl;
    #else
    const SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (scm == nullptr)
    {
        std::cout << "skipped: not elevated (cannot open the service control manager)" << std::endl;
        return;
    }
    CloseServiceHandle(scm);

    Pothos::App::ServiceOptions opts;
    opts.name = "PothosUtilTest" + std::to_string(GetCurrentProcessId());
    opts.displayName = "Pothos self-test service";
    opts.description = "Registered and removed by test_app_windows_service";
    opts.args = {"--proxy-server=tcp://127.0.0.1:0"};
    Pothos::App::registerService(opts);

    bool started = false, stopped = false;
    {
        const SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        const auto wideName = std::wstring(opts.name.begin(), opts.name.end());
        const SC_HANDLE service = OpenServiceW(manager, wideName.c_str(), SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (service != nullptr)
        {
            if (StartServiceW(service, 0, nullptr)) started = waitServiceState(service, SERVICE_RUNNING, 30s);
            SERVICE_STATUS status{};
            if (started and ControlService(service, SERVICE_CONTROL_STOP, &status)) stopped = waitServiceState(service, SERVICE_STOPPED, 30s);
            CloseServiceHandle(service);
        }
        CloseServiceHandle(manager);
    }
    Pothos::App::unregisterService(opts.name); //always clean up
    POTHOS_TEST_TRUE(started);
    POTHOS_TEST_TRUE(stopped);

    //gone after unregistering
    const SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    const auto wideName = std::wstring(opts.name.begin(), opts.name.end());
    const SC_HANDLE service = OpenServiceW(manager, wideName.c_str(), SERVICE_QUERY_STATUS);
    const bool gone = service == nullptr or serviceState(service) == 0 or serviceState(service) == SERVICE_STOPPED;
    if (service != nullptr) CloseServiceHandle(service);
    CloseServiceHandle(manager);
    POTHOS_TEST_TRUE(gone);
    #endif
}

POTHOS_TEST_BLOCK("/app/tests", helper_sleep)
{
    //a long command that is not a server mode (launched by test_app_signal_interrupts_commands)
    if (Pothos::Platform::getEnv("POTHOS_TEST_SLEEP") != std::optional<std::string>("1")) return;
    std::cout << "sleeping" << std::endl;
    std::this_thread::sleep_for(30s);
}

POTHOS_TEST_BLOCK("/app/tests", test_app_signal_interrupts_commands)
{
    #ifdef _WIN32
    std::cout << "skipped: Unix only" << std::endl;
    #else
    //Ctrl-C ends commands that do not wait for a termination request
    auto l = std::make_unique<Launched>();
    ProcessOptions opts;
    opts.args = {"--self-test1=/app/tests/helper_sleep"};
    opts.env = {{"POTHOS_TEST_SLEEP", "1"}};
    opts.out = &l->out;
    l->proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    {
        PipeInputStream is(l->out);
        std::string line;
        while (std::getline(is, line) and line.find("sleeping") == std::string::npos) {}
    }
    POTHOS_TEST_EQUAL(::kill(l->proc.id(), SIGINT), 0);
    const auto code = waitFor(l->proc, 5s);
    POTHOS_TEST_TRUE(bool(code)); //not still sleeping
    POTHOS_TEST_TRUE(*code != 0);
    #endif
}

POTHOS_TEST_BLOCK("/app/tests", test_app_std_exception_exit_code)
{
    //a command failing with a standard exception (filesystem_error) prints it and exits 64
    auto l = launchPothosUtil({"--generate-simd-dispatchers=/no/such/dispatchers.json", "--simd-arches=sse2", "--output=/no/such/out.hpp"});
    const auto err = readAll(l->err);
    const auto code = waitFor(l->proc, 30s);
    POTHOS_TEST_TRUE(code and *code == 64);
    POTHOS_TEST_TRUE(err.find("dispatchers.json") != std::string::npos);
}

POTHOS_TEST_BLOCK("/app/tests", test_app_daemon_with_threads)
{
    #ifdef _WIN32
    std::cout << "skipped: Unix only" << std::endl;
    #else
    //threads that start while the library loads (syslog forwarding, the async logger)
    //must not keep the daemon from terminating
    Pothos::Util::Detail::TempFile pidFile(false);
    auto l = std::make_unique<Launched>();
    ProcessOptions opts;
    opts.args = {"--daemon", "--pidfile=" + pidFile.path().string(), "--proxy-server=tcp://127.0.0.1:0"};
    opts.env = {{"POTHOS_SYSLOG_ADDR", "127.0.0.1:9"}, {"POTHOS_LOG_LEVEL", "bogus"}};
    opts.out = &l->out;
    opts.err = &l->err;
    l->proc = Process::launch(Pothos::System::getPothosUtilExecutablePath(), opts);
    POTHOS_TEST_TRUE(waitFor(l->proc, 10s).value_or(-1) == 0);

    pid_t pid = 0;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (pid == 0 and std::chrono::steady_clock::now() < deadline)
    {
        std::ifstream file(pidFile.path());
        if (not (file >> pid)) pid = 0;
        if (pid == 0) std::this_thread::sleep_for(20ms);
    }
    POTHOS_TEST_TRUE(pid > 0);
    POTHOS_TEST_EQUAL(::kill(pid, SIGTERM), 0);
    const auto end = std::chrono::steady_clock::now() + 5s;
    while (::kill(pid, 0) == 0 and std::chrono::steady_clock::now() < end) std::this_thread::sleep_for(20ms);
    const bool exited = ::kill(pid, 0) != 0;
    if (not exited) ::kill(pid, SIGKILL);
    POTHOS_TEST_TRUE(exited);
    #endif
}
