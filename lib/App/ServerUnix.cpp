// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/Server.hpp"
#include <Pothos/Exception.hpp>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

namespace {

//self-pipe: the signal handler (any thread) wakes the waiting thread
int terminationPipe[2] = {-1, -1};

extern "C" void onTerminationSignal(int)
{
    const char byte = 1;
    const auto saved = errno;
    [[maybe_unused]] const auto n = ::write(terminationPipe[1], &byte, 1);
    errno = saved;
}

} //namespace

void Pothos::App::installTerminationHandlers(void)
{
    static std::once_flag once;
    std::call_once(once, []
    {
        if (::pipe(terminationPipe) != 0) throw Pothos::SystemException("installTerminationHandlers", std::strerror(errno));
        for (const int fd : terminationPipe) ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        ::fcntl(terminationPipe[1], F_SETFL, O_NONBLOCK); //a burst of signals never blocks the handler

        struct sigaction sa{};
        sa.sa_handler = &onTerminationSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        for (const int sig : {SIGINT, SIGTERM, SIGQUIT}) ::sigaction(sig, &sa, nullptr);
    });
}

void Pothos::App::waitForTerminationRequest(void)
{
    installTerminationHandlers();
    char byte = 0;
    while (::read(terminationPipe[0], &byte, 1) < 0 and errno == EINTR) {}
}

void Pothos::App::becomeDaemon(const std::optional<unsigned> umaskValue)
{
    const auto pid = ::fork();
    if (pid < 0) throw Pothos::SystemException("becomeDaemon", std::string("cannot fork: ") + std::strerror(errno));
    if (pid != 0) ::_exit(0); //the parent: no destructors, the daemon carries on

    ::setsid();
    ::umask(static_cast<mode_t>(umaskValue.value_or(027)));

    //on /dev/null rather than closed: code that writes to stdout keeps working
    if (std::freopen("/dev/null", "r", stdin) == nullptr or
        std::freopen("/dev/null", "w", stdout) == nullptr or
        std::freopen("/dev/null", "w", stderr) == nullptr)
    {
        throw Pothos::SystemException("becomeDaemon", "cannot attach the standard streams to /dev/null");
    }
}

Pothos::App::PidFile::PidFile(std::filesystem::path path):
    _path(std::move(path))
{
    //written to a temporary name, then renamed: readers never see a partial pid
    auto tmp = _path;
    tmp += ".tmp";
    {
        std::ofstream file(tmp, std::ios::trunc);
        file << ::getpid() << std::endl;
        if (not file) throw Pothos::FileException("PidFile", "cannot write " + tmp.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, _path, ec);
    if (ec) throw Pothos::FileException("PidFile", "cannot write " + _path.string() + ": " + ec.message());
}

Pothos::App::PidFile::~PidFile(void)
{
    std::error_code ec;
    std::filesystem::remove(_path, ec);
}
