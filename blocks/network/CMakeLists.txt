########################################################################
# Network blocks module: Asio through the core's internal network layer
########################################################################
pothos_blocks_module(
    TARGET NetworkBlocks
    SOURCES
        NetworkSource.cpp
        NetworkSink.cpp
        SocketEndpoint.cpp
        TestNetworkBlocks.cpp
        TestNetworkTopology.cpp
        DatagramIO.cpp
    LIBRARIES
        pothos_net
    TESTS
        test_network_blocks
        test_network_blocks_ipv6
        test_network_deactivate_unblocks
        test_network_topology
        test_datagram_io
)
