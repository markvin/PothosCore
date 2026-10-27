// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//TCP server with a thread per connection (internal, not installed)

#pragma once
#include "Net/TcpStream.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Pothos {
namespace Net {

/*!
 * Accepts connections on the shared io_context and runs the handler of each
 * connection on its own thread; the connection closes when the handler returns.
 * The server knows nothing about the protocol spoken over the connection.
 */
class TcpServer
{
public:
    using Handler = std::function<void(TcpStream &)>;

    //! Called with the number of open connections after each open and close
    using CountHandler = std::function<void(std::size_t active)>;

    /*!
     * Bind and listen. Port 0 picks a free port. The IPv6 wildcard "::" also
     * accepts IPv4 connections where the platform allows it.
     * \throws Pothos::IOException when the address cannot be bound
     */
    TcpServer(const std::string &host, std::uint16_t port, Handler handler, CountHandler onCountChange = {});

    //! Stop accepting, close open connections and wait for their handlers
    ~TcpServer(void);

    TcpServer(const TcpServer &) = delete;
    TcpServer &operator=(const TcpServer &) = delete;

    //! The bound address (without brackets)
    std::string host(void) const;

    //! The bound port (the actual port when 0 was requested)
    std::uint16_t port(void) const;

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};

} //namespace Net
} //namespace Pothos
