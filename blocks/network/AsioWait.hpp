// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <asio/io_context.hpp>
#include <chrono>
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
