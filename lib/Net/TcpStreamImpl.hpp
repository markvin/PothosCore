// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Implementation of TcpStream shared with TcpServer (internal)

#pragma once
#include "Net/TcpStream.hpp"
#include <asio/ip/tcp.hpp>
#include <streambuf>
#include <vector>

namespace Pothos {
namespace Net {

//! A std::streambuf over a blocking TCP socket
class SocketBuf : public std::streambuf
{
public:
    static constexpr std::size_t BufferSize = 64*1024;

    explicit SocketBuf(asio::ip::tcp::socket &socket);

protected:
    int_type underflow(void) override;
    int_type overflow(int_type ch) override;
    std::streamsize xsputn(const char *s, std::streamsize n) override;
    int sync(void) override;

private:
    bool flushOut(void);
    bool writeAll(const char *data, std::size_t size);

    asio::ip::tcp::socket &_socket;
    std::vector<char> _in, _out;
};

struct TcpStream::Impl
{
    explicit Impl(asio::ip::tcp::socket &&s):
        socket(std::move(s)),
        buf(socket)
    {
        return;
    }

    asio::ip::tcp::socket socket;
    SocketBuf buf;
};

//! Address text without IPv4-mapped IPv6 decoration
std::string addressText(const asio::ip::address &addr);

} //namespace Net
} //namespace Pothos
