// Copyright (c) 2013-2017 Josh Blum
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include <Pothos/Init.hpp>
#include <Pothos/Remote.hpp>
#include <Pothos/Util/Network.hpp>
#include "Net/TcpServer.hpp"
#include "Net/Uri.hpp"
#include "Platform/Process.hpp"
#include <cstdio>
#include <iostream>

/***********************************************************************
 * Spawn TCP proxy server given server URI
 *  - every connection runs a remote handler on its own thread
 *  - in require active mode, the process terminates after the last
 *    connection closes
 **********************************************************************/
void PothosUtilBase::proxyServer(const std::string &, const std::string &uriStr)
{
    //remove automatic flushing from iostreams
    //only flushes on newlines and intentional flushes
    std::cout << std::nounitbuf;
    std::cerr << std::nounitbuf;
    std::clog << std::nounitbuf;

    //fully buffered IO backs up and is not acceptable for logging
    //set stdio to be line buffered which is useful for logging
    //on windows, line buffering is not supported, use unbuffered
    #ifdef _MSC_VER
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    #else
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    #endif

    Pothos::ScopedInit init;

    //parse the URI
    const std::string defaultUri = "tcp://"+Pothos::Util::getWildcardAddr(Pothos::RemoteServer::getLocatorPort());
    const auto uri = Pothos::Net::parseUri(uriStr.empty()?defaultUri:uriStr);
    if (uri.scheme != "tcp")
    {
        throw Pothos::Exception("PothosUtil::proxyServer("+uriStr+")", "unsupported URI scheme");
    }

    //create the server
    const bool requireActive = this->config().hasOption("requireActive");
    Pothos::Net::TcpServer server(uri.host, uri.port,
        [](Pothos::Net::TcpStream &stream)
        {
            Pothos::RemoteHandler handler(stream.peerHost());
            handler.runHandler(stream, stream);
        },
        [requireActive](const std::size_t active)
        {
            if (active != 0 or not requireActive) return;
            std::cerr << "Proxy server: No active connections - terminating" << std::endl;
            Pothos::Platform::requestSelfTermination();
        });

    std::cout << "Host: " << server.host() << std::endl;
    std::cout << "Port: " << server.port() << std::endl;

    //wait here until the term signal is received
    this->waitForTerminationRequest();
}
