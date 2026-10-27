// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <asio/io_context.hpp>
#include <chrono>
#include <algorithm>
#include <system_error>

//! Run the operations started on the context for at most timeout,
//! then cancel what is left on the socket and wait for the cancellation
template <typename Socket>
void runFor(asio::io_context &context, Socket &socket, const std::chrono::nanoseconds timeout)
{
    context.restart();
    context.run_for(timeout);
    if (context.stopped()) return;
    asio::error_code ec;
    socket.cancel(ec);
    context.run();
}

//! Wait until the socket (or acceptor) is readable: false on timeout
template <typename Socket>
bool waitReadable(asio::io_context &context, Socket &socket, const std::chrono::nanoseconds timeout)
{
    bool ready = false;
    socket.async_wait(Socket::wait_read, [&](const asio::error_code &ec){ready = not ec;});
    runFor(context, socket, timeout);
    return ready;
}

/***********************************************************************
 * Wait on the native handle with poll(): unlike a cancelled async_wait it
 * touches no other operation on the socket (on Windows a cancel aborts the
 * I/O of every thread on the handle, a send of another thread included)
 **********************************************************************/
#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#endif

template <typename Socket>
bool pollSocket(Socket &socket, const bool forWrite, const std::chrono::nanoseconds timeout)
{
    using namespace std::chrono;
    const auto ms = int(std::max<long long>(0, duration_cast<milliseconds>(timeout + milliseconds(1) - nanoseconds(1)).count()));
#ifdef _WIN32
    WSAPOLLFD fd{};
    fd.fd = socket.native_handle();
    fd.events = forWrite? POLLWRNORM : POLLRDNORM;
    return ::WSAPoll(&fd, 1, ms) > 0;
#else
    pollfd fd{};
    fd.fd = socket.native_handle();
    fd.events = forWrite? POLLOUT : POLLIN;
    return ::poll(&fd, 1, ms) > 0;
#endif
}
