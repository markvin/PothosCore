########################################################################
# The blocks toolkit, built with the core: the modules link the core's
# third party targets (json, xsimd, Asio through lib/Net), which stay
# private to this build and are never installed
########################################################################

#pothos_blocks_module(TARGET <name> SOURCES <sources...>
#    [LIBRARIES <libraries...>] [TESTS <self test names...>])
#builds and installs a blocks module, TESTS are names under /blocks/tests
#run one by one by the pothos.blocks.selftests test
function(pothos_blocks_module)
    cmake_parse_arguments(BLOCKS "" "TARGET" "SOURCES;LIBRARIES;TESTS" ${ARGN})
    set(docs)
    if(POTHOS_BUILD_APPS) #the doc parser is PothosUtil
        set(docs ENABLE_DOCS)
    endif()
    POTHOS_MODULE_UTIL(
        TARGET ${BLOCKS_TARGET}
        SOURCES ${BLOCKS_SOURCES}
        LIBRARIES pothos_json ${BLOCKS_LIBRARIES}
        DESTINATION blocks
        ${docs})
    set_target_properties(${BLOCKS_TARGET} PROPERTIES CXX_EXTENSIONS OFF)
    #blocks/ for common/, lib/ for the core's internal header-only helpers
    target_include_directories(${BLOCKS_TARGET} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
        ${PROJECT_SOURCE_DIR}/lib)
    pothos_warnings_as_errors(${BLOCKS_TARGET})
    foreach(test IN LISTS BLOCKS_TESTS)
        set_property(GLOBAL APPEND PROPERTY POTHOS_BLOCKS_TESTS /blocks/tests/${test})
    endforeach()
endfunction()

add_subdirectory(event)
add_subdirectory(file)
add_subdirectory(network)
add_subdirectory(packet)
add_subdirectory(serialize)
add_subdirectory(stream)
add_subdirectory(testers)
