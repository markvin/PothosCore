// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/TcpServer.hpp"
#include "Net/TcpStreamImpl.hpp"
#include "Net/Address.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <Pothos/System/Logger.hpp>
#include <asio/ip/v6_only.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <condition_variable>
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

//! Pause before accepting again after an accept error (EMFILE...): no busy loop
constexpr auto AcceptRetryDelay = std::chrono::milliseconds(100);

} //namespace

struct TcpServer::Impl : std::enable_shared_from_this<TcpServer::Impl>
{
    Impl(Handler handler, CountHandler onCountChange):
        acceptor(ioContext()),
        retryTimer(ioContext()),
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
            if (not ec) return self->accept();
            self->retryTimer.expires_after(AcceptRetryDelay);
            self->retryTimer.async_wait([self](const asio::error_code &timerEc)
            {
                if (not timerEc) self->accept();
            });
        });
    }

    //! Called with the mutex held; a connection that cannot be started is closed
    void start(asio::ip::tcp::socket &&socket)
    {
        std::shared_ptr<Connection> conn;
        try
        {
            asio::error_code ec;
            socket.set_option(asio::ip::tcp::no_delay(true), ec);
            conn = std::make_shared<Connection>();
            conn->stream = std::make_unique<TcpStream>(std::make_unique<TcpStream::Impl>(std::move(socket)));

            //the thread waits for the mutex before it can finish: count it after it exists
            auto self = this->shared_from_this();
            conn->thread = std::thread([self, conn]{self->run(conn);});
        }
        catch (const std::exception &ex)
        {
            POTHOS_LOG_ERROR("Pothos.Net.TcpServer", "cannot start a connection: {}", ex.what());
            if (conn and conn->stream) conn->stream->close();
            return;
        }
        connections.push_back(conn);
        this->notify(++active);
    }

    //! The connection thread
    void run(const std::shared_ptr<Connection> &conn)
    {
        try {handler(*conn->stream);}
        catch (...) {} //a failing connection must not take the server down
        conn->stream->flush(); //may block on a slow peer: not under the mutex

        //counts are delivered under the mutex: in order, the last one is current
        std::lock_guard<std::mutex> lock(mutex);
        conn->stream->close();
        conn->done = true;
        this->notify(--active);
        doneCond.notify_all();

        //join this thread soon, not at the next accept: the join on the
        //io_context thread waits only for this thread to return
        if (stopping) return; //the destructor joins or detaches the threads
        auto self = this->shared_from_this();
        asio::post(ioContext(), [self]
        {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->reapFinished();
        });
    }

    //! Called with the mutex held: deliveries stay in order
    void notify(const std::size_t count)
    {
        if (not onCountChange) return;
        try {onCountChange(count);}
        catch (const std::exception &ex)
        {
            POTHOS_LOG_ERROR("Pothos.Net.TcpServer", "connection count callback failed: {}", ex.what());
        }
        catch (...)
        {
            POTHOS_LOG_ERROR("Pothos.Net.TcpServer", "connection count callback failed");
        }
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
    asio::steady_timer retryTimer;
    const Handler handler;
    const CountHandler onCountChange;
    std::mutex mutex;
    std::condition_variable doneCond;
    std::list<std::shared_ptr<Connection>> connections;
    std::size_t active = 0;
    bool stopping = false;
    std::string host;
    std::uint16_t port = 0;
};

TcpServer::TcpServer(const std::string &host, const std::uint16_t port, Handler handler, CountHandler onCountChange):
    _impl(std::make_shared<Impl>(std::move(handler), std::move(onCountChange)))
{
    //an empty host is the wildcard address: IPv6 (dual-stack) when the system has it
    const auto wildcard = ipv6Supported()?
        asio::ip::address(asio::ip::address_v6::any()) : asio::ip::address(asio::ip::address_v4::any());
    const auto where = "bind " + formatHostPort(host.empty()? addressText(wildcard) : host, port);
    const auto addr = host.empty()? wildcard : resolveHost(host, std::chrono::seconds(5));
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

    //the acceptor belongs to the io_context thread: close it there,
    //or directly when that thread is gone (process exit)
    bool closedThere = false;
    if (not ioContext().stopped())
    {
        auto closed = std::make_shared<std::promise<void>>(); //outlives a timed out wait
        auto done = closed->get_future();
        asio::post(ioContext(), [impl, closed]
        {
            asio::error_code ec;
            impl->acceptor.close(ec);
            impl->retryTimer.cancel();
            closed->set_value();
        });
        closedThere = done.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    }
    if (not closedThere and ioContext().stopped())
    {
        asio::error_code ec;
        impl->acceptor.close(ec);
        impl->retryTimer.cancel();
    }
    //otherwise the io_context thread is stuck in a callback: the acceptor stays
    //open until it gets there, and the stopping flag ends the accept loop

    //wait a bounded time for the handlers: a handler stuck outside of socket
    //I/O (a long remote call) is detached instead of hanging the caller
    std::unique_lock<std::mutex> lock(impl->mutex);
    impl->doneCond.wait_for(lock, ShutdownGrace, [&impl]
    {
        for (const auto &conn : impl->connections) if (not conn->done) return false;
        return true;
    });
    for (auto &conn : impl->connections)
    {
        if (not conn->thread.joinable()) continue;
        if (conn->done) conn->thread.join(); //it only has to return
        else conn->thread.detach(); //it keeps the connection and the server state alive
    }
    impl->connections.clear();
}

std::string TcpServer::host(void) const
{
    return _impl->host;
}

std::uint16_t TcpServer::port(void) const
{
    return _impl->port;
}

std::size_t TcpServer::threadCount(void) const
{
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->connections.size();
}
