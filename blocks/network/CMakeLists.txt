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
        test_network_activate_stalled_peer
        test_network_stalled_reader
        test_network_topology
        test_datagram_io
)
