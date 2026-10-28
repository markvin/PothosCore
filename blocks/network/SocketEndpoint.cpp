// Copyright (c) 2014-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SocketEndpoint.hpp"
#include "AsioWait.hpp"
#include "common/ByteOrder.hpp"
#include <Pothos/Util/Network.hpp>
#include <Pothos/Exception.hpp>
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/v6_only.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <mutex>
#include <cassert>
#include <iostream>
#include <algorithm> //min/max

/***********************************************************************
 * The maximum number of bytes passed to a single send() call.
 * Larger buffers will be split into multiple calls to send().
 **********************************************************************/
#define SEND_MTU 4096

/***********************************************************************
 * Ensure that the MSG_MORE flag exists:
 * MSG_MORE hints to send that there is guaranteed additional data.
 * (no hint on Windows: MSG_PARTIAL is not for stream sockets)
 **********************************************************************/
#if defined(_WIN32) || !defined(MSG_MORE)
#undef MSG_MORE
#define MSG_MORE 0
#endif

/***********************************************************************
 * Timeouts of the socket operations: a connection attempt, and the rest
 * of a header or payload that has started to arrive (a stalled peer)
 **********************************************************************/
static const auto ConnectTimeout = std::chrono::seconds(10);
static const auto StallTimeout = std::chrono::seconds(5);

/***********************************************************************
 * TCP socket of the endpoint: one client on a bound endpoint
 **********************************************************************/
struct PothosPacketSocketEndpointTcp
{
    PothosPacketSocketEndpointTcp(const std::string &host, const std::uint16_t port, const bool server):
        server(server),
        connected(false),
        acceptor(context),
        socket(context)
    {
        const asio::ip::tcp::endpoint endpoint(hostAddress(host), port);
        if (server)
        {
            acceptor.open(endpoint.protocol());
            asio::error_code ec;
            if (endpoint.address().is_v6() and endpoint.address().is_unspecified())
            {
                acceptor.set_option(asio::ip::v6_only(false), ec); //dual stack wildcard
            }
            acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.listen(1/*only one client expected*/);
        }
        else
        {
            asio::error_code ec = asio::error::timed_out;
            socket.async_connect(endpoint, [&](const asio::error_code &e){ec = e;});
            runFor(context, socket, ConnectTimeout);
            if (ec) throw asio::system_error(ec, "connect");
            this->setupConnected();
        }
    }

    static asio::ip::address hostAddress(const std::string &host)
    {
        asio::error_code ec;
        const auto addr = asio::ip::make_address(host, ec);
        if (not ec) return addr;
        return asio::ip::make_address(Pothos::Util::resolveHost(host, ConnectTimeout));
    }

    std::string getPort(void) const
    {
        if (server) return std::to_string(acceptor.local_endpoint().port());
        return std::to_string(socket.local_endpoint().port());
    }

    bool isRecvReady(const std::chrono::high_resolution_clock::duration &timeout)
    {
        if (not connected)
        {
            if (not pollSocket(acceptor, false, timeout)) return false;
            acceptor.accept(socket);
            this->setupConnected();
            return false;
        }
        return pollSocket(socket, false, std::chrono::milliseconds(50));
    }

    //! The socket never blocks: the waits below have the stall timeout
    void setupConnected(void)
    {
        socket.set_option(asio::ip::tcp::no_delay(true));
        socket.non_blocking(true);
        connected = true;
    }

    //! Send everything, failing when the peer accepts nothing for the stall timeout
    void sendAll(const void *buff, const size_t length, const int flags)
    {
        size_t sent = 0;
        while (sent < length)
        {
            asio::error_code ec;
            const auto n = socket.send(asio::buffer(static_cast<const char *>(buff)+sent, length-sent), flags, ec);
            if (ec == asio::error::would_block or ec == asio::error::try_again)
            {
                if (not pollSocket(socket, true, StallTimeout)) throw Pothos::IOException(
                    "PothosPacketSocketEndpoint::send()", "the peer stalled (accepted no data for 5 s)");
                continue;
            }
            if (ec) throw Pothos::IOException("PothosPacketSocketEndpoint::send()", ec.message());
            sent += n;
        }
    }

    //! Receive exactly length bytes (the peer has started to send them)
    void recvAll(void *buff, const size_t length)
    {
        size_t received = 0;
        while (received < length) received += this->recvSome(static_cast<char *>(buff)+received, length-received);
    }

    //! Receive what is available, at least one byte, failing when nothing comes for the stall timeout
    size_t recvSome(void *buff, const size_t length)
    {
        while (true)
        {
            asio::error_code ec;
            const auto n = socket.read_some(asio::buffer(buff, length), ec);
            if (ec == asio::error::would_block or ec == asio::error::try_again)
            {
                if (not pollSocket(socket, false, StallTimeout)) throw Pothos::IOException(
                    "PothosPacketSocketEndpoint::recv()", "the peer stalled (no data for 5 s)");
                continue;
            }
            if (ec) throw Pothos::IOException("PothosPacketSocketEndpoint::recv()", ec.message());
            return n;
        }
    }

    //! Stop the connection after an error: both directions, safe while another thread waits on it
    void shutdown(void)
    {
        asio::error_code ec;
        socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    }

    const bool server;
    bool connected;
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor;
    asio::ip::tcp::socket socket;
};

/***********************************************************************
 * Protocol header format
 **********************************************************************/
#define POTHOS_PACKET_WORD32(str) \
    (uint32_t(str[0]) << 24) | \
    (uint32_t(str[1]) << 16) | \
    (uint32_t(str[2]) << 8) | \
    (uint32_t(str[3]) << 0)

static const uint32_t PothosPacketHeaderWord = POTHOS_PACKET_WORD32("PTH2");

#define PothosPacketFlagFin (1 << 0)
#define PothosPacketFlagSyn (1 << 1)
#define PothosPacketFlagRst (1 << 2)
#define PothosPacketFlagPsh (1 << 3)
#define PothosPacketFlagAck (1 << 4)
#define PothosPacketFlagFlo (1 << 5)

struct PothosPacketHeader
{
    uint32_t headerWord;
    uint16_t flags;
    uint16_t type;
    uint32_t payloadBytes;
    uint32_t packetCount;
};

/***********************************************************************
 * States for connection establishment and termination
 **********************************************************************/
enum EndpointState
{
    EP_STATE_LISTEN,
    EP_STATE_SYN_SENT,
    EP_STATE_SYN_RECEIVED,
    EP_STATE_ESTABLISHED,
    EP_STATE_FIN_WAIT_1,
    EP_STATE_FIN_WAIT_2,
    EP_STATE_CLOSE_WAIT,
    EP_STATE_CLOSING,
    EP_STATE_LAST_ACK,
    EP_STATE_TIME_WAIT,
    EP_STATE_CLOSED,
};

/***********************************************************************
 * Private implementation guts
 **********************************************************************/
struct PothosPacketSocketEndpoint::Impl
{
    Impl(void):
        state(EP_STATE_CLOSED),
        lastSentPacketCount(0),
        nextRecvPacketCount(0),
        bytesLeftInStream(0),
        iface(nullptr)
    {
        return;
    }

    //state
    EndpointState state;
    uint32_t lastSentPacketCount;
    uint32_t nextRecvPacketCount;
    size_t bytesLeftInStream;
    uint16_t lastType;
    uint64_t totalBytesRecv;
    uint64_t totalBytesSent;
    uint64_t lastFlowMsgRecv;
    uint64_t lastFlowMsgSent;

    PothosPacketSocketEndpointTcp *iface;

    void unpackHeader(const PothosPacketHeader &header, const size_t recvBytes, uint16_t &flags, uint16_t &type, size_t &payloadBytes);
    void handleState(const uint16_t &flags);
    void send(const uint16_t flags)
    {
        return this->send(flags, 0, nullptr, 0);
    }
    void send(const uint16_t flags, const uint16_t type, const void *buff, const size_t numBytes, const bool more = false);
    void recv(uint16_t &flags, uint16_t &type, Pothos::BufferChunk &buffer, const std::chrono::high_resolution_clock::duration &timeout);

    //! After an error the stream is out of sync: the connection ends
    void fail(void)
    {
        this->state = EP_STATE_CLOSED;
        if (this->iface != nullptr) this->iface->shutdown();
    }

    uint64_t flowControlWindowBytes(void) const
    {
        return 256*1024;
    }

    std::mutex sendMutex;
};

/***********************************************************************
 * endpoint constructor
 **********************************************************************/
PothosPacketSocketEndpoint::PothosPacketSocketEndpoint(const std::string &uri, const std::string &opt):
    _impl(new Impl())
{
    if (opt == "DISCONNECT") return;
    if (opt == "BIND") _impl->state = EP_STATE_LISTEN;
    if (opt == "CONNECT") _impl->state = EP_STATE_CLOSED;
    const auto uriObj = Pothos::Util::parseUri(uri);
    if (uriObj.scheme != "tcp" or (opt != "BIND" and opt != "CONNECT"))
    {
        throw Pothos::InvalidArgumentException("PothosPacketSocketEndpoint("+uri+" -> "+opt+")",
            "unknown URI scheme + opt combo, expects tcp, CONNECT/BIND");
    }
    try
    {
        _impl->iface = new PothosPacketSocketEndpointTcp(uriObj.host, uriObj.port, opt == "BIND");
    }
    catch (const std::system_error &ex)
    {
        delete _impl;
        throw Pothos::RuntimeException("PothosPacketSocketEndpoint("+uri+" -> "+opt+")", ex.what());
    }
}

PothosPacketSocketEndpoint::~PothosPacketSocketEndpoint(void)
{
    try
    {
        this->closeComms();
    }
    catch (...)
    {
        //failure OK, other endpoint may be destructed
    }
    delete _impl->iface;
    delete _impl;
}

std::string PothosPacketSocketEndpoint::getActualPort(void) const
{
    return _impl->iface->getPort();
}

bool PothosPacketSocketEndpoint::isReady(void)
{
    return (_impl->state == EP_STATE_ESTABLISHED) and
        (_impl->lastFlowMsgRecv + _impl->flowControlWindowBytes() > _impl->totalBytesSent);
}

/***********************************************************************
 * initiate open transactions
 **********************************************************************/
void PothosPacketSocketEndpoint::openComms(const std::chrono::high_resolution_clock::duration &timeout)
{
    Pothos::BufferChunk buffer(1024);
    uint16_t type = 0;

    //start with a new random sequence number
    _impl->lastSentPacketCount = uint32_t(std::rand());
    _impl->totalBytesRecv = 0;
    _impl->totalBytesSent = 0;
    _impl->lastFlowMsgRecv = 0;
    _impl->lastFlowMsgSent = 0;

    //initiate connect operation
    if (_impl->state == EP_STATE_CLOSED)
    {
        _impl->send(PothosPacketFlagSyn);
        _impl->state = EP_STATE_SYN_SENT;
    }

    //loop until timeout (we will exit before timeout under normal conditions)
    const auto exitTime = std::chrono::high_resolution_clock::now() + timeout;
    while (std::chrono::high_resolution_clock::now() < exitTime)
    {
        if (_impl->state == EP_STATE_ESTABLISHED) break;
        if (_impl->state == EP_STATE_CLOSED) break;
        this->recv(type, buffer);
    }

    //check the state on loop exit
    if (_impl->state != EP_STATE_ESTABLISHED)
    {
        _impl->state = EP_STATE_CLOSED;
        throw Pothos::RuntimeException("PothosPacketSocketEndpoint::openComms()", "handshake failed");
    }
}

/***********************************************************************
 * initiate close transactions
 **********************************************************************/
void PothosPacketSocketEndpoint::closeComms(const std::chrono::high_resolution_clock::duration &timeout)
{
    if (_impl->state == EP_STATE_CLOSED) return;

    Pothos::BufferChunk buffer(1024);
    uint16_t type = 0;

    //initiate a close operation
    switch (_impl->state)
    {
    case EP_STATE_ESTABLISHED:
        _impl->send(PothosPacketFlagFin);
        _impl->state = EP_STATE_FIN_WAIT_1;
        break;
    case EP_STATE_CLOSE_WAIT:
        _impl->send(PothosPacketFlagFin);
        _impl->state = EP_STATE_LAST_ACK;
        break;
    default:
        _impl->state = EP_STATE_CLOSED;
    }

    //loop until timeout (we will exit before timeout under normal conditions)
    const auto exitTime = std::chrono::high_resolution_clock::now() + timeout;
    while (std::chrono::high_resolution_clock::now() < exitTime)
    {
        if (_impl->state == EP_STATE_CLOSED) break;
        this->recv(type, buffer);
    }

    //the loop above is meant to timeout in the time-wait state
    if (_impl->state == EP_STATE_TIME_WAIT) _impl->state = EP_STATE_CLOSED;

    //check the state on loop exit
    if (_impl->state != EP_STATE_CLOSED)
    {
        _impl->state = EP_STATE_CLOSED;
        throw Pothos::RuntimeException("PothosPacketSocketEndpoint::closeComms()", "handshake failed");
    }
}

/***********************************************************************
 * handle connection state
 **********************************************************************/
void PothosPacketSocketEndpoint::Impl::handleState(const uint16_t &flags)
{
    switch (this->state)
    {
    case EP_STATE_LISTEN:
        if ((flags & PothosPacketFlagSyn) != 0)
        {
            this->send(PothosPacketFlagSyn | PothosPacketFlagAck);
            this->state = EP_STATE_SYN_RECEIVED;
        }
        break;

    case EP_STATE_SYN_SENT:
        if ((flags & (PothosPacketFlagSyn | PothosPacketFlagAck)) != 0)
        {
            this->send(PothosPacketFlagAck);
            this->state = EP_STATE_ESTABLISHED;
        }
        else if ((flags & PothosPacketFlagSyn) != 0)
        {
            this->send(PothosPacketFlagSyn | PothosPacketFlagAck);
            this->state = EP_STATE_SYN_RECEIVED;
        }
        break;

    case EP_STATE_SYN_RECEIVED:
        if ((flags & PothosPacketFlagAck) != 0)
        {
            this->state = EP_STATE_ESTABLISHED;
        }
        break;

    case EP_STATE_ESTABLISHED:
        if ((flags & PothosPacketFlagFin) != 0)
        {
            this->send(PothosPacketFlagAck);
            this->state = EP_STATE_CLOSE_WAIT;
        }
        break;

    case EP_STATE_FIN_WAIT_1:
        if ((flags & PothosPacketFlagAck) != 0)
        {
            this->state = EP_STATE_FIN_WAIT_2;
        }
        else if ((flags & PothosPacketFlagFin) != 0)
        {
            this->send(PothosPacketFlagAck);
            this->state = EP_STATE_CLOSING;
        }
        break;

    case EP_STATE_FIN_WAIT_2:
        if ((flags & PothosPacketFlagFin) != 0)
        {
            this->send(PothosPacketFlagAck);
            this->state = EP_STATE_TIME_WAIT;
        }
        break;

    case EP_STATE_CLOSE_WAIT:
        //handled by close comms
        break;

    case EP_STATE_CLOSING:
        if ((flags & PothosPacketFlagAck) != 0)
        {
            this->state = EP_STATE_TIME_WAIT;
        }
        break;

    case EP_STATE_LAST_ACK:
        if ((flags & PothosPacketFlagAck) != 0)
        {
            this->state = EP_STATE_CLOSED;
        }
        break;

    case EP_STATE_TIME_WAIT:
        //handled by close comms
        break;

    case EP_STATE_CLOSED:
        break;

    }
}

/***********************************************************************
 * handler/parser for received buffers
 **********************************************************************/
void PothosPacketSocketEndpoint::Impl::unpackHeader(const PothosPacketHeader &header, const size_t recvBytes, uint16_t &flags, uint16_t &type, size_t &payloadBytes)
{
    if (recvBytes < sizeof(header))
    {
        throw Pothos::Exception("PothosPacketSocketEndpoint::unpackHeader()", "incomplete header");
    }

    if (BlocksByteOrder::fromNetwork(header.headerWord) != PothosPacketHeaderWord)
    {
        throw Pothos::Exception("PothosPacketSocketEndpoint::unpackHeader()", "headerWord fail");
    }

    //extract header fields
    flags = BlocksByteOrder::fromNetwork(header.flags);
    const uint32_t recvPacketCount = BlocksByteOrder::fromNetwork(header.packetCount);
    payloadBytes = BlocksByteOrder::fromNetwork(header.payloadBytes);
    type = BlocksByteOrder::fromNetwork(header.type);

    //when the sender is telling us to use a new sequence number
    if ((flags & PothosPacketFlagSyn) != 0) this->nextRecvPacketCount = recvPacketCount;

    //always must be correct
    if (recvPacketCount != this->nextRecvPacketCount)
    {
        throw Pothos::Exception("PothosPacketSocketEndpoint::unpackHeader()", "packetCount fail");
    }

    //increment for next packet
    this->nextRecvPacketCount = recvPacketCount + 1;

    //save header fields for partial recvs
    lastType = type;

    //run the handler for the state machine
    this->handleState(flags);
}

/***********************************************************************
 * perform a recv operation on the connected socket
 **********************************************************************/
void PothosPacketSocketEndpoint::recv(uint16_t &type, Pothos::BufferChunk &buffer, const std::chrono::high_resolution_clock::duration &timeout)
{
    uint16_t flags = 0;
    try {return _impl->recv(flags, type, buffer, timeout);}
    catch (...) {_impl->fail(); throw;}
}

void PothosPacketSocketEndpoint::Impl::recv(uint16_t &flags, uint16_t &type, Pothos::BufferChunk &buffer, const std::chrono::high_resolution_clock::duration &timeout)
{
    flags = 0;
    type = 0;

    if (not this->iface->isRecvReady(timeout)) return;

    PothosPacketHeader header;

    //no bytes left in stream, receive a new header
    if (this->bytesLeftInStream == 0)
    {
        //receive the header
        this->iface->recvAll(&header, sizeof(header));
        this->totalBytesRecv += sizeof(header);

        //extract header fields
        this->unpackHeader(header, sizeof(header), flags, type, this->bytesLeftInStream);

        //create a new buffer of the required length if need be
        //partial receives are always ok with packet buffer type
        if (type != PothosPacketTypeBuffer and buffer.length < this->bytesLeftInStream)
        {
            buffer = Pothos::BufferChunk(this->bytesLeftInStream);
        }
    }

    //otherwise, restore from the last recv()
    else
    {
        flags = 0;
        type = lastType;
    }

    //receive the payload into the available buffer
    size_t bytesRecvd = 0;
    buffer.length = std::min(buffer.length, this->bytesLeftInStream);
    while (buffer.length > bytesRecvd)
    {
        const auto ret = this->iface->recvSome((buffer.as<char *>() + bytesRecvd), buffer.length-bytesRecvd);
        this->totalBytesRecv += ret;
        bytesRecvd += ret;
    }

    this->bytesLeftInStream -= buffer.length;

    //deal with flow control (incoming)
    if ((flags & PothosPacketFlagFlo) != 0 and buffer.length >= sizeof(uint64_t))
    {
        const uint64_t totalN = buffer.as<const uint64_t *>()[0];
        this->lastFlowMsgRecv = BlocksByteOrder::fromNetwork(uint64_t(totalN));
    }

    //deal with flow control (outgoing)
    if (this->totalBytesRecv > this->lastFlowMsgSent + this->flowControlWindowBytes()/8)
    {
        const uint64_t totalN = BlocksByteOrder::toNetwork(uint64_t(this->totalBytesRecv));
        this->send(PothosPacketFlagFlo, 0, &totalN, sizeof(totalN));
        this->lastFlowMsgSent = this->totalBytesRecv;
    }
}

/***********************************************************************
 * perform a send operation on the connected socket
 **********************************************************************/
void PothosPacketSocketEndpoint::send(const uint16_t type, const void *buff, const size_t numBytes, const bool more)
{
    try {_impl->send(PothosPacketFlagPsh, type, buff, numBytes, more);}
    catch (...) {_impl->fail(); throw;}
}

void PothosPacketSocketEndpoint::Impl::send(const uint16_t flags, const uint16_t type, const void *buff, const size_t numBytes, const bool more)
{
    std::unique_lock<std::mutex> lock(this->sendMutex);

    bool hasMore;
    PothosPacketHeader header;
    header.headerWord = BlocksByteOrder::toNetwork(PothosPacketHeaderWord);
    header.flags = BlocksByteOrder::toNetwork(flags);
    header.payloadBytes = BlocksByteOrder::toNetwork(uint32_t(numBytes));
    header.packetCount = BlocksByteOrder::toNetwork(uint32_t(this->lastSentPacketCount++));
    header.type = BlocksByteOrder::toNetwork(type);

    //send the header
    hasMore = (numBytes != 0) or more;
    this->iface->sendAll(&header, sizeof(header), hasMore?MSG_MORE:0);
    this->totalBytesSent += sizeof(header);

    //send all of the available buffer
    size_t bytesLeft = numBytes;
    while (bytesLeft != 0)
    {
        const size_t bytesToSend = std::min<size_t>(bytesLeft, SEND_MTU);
        hasMore = (bytesToSend != bytesLeft) or more;
        this->iface->sendAll(static_cast<const char *>(buff)+numBytes-bytesLeft, bytesToSend, hasMore?MSG_MORE:0);
        this->totalBytesSent += bytesToSend;
        bytesLeft -= bytesToSend;
    }
}
