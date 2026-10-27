########################################################################
# File blocks module
########################################################################
pothos_blocks_module(
    TARGET FileBlocks
    SOURCES
        BinaryFileSink.cpp
        BinaryFileSource.cpp
        MemoryMappedBufferContainer.cpp
        MemoryMappedBufferManager.cpp
        TextFileSink.cpp
        TestBinaryFileBlocks.cpp
    TESTS
        test_binary_file_blocks
        test_binary_file_descriptor_blocks
        test_circular_binary_file_source
)
