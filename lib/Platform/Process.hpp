// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Internal platform layer: child processes and anonymous pipes
//(not installed, not part of the public API)

#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <streambuf>
#include <string>
#include <vector>

namespace Pothos {
namespace Platform {

/*!
 * An anonymous pipe. Copies share the same pipe, like a handle.
 * Both ends are closed when the last copy is destroyed.
 */
class Pipe
{
public:
    Pipe(void);

    //! Read up to len bytes; returns 0 at end of stream (write end closed)
    size_t read(char *buf, const size_t len);

    //! Write all len bytes; returns len
    size_t write(const char *buf, const size_t len);

    void closeRead(void);
    void closeWrite(void);
    void close(void);

    //! Native handles (file descriptor or HANDLE), -1 when closed
    intptr_t readHandle(void) const;
    intptr_t writeHandle(void) const;

    struct Impl;
private:
    std::shared_ptr<Impl> _impl;
};

namespace Detail {

class PipeInBuf : public std::streambuf
{
public:
    explicit PipeInBuf(const Pipe &pipe): _pipe(pipe) {}
protected:
    int_type underflow(void) override
    {
        if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
        const auto n = _pipe.read(_buf, sizeof(_buf));
        if (n == 0) return traits_type::eof();
        setg(_buf, _buf, _buf+n);
        return traits_type::to_int_type(*gptr());
    }
private:
    Pipe _pipe;
    char _buf[4096];
};

class PipeOutBuf : public std::streambuf
{
public:
    explicit PipeOutBuf(const Pipe &pipe): _pipe(pipe) {setp(_buf, _buf+sizeof(_buf));}
    ~PipeOutBuf(void) override {this->sync();}
protected:
    int_type overflow(int_type ch) override
    {
        if (this->sync() != 0) return traits_type::eof();
        if (not traits_type::eq_int_type(ch, traits_type::eof()))
        {
            *pptr() = traits_type::to_char_type(ch);
            pbump(1);
        }
        return traits_type::not_eof(ch);
    }
    int sync(void) override
    {
        const auto n = size_t(pptr()-pbase());
        if (n != 0) try {_pipe.write(pbase(), n);} catch (...) {return -1;}
        setp(_buf, _buf+sizeof(_buf));
        return 0;
    }
private:
    Pipe _pipe;
    char _buf[4096];
};

} //namespace Detail

//! Read a pipe as an input stream
class PipeInputStream : public std::istream
{
public:
    explicit PipeInputStream(const Pipe &pipe): std::istream(nullptr), _buf(pipe) {this->rdbuf(&_buf);}
private:
    Detail::PipeInBuf _buf;
};

//! Write a pipe as an output stream
class PipeOutputStream : public std::ostream
{
public:
    explicit PipeOutputStream(const Pipe &pipe): std::ostream(nullptr), _buf(pipe) {this->rdbuf(&_buf);}
private:
    Detail::PipeOutBuf _buf;
};

struct ProcessOptions
{
    //! Arguments after the executable path
    std::vector<std::string> args;

    //! Variables added to (or replacing in) the inherited environment
    std::map<std::string, std::string> env;

    //! Pipes for standard streams; nullptr inherits the parent's stream
    Pipe *in = nullptr;
    Pipe *out = nullptr;
    Pipe *err = nullptr;
};

/*!
 * A child process. Copies refer to the same process.
 * Destruction does not terminate the process.
 */
class Process
{
public:
    /*!
     * Start a process. The executable is looked up in PATH when it has no
     * directory part. After the launch, the parent's copies of the child's
     * pipe ends are closed (so the parent sees end of stream when the child exits).
     * \throws Pothos::SystemException when the process cannot be started
     */
    static Process launch(const std::filesystem::path &exe, const ProcessOptions &opts);

    int id(void) const;

    //! Wait for the process and return its exit code (128 + signal when killed by a signal)
    int wait(void);

    //! The exit code when the process has finished, otherwise nullopt
    std::optional<int> tryWait(void);

    //! Ask the process to terminate (SIGTERM on Unix)
    void requestTermination(void);

    //! Terminate the process immediately
    void kill(void);

    struct Impl;
private:
    std::shared_ptr<Impl> _impl;
};

/*!
 * Build a Windows command line from an executable and arguments with the
 * quoting rules that CommandLineToArgvW and the MSVC runtime expect.
 * Available on every platform for testing.
 */
std::wstring buildWindowsCommandLine(const std::filesystem::path &exe, const std::vector<std::string> &args);

/*!
 * Name of the Windows event that asks PothosUtil to terminate (it waits for
 * it in server modes): "PothosUtilTerminate" + the pid as 8 upper case hex digits.
 */
std::wstring terminationEventName(const int pid);

/*!
 * Ask this process to terminate the way Process::requestTermination asks a
 * child: SIGTERM on Unix, the named termination event on Windows (immediate
 * exit when no one waits for it). Used by the proxy server in require-active mode.
 */
void requestSelfTermination(void);

//! UTF-8 to wide string (UTF-16 on Windows, UTF-32 elsewhere)
std::wstring utf8ToWide(const std::string &s);

} //namespace Platform
} //namespace Pothos
