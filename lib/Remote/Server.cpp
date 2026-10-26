// Copyright (c) 2013-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Remote.hpp>
#include <Pothos/System/Paths.hpp>
#include "Platform/Process.hpp"
#include "System/LoggerSinks.hpp"
#include <optional>
#include "Util/StringUtils.hpp"
#include <Poco/URI.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Message.h>
#include <Poco/Net/RemoteSyslogChannel.h>
#include <functional>
#include <thread>
#include <cassert>

std::string Pothos::RemoteServer::getLocatorPort(void)
{
    return "16415";
}

struct Pothos::RemoteServer::Impl
{
    Impl(const Pothos::Platform::Process &ph):
        ph(ph)
    {
        return;
    }

    ~Impl(void)
    {
        client = RemoteClient(); //reset
        ph.requestTermination();
        //the forwarding threads see end of stream once the server exits
        if (outThread.joinable()) outThread.join();
        if (errThread.joinable()) errThread.join();
        outPipe.close();
        errPipe.close();
        ph.wait();
    }

    Pothos::Platform::Process ph;
    std::string actualPort;
    RemoteClient client;
    std::string uriStr;

    //members used for syslog forwarding
    Pothos::Platform::Pipe outPipe, errPipe;
    std::thread outThread, errThread;
};

Pothos::RemoteServer::RemoteServer(void)
{
    assert(not *this);
}

Pothos::RemoteServer::RemoteServer(const std::string &uriStr, const bool closePipes)
{
    //validate the URI first
    if (not uriStr.empty()) POTHOS_EXCEPTION_TRY
    {
        Poco::URI uri(uriStr);
        if (uri.getScheme() != "tcp") throw InvalidArgumentException("unsupported URI scheme");
    }
    POTHOS_EXCEPTION_CATCH(const Exception &ex)
    {
        throw RemoteServerError("Pothos::RemoteServer("+uriStr+")", ex);
    }

    //create args
    std::vector<std::string> args;
    args.push_back("--require-active");
    args.push_back("--proxy-server="+uriStr);

    //launch
    Pothos::Platform::Pipe outPipe, errPipe;
    Pothos::Platform::ProcessOptions opts;
    opts.args = args;
    opts.out = &outPipe;
    opts.err = &errPipe;
    std::optional<Pothos::Platform::Process> ph;
    std::string launchError;
    try
    {
        ph = Pothos::Platform::Process::launch(System::getPothosUtilExecutablePath(), opts);
    }
    catch (const Pothos::SystemException &ex)
    {
        launchError = ex.displayText();
    }
    if (not ph)
    {
        std::string errMsg = "Failed to launch server process: ";
        errMsg += System::getPothosUtilExecutablePath();
        for (const auto &arg : args) errMsg += " " + arg;
        throw Pothos::RemoteServerError("Pothos::RemoteServer("+uriStr+")", errMsg + ": " + launchError);
    }
    _impl.reset(new Impl(*ph));
    _impl->uriStr = uriStr;

    //read back the port from the server
    Pothos::Platform::PipeInputStream is(outPipe);
    while (is.good() and not is.eof())
    {
        std::string line; std::getline(is, line);
        if (line.empty()) continue;
        const auto tok = Pothos::Util::Detail::split(line, " ", true, false);
        if (tok.size() >= 2 and tok[0] == "Port:")
        {
            _impl->actualPort = tok[1];
            break;
        }
    }
    const bool readPortError = _impl->actualPort.empty();

    //close pipes to not overfill and backup
    if (closePipes or readPortError)
    {
        outPipe.close();
        errPipe.close();
    }
    //save our pipes to be used with syslog
    else
    {
        _impl->outPipe = outPipe;
        _impl->errPipe = errPipe;
    }

    //An error spawning the process above could have caused a problem,
    //which can be detected when the parsing of the port fails.
    if (readPortError)
    {
        _impl.reset(); //kills process
        std::string errMsg;

        //collect error message
        try
        {
            Pothos::Platform::PipeInputStream ies(errPipe);
            errMsg = std::string(
                std::istreambuf_iterator<char>(ies),
                std::istreambuf_iterator<char>());
        }
        catch (...)
        {
            errMsg = "Failed to read the server process error output";
        }

        throw Pothos::RemoteServerError("Pothos::RemoteServer("+uriStr+")", errMsg);
    }

    //Try to connect to the server.
    //Store an open connection within this server wrapper.
    {
        Poco::URI uri(uriStr);
        uri.setPort(std::stoul(this->getActualPort()));

        //rewrite wildcard bind addresses into linklocal addresses
        Poco::Net::SocketAddress serverAddr(uri.getHost(), uri.getPort());
        if (serverAddr.host().isWildcard()) switch (serverAddr.family())
        {
        case Poco::Net::IPAddress::IPv4: uri.setHost("127.0.0.1"); break;
        case Poco::Net::IPAddress::IPv6: uri.setHost("::1"); break;
        default: break;
        }

        _impl->client = RemoteClient(uri.toString());
    }
}

const std::string &Pothos::RemoteServer::getUri(void) const
{
    assert(_impl);
    return _impl->uriStr;
}

Pothos::RemoteServer::operator bool(void) const
{
    return bool(_impl);
}

std::string Pothos::RemoteServer::getActualPort(void) const
{
    assert(_impl);
    return _impl->actualPort;
}

/***********************************************************************
 * syslog forwarding implementation
 **********************************************************************/

//! Guess the priority using keywords in the front of the message.
static Poco::Message::Priority guessPrio(const std::string &msg)
{
    //the level numbers are the syslog priorities
    return Poco::Message::Priority(int(Pothos::System::Detail::guessLevel(msg)));
}

//! Worker thread operation to read the pipe and forward to the syslog channel
static void pipeToSyslogWorker(Poco::AutoPtr<Poco::Channel> channel, Pothos::Platform::Pipe pipe, const std::string &source)
{
    Pothos::Platform::PipeInputStream is(pipe);
    try
    {
        std::string line;
        while (is.good())
        {
            std::getline(is, line);
            if (line.empty()) continue;
            channel->log(Poco::Message(source, line, guessPrio(line)));
        }
    }
    catch (...){}
}

//! Create the syslog channel and forwarding threads
void Pothos::RemoteServer::startSyslogForwarding(const std::string &addr, const std::string &source)
{
    assert(_impl);
    Poco::AutoPtr<Poco::Channel> channel(new Poco::Net::RemoteSyslogChannel(addr, ""/*empty name*/));
    _impl->outThread = std::thread(std::bind(&pipeToSyslogWorker, channel, _impl->outPipe, source));
    _impl->errThread = std::thread(std::bind(&pipeToSyslogWorker, channel, _impl->errPipe, source));
}

#include <Pothos/Managed.hpp>

static auto managedRemoteServer = Pothos::ManagedClass()
    .registerConstructor<Pothos::RemoteServer>()
    .registerConstructor<Pothos::RemoteServer, std::string>()
    .registerConstructor<Pothos::RemoteServer, std::string, bool>()
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::RemoteServer, getActualPort))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::RemoteServer, startSyslogForwarding))
    .commit("Pothos/RemoteServer");
