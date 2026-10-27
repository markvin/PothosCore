########################################################################
# Stream blocks module
########################################################################
#SIMD kernels: one copy of SIMD/*.cpp per architecture, chosen at runtime
#by the dispatchers that PothosUtil generates (StreamBlocks_SIMD.hpp)
set(simd_sources)
set(simd_tests)
if(POTHOS_BUILD_APPS)
    include(${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfigSIMD.cmake)
    PothosGenerateSIMDSources(simd_sources
        SIMD/StreamBlocks.json
        SIMD/Clamp.cpp
        SIMD/IsX.cpp
        SIMD/MinMax.cpp
        SIMD/Round.cpp)
    list(APPEND simd_sources TestSIMDDispatch.cpp)
    list(APPEND simd_tests test_simd_dispatch_matches_scalar)
endif()

pothos_blocks_module(
    TARGET StreamBlocks
    SOURCES
        Converter.cpp
        TestConverter.cpp
        Copier.cpp
        Delay.cpp
        TestDelay.cpp
        DynamicRouter.cpp
        Pacer.cpp
        Relabeler.cpp
        LabelStripper.cpp
        Gateway.cpp
        TestGateway.cpp
        Reinterpret.cpp
        RateMonitor.cpp
        MinMax.cpp
        TestMinMax.cpp
        Interleaver.cpp
        TestInterleaver.cpp
        Deinterleaver.cpp
        Clamp.cpp
        TestClamp.cpp
        Repeat.cpp
        TestRepeat.cpp
        IsX.cpp
        TestIsX.cpp
        Mute.cpp
        Round.cpp
        TestRoundBlocks.cpp
        Replace.cpp
        TestReplace.cpp
        FirstN.cpp
        TestFirstN.cpp
        TestCopier.cpp
        ${simd_sources}
    LIBRARIES
        $<$<BOOL:${POTHOS_BUILD_APPS}>:pothos_xsimd>
    TESTS
        ${simd_tests}
        test_converter
        test_round_blocks
        test_is_x
        test_gateway
        test_first_n
        test_repeat
        test_interleaver
        test_deinterleaver
        test_deinterleaver_to_interleaver
        test_interleaver_to_deinterleaver
        test_replace
        test_replace_infinity
        test_replace_neg_infinity
        test_replace_nan
        test_replace_neg_nan
        test_minmax
        test_delay
        test_clamp
        test_copier_domain_crossing
)
if(POTHOS_BUILD_APPS)
    target_compile_definitions(StreamBlocks PRIVATE POTHOS_XSIMD)
    target_include_directories(StreamBlocks PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    add_dependencies(StreamBlocks StreamBlocks_SIMDDispatcher)
endif()
