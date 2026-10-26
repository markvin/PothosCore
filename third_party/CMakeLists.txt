########################################################################
# Third party code built from the source tree.
# These targets are private implementation details of the Pothos
# library: they are never installed or exported.
########################################################################

########################################################################
# nlohmann/json v3.11.3 (single header, MIT)
########################################################################
add_library(pothos_json INTERFACE)
target_include_directories(pothos_json INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

########################################################################
# muparserx expression parser (transitional, static, built from the
# submodule sources without its own cmake project)
########################################################################
set(MUPARSERX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/muparserx/parser)
if (NOT EXISTS ${MUPARSERX_SOURCE_DIR}/mpParser.h)
    message(FATAL_ERROR "muparserx submodule not configured (git submodule update --init muparserx)")
endif ()

file(GLOB MUPARSERX_SOURCES "${MUPARSERX_SOURCE_DIR}/*.cpp")
add_library(pothos_muparserx STATIC ${MUPARSERX_SOURCES})
set_target_properties(pothos_muparserx PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pothos_muparserx SYSTEM PUBLIC
    $<BUILD_INTERFACE:${MUPARSERX_SOURCE_DIR}>)
