########################################################################
# Tester blocks module
########################################################################
pothos_blocks_module(
    TARGET TesterBlocks
    SOURCES
        TestUnitTestBlocks.cpp
        TestSetThreadPool.cpp
        FeederSource.cpp
        CollectorSink.cpp
        BlackHole.cpp
        FiniteRelease.cpp
        InfiniteSource.cpp
        SporadicDropper.cpp
        SporadicLabeler.cpp
        SporadicSubnormal.cpp
        VectorSource.cpp
        MessageGenerator.cpp
        ConstantSource.cpp
        Abort.cpp
    TESTS
        test_unit_test_blocks
        test_unit_testplans
        test_set_thread_pool
)
