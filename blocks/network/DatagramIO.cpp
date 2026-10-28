// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "AsioWait.hpp"
#include <Pothos/Util/Network.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/System/Logger.hpp>
#include <asio/ip/udp.hpp>
#include <chrono>
#include <algorithm> //min/max
#include <iostream>

/***********************************************************************
 * |PothosDoc Datagram IO
 *
 * The datagram IO block binds or connects to a UDP socket
 * and provides input and output ports for either streams or packets.
 *
 * The input port 0 accepts all stream and input packets and
 * sends their raw bytes over UDP. Input streams are fragmented
 * to UDP MTU size. Packets are truncated to UDP MTU size.
 * Packet metadata, labels, and datatype are not preserved.
 *
 * The output port 0 produces streams of the specified data type
 * in the "STREAM" output mode. And produces packets of the specified
 * data type in the "PACKET" output mode.
 *
 * <h2>Bind vs Connect</h2>
 *
 * The socket can be used in two basic operation modes: bind or connect.
 *
 * In the bind mode, the socket is bound to a network address like:
 * 0.0.0.0, localhost, or the address of a configured ethernet device.
 * The socket is capable of receiving UDP packets from another socket,
 * but it cannot send any packets until it has received at least one packet.
 * Because the last received packet provides the destination for sending.
 *
 * In connected mode, the socket is connected to a network address
 * where a bound server socket is running. Example addresss: localhost,
 * a resolvable hostname, or IP address of a specific machine.
 * The connected socket can both send UDP packets to the server
 * or receive UDP packets from the server and only from the server.
 *
 * |category /Network
 * |keywords udp datagram packet network
 *
 * |param dtype[Data Type] The output data type.
 * Sets the data type of the output port and also of the buffer in packet mode.
 * |widget DTypeChooser(float=1,cfloat=1,int=1,cint=1,uint=1,cuint=1,dim=1)
 * |default "complex_float32"
 * |preview disable
 *
 * |param uri[URI] The bind or connection uri string.
 * |default "udp://localhost:1234"
 * |widget StringEntry()
 *
 * |param opt[Option] Control if the socket is a server (BIND) or client (CONNECT).
 * |option [Connect] "CONNECT"
 * |option [Bind] "BIND"
 * |default "BIND"
 *
 * |param mode[Mode] The output mode (stream or packets).
 * <ul>
 * <li>"STREAM" - Produce the received datagram as a sample stream.</li>
 * <li>"PACKET" - Preserve the datagram boundaries and produce Pothos::Packet.</li>
 * </lu>
 * |default "STREAM"
 * |option [Stream] "STREAM"
 * |option [Packet] "PACKET"
 *
 * |param mtu[MTU] The maximum size of a datagram payload in bytes.
 * |default 1472
 * |units bytes
 *
 * |param recvTimeout[Receive Timeout] The receive timeout in microseconds.
 * How long to wait in work for an incoming datagram before yielding the context.
 * |units us
 * |tab Advanced
 * |preview disable
 * |default 10
 *
 * |param recvBuffSize[Receive Buffer] The size of the receive buffer.
 * Set the size of the receive socket buffer (0 for default).
 * |units bytes
 * |tab Advanced
 * |preview valid
 * |default 0
 *
 * |param sendBuffSize[Send Buffer] The size of the send buffer.
 * Set the size of the send socket buffer (0 for default).
 * |units bytes
 * |tab Advanced
 * |preview valid
 * |default 0
 *
 * |factory /blocks/datagram_io(dtype)
 * |initializer setupSocket(uri, opt)
 * |setter setMode(mode)
 * |setter setMTU(mtu)
 * |setter setRecvTimeout(recvTimeout)
 * |setter setBufferSize(recvBuffSize, sendBuffSize)
 **********************************************************************/
class DatagramIO : public Pothos::Block
{
public:
    static Block *make(const Pothos::DType &dtype)
    {
        return new DatagramIO(dtype);
    }

    DatagramIO(const Pothos::DType &dtype):
        _sock(_context),
        _packetMode(false),
        _timeoutUs(10),
        _mtu(1472)
    {
        this->setupInput(0);
        this->setupOutput(0, dtype);
        this->registerCall(this, POTHOS_FCN_TUPLE(DatagramIO, setupSocket));
        this->registerCall(this, POTHOS_FCN_TUPLE(DatagramIO, setMode));
        this->registerCall(this, POTHOS_FCN_TUPLE(DatagramIO, setMTU));
        this->registerCall(this, POTHOS_FCN_TUPLE(DatagramIO, setRecvTimeout));
        this->registerCall(this, POTHOS_FCN_TUPLE(DatagramIO, setBufferSize));
    }

    ~DatagramIO(void)
    {
        asio::error_code ec;
        _sock.close(ec);
    }

    void setupSocket(const std::string &uri, const std::string &opt)
    {
        try
        {
            const auto uriObj = Pothos::Util::parseUri(uri);
            asio::error_code ec;
            auto host = asio::ip::make_address(uriObj.host, ec);
            if (ec) host = asio::ip::make_address(Pothos::Util::resolveHost(uriObj.host, std::chrono::seconds(10)));
            const asio::ip::udp::endpoint addr(host, uriObj.port);
            if (opt != "CONNECT" and opt != "BIND") throw Pothos::FileException("DatagramIO::setupSocket("+uri+" -> "+opt+")", "unknown option");
            _sock.close(ec);
            _sock.open(addr.protocol());
            if (opt == "CONNECT") _sock.connect(addr);
            else
            {
                _sock.set_option(asio::ip::udp::socket::reuse_address(true));
                _sock.bind(addr);
            }
        }
        catch (const std::system_error &ex)
        {
            throw Pothos::InvalidArgumentException("DatagramIO::setupSocket("+uri+" -> "+opt+")", ex.what());
        }
        this->applyBufferSizes();
        _socketConnected = (opt == "CONNECT");
    }

    void setMode(const std::string &mode)
    {
        if (mode == "STREAM") _packetMode = false;
        else if (mode == "PACKET") _packetMode = true;
        else throw Pothos::FileException("DatagramIO::InvalidArgumentException("+mode+")", "unknown mode");
    }

    void setMTU(const size_t mtu)
    {
        auto outPort = this->output(0);
        const size_t elemSize = outPort->dtype().size();
        if ((mtu % elemSize) != 0) throw Pothos::InvalidArgumentException("DatagramIO::setMTU("+std::to_string(mtu)+")",
            "The MTU is not a multiple of the output data-type size: " + outPort->dtype().toString());

        outPort->setReserve(_mtu/elemSize);
        _mtu = mtu;
    }

    void setRecvTimeout(const long timeoutUs)
    {
        _timeoutUs = timeoutUs;
    }

    void setBufferSize(const size_t recvSize, const size_t sendSize)
    {
        _recvBuffSize = recvSize;
        _sendBuffSize = sendSize;
        this->applyBufferSizes();
    }

    void work(void)
    {
        auto inPort = this->input(0);
        bool hadEvent = false;

        //incoming packet to send
        if (inPort->hasMessage())
        {
            const auto msg = inPort->popMessage();
            if (msg.type() != typeid(Pothos::Packet))
            {
                Pothos::System::log(Pothos::System::LogLevel::Error, "DatagramIO", "Dropped input message of type {}; only Pothos::Packet supported", msg.getTypeString());
            }
            const auto &pkt = msg.extract<Pothos::Packet>();
            this->sendBuffer(pkt.payload);
            hadEvent = true;
        }

        //incoming stream to send (clip to MTU)
        auto inBuff = inPort->buffer();
        if (inBuff.length != 0)
        {
            //clip to the MTU size
            inBuff.length = std::min(inBuff.length, _mtu);

            //preserve element multiples
            const size_t elemSize = inBuff.dtype.size();
            inBuff.length = (inBuff.length/elemSize)*elemSize;

            inPort->consume(inBuff.length);
            this->sendBuffer(inBuff);
            hadEvent = true;
        }

        //small polling sleep if nothing happened and there is nothing to recv
        asio::error_code ec;
        if (not hadEvent and _sock.is_open() and _sock.available(ec) == 0)
        {
            const auto pollTimeUs = std::min<long long>(_timeoutUs, this->workInfo().maxTimeoutNs/1000);
            waitReadable(_context, _sock, std::chrono::microseconds(pollTimeUs));
        }

        //incoming UDP datagram
        if (_sock.is_open() and _sock.available(ec) != 0)
        {
            auto outPort = this->output(0);
            auto outBuff = outPort->buffer();
            asio::ip::udp::endpoint recvAddr;
            const auto ret = _sock.receive_from(asio::buffer(outBuff.as<void *>(), outBuff.length), recvAddr, 0, ec);
            if (ec)
            {
                Pothos::System::log(Pothos::System::LogLevel::Error, "DatagramIO", "Socket recv {} bytes failed: {}", outBuff.length, ec.message());
            }
            else if (ret > 0)
            {
                if ((ret % outBuff.dtype.size()) != 0)
                {
                    Pothos::System::log(Pothos::System::LogLevel::Warning, "DatagramIO",
                        "Received {} bytes is not a multiple of the output size: {}.\n"
                        "Until the sender is fixed, expect possible truncation of data.",
                        ret, outBuff.dtype.toString());
                }

                outBuff.length = ret;
                if (_packetMode)
                {
                    Pothos::Packet pkt;
                    pkt.payload = std::move(outBuff);
                    outPort->popElements(pkt.payload.elements());
                    outPort->postMessage(std::move(pkt));
                }
                else
                {
                    outPort->produce(outBuff.elements());
                }

                //the new send-to address for bound sockets
                if (not _socketConnected)
                {
                    _sendAddr = recvAddr;
                    _hasSendAddr = true;
                }
            }
        }

        return this->yield(); //always yield to service recv() again
    }

private:

    void sendBuffer(const Pothos::BufferChunk &buff)
    {
        if (not _socketConnected and not _hasSendAddr)
        {
            Pothos::System::log(Pothos::System::LogLevel::Error, "DatagramIO", "A bound socket cannot send until it has received!");
            return;
        }
        asio::error_code ec;
        const auto data = asio::buffer(buff.as<const void *>(), buff.length);
        const auto ret = _socketConnected? _sock.send(data, 0, ec) : _sock.send_to(data, _sendAddr, 0, ec);
        if (ec or ret != buff.length)
        {
            Pothos::System::log(Pothos::System::LogLevel::Error, "DatagramIO", "Socket send {} bytes failed: {}",
                buff.length, ec? ec.message() : "sent "+std::to_string(ret));
        }
    }

    //! Apply the requested socket buffer sizes once the socket is open
    void applyBufferSizes(void)
    {
        if (not _sock.is_open()) return;
        asio::error_code ec;
        if (_recvBuffSize != 0)
        {
            _sock.set_option(asio::socket_base::receive_buffer_size(int(_recvBuffSize)), ec);
            asio::socket_base::receive_buffer_size actual;
            _sock.get_option(actual, ec);
            if (actual.value() < int(_recvBuffSize)) Pothos::System::log(Pothos::System::LogLevel::Warning, "DatagramIO",
                "Attempted to set the socket receive buffer to {} bytes.\n"
                "The actual size was {} bytes. System limits may require reconfiguration.",
                _recvBuffSize, actual.value());
        }
        if (_sendBuffSize != 0)
        {
            _sock.set_option(asio::socket_base::send_buffer_size(int(_sendBuffSize)), ec);
            asio::socket_base::send_buffer_size actual;
            _sock.get_option(actual, ec);
            if (actual.value() < int(_sendBuffSize)) Pothos::System::log(Pothos::System::LogLevel::Warning, "DatagramIO",
                "Attempted to set the socket send buffer to {} bytes.\n"
                "The actual size was {} bytes. System limits may require reconfiguration.",
                _sendBuffSize, actual.value());
        }
    }

    asio::io_context _context;
    asio::ip::udp::socket _sock;
    bool _packetMode;
    long _timeoutUs;
    size_t _mtu;
    size_t _recvBuffSize = 0;
    size_t _sendBuffSize = 0;

    //bound sockets only send to the last received address
    bool _socketConnected = false;
    bool _hasSendAddr = false;
    asio::ip::udp::endpoint _sendAddr;
};

static Pothos::BlockRegistry registerDatagramIO(
    "/blocks/datagram_io", &DatagramIO::make);
