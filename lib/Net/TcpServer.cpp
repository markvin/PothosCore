// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/TcpServer.hpp"
#include "Net/TcpStreamImpl.hpp"
#include "Net/Address.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <asio/ip/v6_only.hpp>
#include <asio/post.hpp>
#include <future>
#include <list>
#include <mutex>
#include <thread>

using namespace Pothos::Net;

namespace {

struct Connection
{
    std::unique_ptr<TcpStream> stream;
    std::thread thread;
    bool done = false;
};

} //namespace

struct TcpServer::Impl : std::enable_shared_from_this<TcpServer::Impl>
{
    Impl(Handler handler, CountHandler onCountChange):
        acceptor(ioContext()),
        handler(std::move(handler)),
        onCountChange(std::move(onCountChange))
    {
        return;
    }

    void accept(void)
    {
        auto self = this->shared_from_this();
        acceptor.async_accept([self](const asio::error_code &ec, asio::ip::tcp::socket socket)
        {
            if (ec == asio::error::operation_aborted) return;
            {
                std::lock_guard<std::mutex> lock(self->mutex);
                if (self->stopping) return;
                self->reapFinished();
                if (not ec) self->start(std::move(socket));
            }
            self->accept();
        });
    }

    //! Called with the mutex held
    void start(asio::ip::tcp::socket &&socket)
    {
        asio::error_code ec;
        socket.set_option(asio::ip::tcp::no_delay(true), ec);
        auto conn = std::make_shared<Connection>();
        conn->stream = std::make_unique<TcpStream>(std::make_unique<TcpStream::Impl>(std::move(socket)));
        connections.push_back(conn);
        const auto active = ++this->active;
        this->notify(active);
        auto self = this->shared_from_this();
        conn->thread = std::thread([self, conn]
        {
            try {self->handler(*conn->stream);}
            catch (...) {} //a failing connection must not take the server down
            std::size_t active = 0;
            {
                std::lock_guard<std::mutex> lock(self->mutex);
                conn->stream->flush();
                conn->stream->close();
                conn->done = true;
                active = --self->active;
            }
            self->notify(active);
        });
    }

    void notify(const std::size_t active)
    {
        if (not onCountChange) return;
        std::lock_guard<std::mutex> lock(notifyMutex);
        onCountChange(active);
    }

    //! Join the threads of finished connections (mutex held)
    void reapFinished(void)
    {
        for (auto it = connections.begin(); it != connections.end();)
        {
            if (not (*it)->done) {++it; continue;}
            if ((*it)->thread.joinable()) (*it)->thread.join();
            it = connections.erase(it);
        }
    }

    asio::ip::tcp::acceptor acceptor;
    const Handler handler;
    const CountHandler onCountChange;
    std::mutex mutex, notifyMutex;
    std::list<std::shared_ptr<Connection>> connections;
    std::size_t active = 0;
    bool stopping = false;
    std::string host;
    std::uint16_t port = 0;
};

TcpServer::TcpServer(const std::string &host, const std::uint16_t port, Handler handler, CountHandler onCountChange):
    _impl(std::make_shared<Impl>(std::move(handler), std::move(onCountChange)))
{
    const auto where = "bind " + formatHostPort(host.empty()? "::" : host, port);
    const auto addr = host.empty()? asio::ip::address(asio::ip::address_v6::any()) : resolveHost(host, std::chrono::seconds(5));
    const asio::ip::tcp::endpoint endpoint(addr, port);
    auto &acceptor = _impl->acceptor;
    asio::error_code ec;
    acceptor.open(endpoint.protocol(), ec);
    if (not ec and addr.is_v6() and addr.is_unspecified())
    {
        asio::error_code v6ec; //dual-stack where available
        acceptor.set_option(asio::ip::v6_only(false), v6ec);
    }
    #ifndef _WIN32
    //on Windows SO_REUSEADDR would let another process take over the port
    if (not ec) acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    #endif
    if (not ec) acceptor.bind(endpoint, ec);
    if (not ec) acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) throw Pothos::IOException(where, ec.message());

    const auto local = acceptor.local_endpoint();
    _impl->host = addressText(local.address());
    _impl->port = local.port();
    _impl->accept();
}

TcpServer::~TcpServer(void)
{
    auto impl = _impl;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->stopping = true;
        for (auto &conn : impl->connections) conn->stream->close();
    }

    //the acceptor belongs to the io_context thread: close it there
    std::promise<void> closed;
    auto done = closed.get_future();
    asio::post(ioContext(), [impl, &closed]
    {
        asio::error_code ec;
        impl->acceptor.close(ec);
        closed.set_value();
    });
    if (done.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
    {
        asio::error_code ec; //the io_context is not running (process exit)
        impl->acceptor.close(ec);
    }

    std::list<std::shared_ptr<Connection>> connections;
    {
        std::lock_guard<std::mutex> lock(impl->mutex);
        connections.swap(impl->connections);
    }
    for (auto &conn : connections)
    {
        if (conn->thread.joinable()) conn->thread.join();
    }
}

std::string TcpServer::host(void) const
{
    return _impl->host;
}

std::uint16_t TcpServer::port(void) const
{
    return _impl->port;
}
