########################################################################
# A parent project that adds the Pothos core with add_subdirectory and
# checks that the core leaves its options, cache and globals alone
# (configured by cmake/PothosSubprojectTest.cmake; -DPOTHOS_SOURCE=<core>)
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosSubprojectParent LANGUAGES C CXX)

#the parent's own options and targets with names the core used to take
option(ENABLE_LIBRARY "the parent's own library" OFF)
option(BUILD_DOCS "the parent's own documentation" OFF)
add_custom_target(uninstall)

set(checked ENABLE_LIBRARY BUILD_DOCS CMAKE_CXX_STANDARD CMAKE_INSTALL_RPATH
    CMAKE_EXPORT_COMPILE_COMMANDS CMAKE_BUILD_TYPE)
foreach(var ${checked})
    set(before_${var} "${${var}}")
endforeach()
get_property(before_cached_build_type CACHE CMAKE_BUILD_TYPE PROPERTY VALUE)

add_subdirectory(${POTHOS_SOURCE} pothos)

set(errors)
foreach(var ${checked})
    if(NOT "${before_${var}}" STREQUAL "${${var}}")
        list(APPEND errors "${var}: [${before_${var}}] -> [${${var}}]")
    endif()
endforeach()
get_property(cached_build_type CACHE CMAKE_BUILD_TYPE PROPERTY VALUE)
if(NOT "${before_cached_build_type}" STREQUAL "${cached_build_type}")
    list(APPEND errors "cached CMAKE_BUILD_TYPE: [${before_cached_build_type}] -> [${cached_build_type}]")
endif()
if(NOT TARGET Pothos::Pothos)
    list(APPEND errors "no target Pothos::Pothos")
endif()
if(errors)
    string(REPLACE ";" "\n  " text "${errors}")
    message(FATAL_ERROR "the Pothos core changed the parent project:\n  ${text}")
endif()
message(STATUS "PARENT_CHECK OK")
