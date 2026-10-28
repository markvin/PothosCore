########################################################################
# A parent project that adds the Pothos core with add_subdirectory and
# checks that the core leaves its options, cache, targets and tests alone
# (configured by cmake/PothosSubprojectTest.cmake; -DPOTHOS_SOURCE=<core>)
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosSubprojectParent LANGUAGES C CXX)

#the parent's own tests, options, cache entries and targets with names
#the core used to take (or that are common in other projects)
enable_testing()
add_test(NAME parent.test COMMAND ${CMAKE_COMMAND} -E true)
option(ENABLE_LIBRARY "the parent's own library" OFF)
option(BUILD_DOCS "the parent's own documentation" OFF)
set(NUMA_LIBRARIES "/parent/libnuma.so" CACHE FILEPATH "the parent's own NUMA library")
add_custom_target(uninstall)
add_custom_target(docs)

get_cmake_property(cache_before CACHE_VARIABLES)
get_property(build_type_before CACHE CMAKE_BUILD_TYPE PROPERTY VALUE)

#the core as add_subdirectory, or as FetchContent that also overrides find_package
if(POTHOS_SUBPROJECT_FETCHCONTENT)
    include(FetchContent)
    FetchContent_Declare(Pothos SOURCE_DIR ${POTHOS_SOURCE} OVERRIDE_FIND_PACKAGE)
    FetchContent_MakeAvailable(Pothos)
else()
    add_subdirectory(${POTHOS_SOURCE} pothos)
endif()

#a toolkit next to the core, directories of the parent without their own
#project() and the parent itself find the core with find_package
add_subdirectory(sibling)
add_subdirectory(plain_a)
add_subdirectory(plain_b)
find_package(Pothos CONFIG REQUIRED)

set(errors)

#new cache entries must belong to the core (POTHOS_/Pothos prefix) or to CMake
#itself, including what its standard find modules cache (Doxygen with the docs),
#or be the find_package result of a dependency of the core (<package>_DIR, and
#asio_INCLUDE_DIR from cmake/Findasio.cmake): the parent can find the same package
get_cmake_property(cache_after CACHE_VARIABLES)
foreach(var ${cache_after})
    if(var IN_LIST cache_before)
        continue()
    endif()
    get_property(type CACHE ${var} PROPERTY TYPE)
    if(var MATCHES "^(POTHOS_|Pothos|CMAKE_|FETCHCONTENT_|_|DOXYGEN_|FIND_PACKAGE_MESSAGE_DETAILS_)" OR type STREQUAL "STATIC")
        continue()
    endif()
    if(var MATCHES "^(nlohmann_json|asio|xsimd)_DIR$" OR var STREQUAL "asio_INCLUDE_DIR")
        continue()
    endif()
    list(APPEND errors "new cache entry ${var}=${${var}}")
endforeach()

foreach(var ENABLE_LIBRARY BUILD_DOCS NUMA_LIBRARIES)
    get_property(value CACHE ${var} PROPERTY VALUE)
    if(var STREQUAL "NUMA_LIBRARIES" AND NOT value STREQUAL "/parent/libnuma.so")
        list(APPEND errors "the cache entry ${var} changed: ${value}")
    endif()
endforeach()
get_property(build_type_after CACHE CMAKE_BUILD_TYPE PROPERTY VALUE)
if(NOT "${build_type_before}" STREQUAL "${build_type_after}")
    list(APPEND errors "cached CMAKE_BUILD_TYPE: [${build_type_before}] -> [${build_type_after}]")
endif()

if(NOT TARGET Pothos::Pothos)
    list(APPEND errors "no target Pothos::Pothos")
endif()
if(TARGET pothos_uninstall)
    list(APPEND errors "the core created an uninstall target (uninstall belongs to the parent)")
endif()
get_target_property(numa_defs Pothos COMPILE_DEFINITIONS)
if(numa_defs MATCHES "HAVE_LIBNUMA" AND NOT POTHOS_ENABLE_LIBNUMA)
    list(APPEND errors "the parent's NUMA_LIBRARIES enabled NUMA in the core")
endif()

if(errors)
    string(REPLACE ";" "\n  " text "${errors}")
    message(FATAL_ERROR "the Pothos core changed the parent project:\n  ${text}")
endif()
message(STATUS "PARENT_CHECK OK")
