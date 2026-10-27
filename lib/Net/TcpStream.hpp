// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Blocking TCP connection as a std::iostream (internal, not installed)

#pragma once
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace Pothos {
namespace Net {

/*!
 * A connected TCP socket with 64 KiB buffers in each direction and TCP_NODELAY.
 * I/O errors and end of stream set the stream state like file streams do.
 */
class TcpStream : public std::iostream
{
public:
    /*!
     * Resolve host and connect, the whole operation bounded by timeout.
     * \throws Pothos::TimeoutException when the timeout expires
     * \throws Pothos::IOException when the host cannot be resolved or refuses
     */
    static std::unique_ptr<TcpStream> connect(const std::string &host, std::uint16_t port, std::chrono::microseconds timeout);

    ~TcpStream(void) override;

    //! The peer address without brackets and port (IPv4-mapped IPv6 as IPv4)
    std::string peerHost(void) const;

    /*!
     * Shut the connection down in both directions: a read blocked in another
     * thread returns end of stream. The socket is released by the destructor
     * (on Windows it is closed here, the only way to wake that read).
     */
    void close(void);

    struct Impl;
    explicit TcpStream(std::unique_ptr<Impl> impl); //!< used by TcpServer

private:
    std::unique_ptr<Impl> _impl;
};

} //namespace Net
} //namespace Pothos
