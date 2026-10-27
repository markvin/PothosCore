// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//The shared Asio io_context of the process (internal, not installed)

#pragma once
#include <asio/io_context.hpp>

namespace Pothos {
namespace Net {

/*!
 * The io_context shared by the network layer (accept, datagrams, timeouts).
 * It runs on one background thread, started on first use and stopped when
 * the library unloads. Handlers must not block: blocking work (a Remote
 * connection) runs on its own thread.
 */
asio::io_context &ioContext(void);

} //namespace Net
} //namespace Pothos
