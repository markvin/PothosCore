########################################################################
# A toolkit next to the core in the same parent project: it finds the
# core like an installed one, with find_package(Pothos)
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosSiblingToolkit CXX)

find_package(Pothos CONFIG REQUIRED)

if(NOT TARGET Pothos::Pothos)
    message(FATAL_ERROR "find_package(Pothos) gave no target Pothos::Pothos")
endif()

#the module helper of the toolkits (docs scanning needs PothosUtil)
if(POTHOS_BUILD_APPS)
    if(NOT POTHOS_UTIL_EXE)
        message(FATAL_ERROR "find_package(Pothos) gave no POTHOS_UTIL_EXE")
    endif()
    POTHOS_MODULE_UTIL(
        TARGET SiblingModule
        SOURCES module.cpp
        DESTINATION sibling
        ENABLE_DOCS)
else()
    add_library(SiblingModule MODULE module.cpp)
    target_link_libraries(SiblingModule PRIVATE Pothos::Pothos)
endif()
message(STATUS "SIBLING_CHECK OK")
