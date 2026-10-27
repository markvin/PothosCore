// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Net/TcpStreamImpl.hpp"
#include "Net/Address.hpp"
#include "Net/IoContext.hpp"
#include <Pothos/Exception.hpp>
#include <asio/connect.hpp>
#include <asio/post.hpp>
#include <asio/write.hpp>
#include <cstring>
#include <future>

using namespace Pothos::Net;

/***********************************************************************
 * SocketBuf
 **********************************************************************/
SocketBuf::SocketBuf(asio::ip::tcp::socket &socket):
    _socket(socket),
    _in(BufferSize),
    _out(BufferSize)
{
    this->setg(_in.data(), _in.data(), _in.data());
    this->setp(_out.data(), _out.data()+_out.size());
}

SocketBuf::int_type SocketBuf::underflow(void)
{
    if (this->gptr() < this->egptr()) return traits_type::to_int_type(*this->gptr());
    if (not this->flushOut()) return traits_type::eof(); //request/response: send before waiting
    asio::error_code ec;
    const auto n = _socket.read_some(asio::buffer(_in), ec);
    if (ec or n == 0) return traits_type::eof();
    this->setg(_in.data(), _in.data(), _in.data()+n);
    return traits_type::to_int_type(*this->gptr());
}

SocketBuf::int_type SocketBuf::overflow(const int_type ch)
{
    if (not this->flushOut()) return traits_type::eof();
    if (not traits_type::eq_int_type(ch, traits_type::eof()))
    {
        *this->pptr() = traits_type::to_char_type(ch);
        this->pbump(1);
    }
    return traits_type::not_eof(ch);
}

std::streamsize SocketBuf::xsputn(const char *s, const std::streamsize n)
{
    //large writes go straight to the socket instead of through the buffer
    if (n < std::streamsize(_out.size())) return std::streambuf::xsputn(s, n);
    if (not this->flushOut()) return 0;
    return this->writeAll(s, std::size_t(n))? n : 0;
}

int SocketBuf::sync(void)
{
    return this->flushOut()? 0 : -1;
}

bool SocketBuf::flushOut(void)
{
    const auto n = std::size_t(this->pptr() - this->pbase());
    if (n == 0) return true;
    const bool ok = this->writeAll(this->pbase(), n);
    this->setp(_out.data(), _out.data()+_out.size());
    return ok;
}

bool SocketBuf::writeAll(const char *data, const std::size_t size)
{
    asio::error_code ec;
    asio::write(_socket, asio::buffer(data, size), ec);
    return not ec;
}

/***********************************************************************
 * TcpStream
 **********************************************************************/
std::string Pothos::Net::addressText(const asio::ip::address &addr)
{
    if (addr.is_v6() and addr.to_v6().is_v4_mapped())
    {
        return asio::ip::make_address_v4(asio::ip::v4_mapped, addr.to_v6()).to_string();
    }
    return addr.to_string();
}

TcpStream::TcpStream(std::unique_ptr<Impl> impl):
    std::iostream(nullptr),
    _impl(std::move(impl))
{
    this->rdbuf(&_impl->buf);
}

TcpStream::~TcpStream(void)
{
    asio::error_code ec;
    _impl->buf.pubsync();
    _impl->socket.close(ec);
}

std::string TcpStream::peerHost(void) const
{
    asio::error_code ec;
    const auto ep = _impl->socket.remote_endpoint(ec);
    return ec? std::string() : addressText(ep.address());
}

void TcpStream::close(void)
{
    asio::error_code ec;
    _impl->socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    #ifdef _WIN32
    //on Windows shutdown() does not wake a recv() blocked in another thread,
    //only closesocket() does; the reader then sees an error and ends the stream
    _impl->socket.close(ec);
    #endif
}

std::unique_ptr<TcpStream> TcpStream::connect(const std::string &host, const std::uint16_t port, const std::chrono::microseconds timeout)
{
    //like the Poco client: the timeout bounds the lookup and the connect separately
    const auto where = "connect " + formatHostPort(host, port);
    const auto addrs = resolveHostAll(host, timeout);
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    //try the addresses in order (IPv4 first) until one accepts
    asio::error_code lastError;
    for (const auto &addr : addrs)
    {
        asio::ip::tcp::socket socket(ioContext());
        auto done = std::make_shared<std::promise<asio::error_code>>();
        auto result = done->get_future();
        socket.async_connect(asio::ip::tcp::endpoint(addr, port), [done](const asio::error_code &ec){done->set_value(ec);});
        if (result.wait_until(deadline) != std::future_status::ready)
        {
            //abort on the io_context thread, then wait for the handler before the socket goes away
            asio::post(ioContext(), [&socket]{asio::error_code ec; socket.close(ec);});
            result.wait();
            throw Pothos::TimeoutException(where, "timed out");
        }
        lastError = result.get();
        if (lastError) continue;

        asio::error_code optEc;
        socket.set_option(asio::ip::tcp::no_delay(true), optEc);
        return std::make_unique<TcpStream>(std::make_unique<TcpStream::Impl>(std::move(socket)));
    }
    throw Pothos::IOException(where, lastError.message());
}
