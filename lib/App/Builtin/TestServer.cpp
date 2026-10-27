// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/Paths.hpp>
#include "Platform/Process.hpp"
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
#ifndef _WIN32
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
