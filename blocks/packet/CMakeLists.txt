########################################################################
# Packet blocks module
########################################################################
pothos_blocks_module(
    TARGET PacketBlocks
    SOURCES
        PacketToStream.cpp
        StreamToPacket.cpp
        TestPacketBlocks.cpp
    TESTS
        test_packet_blocks
        test_packet_to_stream
        test_stream_to_packet
)
