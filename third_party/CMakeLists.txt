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
# standalone Asio 1.38.2 (headers only, BSL-1.0): used by lib/Net
########################################################################
add_library(pothos_asio INTERFACE)
target_include_directories(pothos_asio SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/asio/include>)
target_compile_definitions(pothos_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
if (WIN32)
    target_compile_definitions(pothos_asio INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(pothos_asio INTERFACE ws2_32 mswsock)
endif ()
