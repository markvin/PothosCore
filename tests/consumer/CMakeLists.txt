########################################################################
# A minimal external project built against an installed Pothos.
# It proves the package is usable on its own and that no private
# dependency of the library leaks into the exported target.
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosConsumer CXX)

find_package(Pothos CONFIG REQUIRED)

foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LINK_LIBRARIES INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(val Pothos::Pothos ${prop})
    if(val MATCHES "Poco|muparserx|nlohmann|simdpp")
        message(FATAL_ERROR "Pothos::Pothos leaks a private dependency via ${prop}: ${val}")
    endif()
endforeach()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE Pothos::Pothos)
