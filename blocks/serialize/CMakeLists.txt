########################################################################
# SerializeBlocks module
########################################################################
pothos_blocks_module(
    TARGET SerializeBlocks
    SOURCES
        Serializer.cpp
        Deserializer.cpp
        TestSerialize.cpp
    TESTS
        test_serializer_blocks
)
