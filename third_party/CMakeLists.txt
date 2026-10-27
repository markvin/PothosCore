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
