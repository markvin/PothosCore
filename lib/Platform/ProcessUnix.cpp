// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Platform/Process.hpp"
#include <Pothos/Exception.hpp>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <crt_externs.h> //a shared library on macOS gets the environment from _NSGetEnviron()
static char **processEnviron(void) {return *_NSGetEnviron();}
#else
extern char **environ;
static char **processEnviron(void) {return environ;}
#endif

/***********************************************************************
 * Pipes and spawning share one lock: on systems without pipe2(),
 * a pipe created in one thread must not leak into a child spawned by
 * another thread before its close-on-exec flag is set.
 **********************************************************************/
static std::mutex &fdMutex(void)
{
    static std::mutex mutex;
    return mutex;
}

static std::string errnoText(const int err)
{
    return std::strerror(err);
}

/***********************************************************************
 * Pipe
 **********************************************************************/
struct Pothos::Platform::Pipe::Impl
{
    int fds[2] = {-1, -1};

    ~Impl(void)
    {
        for (auto &fd : fds) if (fd >= 0) ::close(fd);
    }

    void closeEnd(const int i)
    {
        if (fds[i] >= 0) ::close(fds[i]);
        fds[i] = -1;
    }
};

Pothos::Platform::Pipe::Pipe(void):
    _impl(std::make_shared<Impl>())
{
    std::lock_guard<std::mutex> lock(fdMutex());
    #ifdef __linux__
    const int ret = ::pipe2(_impl->fds, O_CLOEXEC);
    #else
    const int ret = ::pipe(_impl->fds);
    if (ret == 0) for (const int fd : _impl->fds) ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    #endif
    if (ret != 0) throw Pothos::SystemException("Pothos::Platform::Pipe()", errnoText(errno));
}

size_t Pothos::Platform::Pipe::read(char *buf, const size_t len)
{
    const int fd = _impl->fds[0];
    if (fd < 0) return 0;
    while (true)
    {
        const auto n = ::read(fd, buf, len);
        if (n >= 0) return size_t(n);
        if (errno == EINTR) continue;
        if (errno == EBADF) return 0; //closed underneath us
        throw Pothos::SystemException("Pothos::Platform::Pipe::read()", errnoText(errno));
    }
}

size_t Pothos::Platform::Pipe::write(const char *buf, const size_t len)
{
    const int fd = _impl->fds[1];
    if (fd < 0) throw Pothos::SystemException("Pothos::Platform::Pipe::write()", "write end closed");
    size_t done = 0;
    while (done < len)
    {
        const auto n = ::write(fd, buf+done, len-done);
        if (n >= 0) {done += size_t(n); continue;}
        if (errno == EINTR) continue;
        throw Pothos::SystemException("Pothos::Platform::Pipe::write()", errnoText(errno));
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
    return _impl->fds[0];
}

intptr_t Pothos::Platform::Pipe::writeHandle(void) const
{
    return _impl->fds[1];
}

/***********************************************************************
 * Process
 **********************************************************************/
struct Pothos::Platform::Process::Impl
{
    pid_t pid = -1;
    std::optional<int> exitCode;
    std::mutex mutex;
};

static int decodeStatus(const int status)
{
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
}

Pothos::Platform::Process Pothos::Platform::Process::launch(const std::filesystem::path &exe, const ProcessOptions &opts)
{
    //argument vector: exe followed by args
    std::vector<std::string> argStrs;
    argStrs.push_back(exe.string());
    argStrs.insert(argStrs.end(), opts.args.begin(), opts.args.end());
    std::vector<char *> argv;
    for (auto &arg : argStrs) argv.push_back(arg.data());
    argv.push_back(nullptr);

    //environment: inherited variables, replaced or extended by opts.env
    std::vector<std::string> envStrs;
    for (char **e = processEnviron(); e != nullptr and *e != nullptr; e++)
    {
        const std::string entry(*e);
        const auto name = entry.substr(0, entry.find('='));
        if (opts.env.count(name) == 0) envStrs.push_back(entry);
    }
    for (const auto &pair : opts.env) envStrs.push_back(pair.first + "=" + pair.second);
    std::vector<char *> envp;
    for (auto &entry : envStrs) envp.push_back(entry.data());
    envp.push_back(nullptr);

    //standard stream redirection
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    if (opts.in != nullptr) posix_spawn_file_actions_adddup2(&actions, int(opts.in->readHandle()), STDIN_FILENO);
    if (opts.out != nullptr) posix_spawn_file_actions_adddup2(&actions, int(opts.out->writeHandle()), STDOUT_FILENO);
    if (opts.err != nullptr) posix_spawn_file_actions_adddup2(&actions, int(opts.err->writeHandle()), STDERR_FILENO);
    const bool nullOut = opts.discardOutput and opts.out == nullptr;
    const bool nullErr = opts.discardOutput and opts.err == nullptr;
    if (nullOut) posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    if (nullErr) posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    //the child inherits only stdin, stdout and stderr: other descriptors
    //(sockets and files of other libraries, without close-on-exec) are closed
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    #if defined(__APPLE__)
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_CLOEXEC_DEFAULT);
    if (opts.in == nullptr) posix_spawn_file_actions_addinherit_np(&actions, STDIN_FILENO);
    if (opts.out == nullptr and not nullOut) posix_spawn_file_actions_addinherit_np(&actions, STDOUT_FILENO);
    if (opts.err == nullptr and not nullErr) posix_spawn_file_actions_addinherit_np(&actions, STDERR_FILENO);
    #elif defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO+1);
    #else
    #warning "child processes inherit descriptors without close-on-exec on this platform"
    #endif

    auto impl = std::make_shared<Impl>();
    int ret = 0;
    {
        std::lock_guard<std::mutex> lock(fdMutex());
        ret = ::posix_spawnp(&impl->pid, argStrs.front().c_str(), &actions, &attr, argv.data(), envp.data());
    }
    posix_spawnattr_destroy(&attr);
    posix_spawn_file_actions_destroy(&actions);

    //the child owns its ends now: close the parent's copies
    if (opts.in != nullptr) opts.in->closeRead();
    if (opts.out != nullptr) opts.out->closeWrite();
    if (opts.err != nullptr) opts.err->closeWrite();

    if (ret != 0) throw Pothos::SystemException("Pothos::Platform::Process::launch("+exe.string()+")", errnoText(ret));

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
    //wait for the exit without the lock (WNOWAIT keeps the child to reap):
    //kill() and requestTermination() from another thread take the lock
    {
        std::lock_guard<std::mutex> lock(_impl->mutex);
        if (_impl->exitCode) return *_impl->exitCode;
    }
    siginfo_t info{};
    while (::waitid(P_PID, id_t(_impl->pid), &info, WEXITED | WNOWAIT) < 0)
    {
        if (errno == ECHILD) break; //reaped by another call of this process object
        if (errno != EINTR) throw Pothos::SystemException("Pothos::Platform::Process::wait()", errnoText(errno));
    }

    //reap under the lock (an other thread can have done it already)
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (_impl->exitCode) return *_impl->exitCode;
    int status = 0;
    while (::waitpid(_impl->pid, &status, 0) < 0)
    {
        if (errno != EINTR) throw Pothos::SystemException("Pothos::Platform::Process::wait()", errnoText(errno));
    }
    _impl->exitCode = decodeStatus(status);
    return *_impl->exitCode;
}

std::optional<int> Pothos::Platform::Process::tryWait(void)
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (_impl->exitCode) return _impl->exitCode;
    int status = 0;
    const auto ret = ::waitpid(_impl->pid, &status, WNOHANG);
    if (ret == 0) return std::nullopt;
    if (ret < 0) throw Pothos::SystemException("Pothos::Platform::Process::tryWait()", errnoText(errno));
    _impl->exitCode = decodeStatus(status);
    return _impl->exitCode;
}

void Pothos::Platform::Process::requestTermination(void)
{
    if (not this->tryWait()) ::kill(_impl->pid, SIGTERM);
}

void Pothos::Platform::Process::kill(void)
{
    if (not this->tryWait()) ::kill(_impl->pid, SIGKILL);
}

void Pothos::Platform::requestSelfTermination(void)
{
    ::kill(::getpid(), SIGTERM);
}
