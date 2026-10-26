########################################################################
# gather version information
# packagers may specify -DPOTHOS_EXTVER="foo" to replace the git hash
########################################################################
cmake_minimum_required(VERSION 3.25)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Changelog.txt" changelog_txt)
string(REGEX MATCH "Release ([0-9]+\\.[0-9]+\\.[0-9]+) \\(" CHANGELOG_MATCH "${changelog_txt}")
if(NOT CHANGELOG_MATCH)
    message(FATAL_ERROR "Failed to extract version number from Changelog.txt")
endif()
set(POTHOS_LIBVER "${CMAKE_MATCH_1}")

########################################################################
# Project setup
########################################################################
project(Pothos VERSION ${POTHOS_LIBVER} LANGUAGES C CXX)
enable_testing()

#C++23 is a required language feature for this project
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT POTHOS_EXTVER)
    include(${PROJECT_SOURCE_DIR}/cmake/Modules/GetGitRevisionDescription.cmake)
    get_git_head_revision(GITREFSPEC GITHASH)
    if (GITHASH)
        string(SUBSTRING "${GITHASH}" 0 8 GITHASH)
        set(POTHOS_EXTVER "g${GITHASH}")
    else (GITHASH)
        set(POTHOS_EXTVER "unknown")
    endif (GITHASH)
endif()

set(POTHOS_VERSION "${POTHOS_LIBVER}-${POTHOS_EXTVER}")

#set in-tree variables for PothosConfig.cmake
set(Pothos_DIR ${PROJECT_SOURCE_DIR}/cmake/Modules)
set(POTHOS_IN_TREE_SOURCE_DIR ${PROJECT_SOURCE_DIR})

#POTHOS_ROOT is compiled into the library to locate the install base.
#By default, the POTHOS_ROOT is set to the CMAKE_INSTALL_PREFIX.
#However users may overload this by specifying -DPOTHOS_ROOT=<path>.
set(POTHOS_ROOT "${CMAKE_INSTALL_PREFIX}" CACHE PATH
    "Installation root for Pothos::System::getRootPath()")
file(TO_CMAKE_PATH "${POTHOS_ROOT}" POTHOS_ROOT)

#POTHOS_ROOT_ENV is the name of the environment variable
#which tells Pothos where to find the root installation.
#By default, the environment variable POTHOS_ROOT is used.
#Example: set -DPOTHOS_ROOT_ENV=SNAP for snappy packages.
set(POTHOS_ROOT_ENV "POTHOS_ROOT" CACHE STRING
    "Environment variable for Pothos::System::getRootPath()")

#APPDATA_ENV is the name of the environment variable
#which tells Pothos where to find the user's data directory.
#By default, the environment variable APPDATA is used.
#Example: set -DAPPDATA_ENV=SNAP_USER_DATA for snappy packages.
set(APPDATA_ENV "APPDATA" CACHE STRING
    "Environment variable for Pothos::System::getUserDataPath()")

include(FeatureSummary)
include(CMakeDependentOption)
include(GNUInstallDirs)

########################################################################
# rpath setup - http://www.cmake.org/Wiki/CMake_RPATH_handling
########################################################################
# use, i.e. don't skip the full RPATH for the build tree
option(CMAKE_SKIP_BUILD_RPATH "skip rpath build" FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
option(CMAKE_BUILD_WITH_INSTALL_RPATH "build with install rpath" FALSE)

# the RPATH to be used when installing, but only if it's not a system directory
option(CMAKE_AUTOSET_INSTALL_RPATH TRUE)
if(CMAKE_AUTOSET_INSTALL_RPATH)
    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    endif()
endif()

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "build with automatic rpath" TRUE)

if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
endif()

########################################################################
# POCO C++ utility libraries (transitional, private, in-tree)
########################################################################
if (NOT EXISTS ${PROJECT_SOURCE_DIR}/poco/CMakeLists.txt)
    message(FATAL_ERROR "poco submodule not configured (git submodule update --init poco)")
endif ()

#mark that the in-tree poco build is used by PothosConfig
set(POCO_IN_TREE TRUE)

block()
    #poco's cmake predates CMake 3.5 policies
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

    #minimum build: only Foundation, Util, XML and Net are used
    set(ENABLE_MONGODB OFF CACHE INTERNAL "")
    set(ENABLE_PDF OFF CACHE INTERNAL "")
    set(ENABLE_NETSSL OFF CACHE INTERNAL "")
    set(ENABLE_CRYPTO OFF CACHE INTERNAL "")
    set(ENABLE_DATA OFF CACHE INTERNAL "")
    set(ENABLE_SEVENZIP OFF CACHE INTERNAL "")
    set(ENABLE_ZIP OFF CACHE INTERNAL "")
    set(ENABLE_APACHECONNECTOR OFF CACHE INTERNAL "")
    set(ENABLE_JSON OFF CACHE INTERNAL "")
    set(ENABLE_REDIS OFF CACHE INTERNAL "")
    set(ENABLE_PAGECOMPILER OFF CACHE INTERNAL "")
    set(ENABLE_PAGECOMPILER_FILE2PAGE OFF CACHE INTERNAL "")

    add_subdirectory(poco)
endblock()

########################################################################
# Third party libraries in the source tree
########################################################################
add_subdirectory(third_party)

########################################################################
# Build the framework library
########################################################################
cmake_dependent_option(ENABLE_LIBRARY "Enable Pothos Library component" ON "TRUE" OFF)
add_feature_info(Library ENABLE_LIBRARY "The main library for all Pothos projects")

if(ENABLE_LIBRARY)
    find_package(Pothos CONFIG REQUIRED)
    add_subdirectory(include)
    add_subdirectory(lib)
    add_subdirectory(cmake/Modules)
    add_subdirectory(apps)
    add_subdirectory(docs)
endif(ENABLE_LIBRARY)

########################################################################
# Tests: the self-tests need an installed layout (see Pothos::init()),
# so the build is first staged into a prefix inside the build tree.
########################################################################
set(POTHOS_STAGE_DIR ${CMAKE_BINARY_DIR}/stage)
set(POTHOS_SELFTEST_TIMEOUT 900 CACHE STRING "Timeout in seconds for the self-tests")

add_test(NAME pothos.stage
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${POTHOS_STAGE_DIR} --config $<CONFIG>)
set_tests_properties(pothos.stage PROPERTIES FIXTURES_SETUP pothos_stage)

add_test(NAME pothos.selftests
    COMMAND ${CMAKE_COMMAND}
        -DPREFIX=${POTHOS_STAGE_DIR}
        -DHOME_DIR=${CMAKE_BINARY_DIR}/test-home
        -DBINDIR=${CMAKE_INSTALL_BINDIR}
        -DLIBDIR=${CMAKE_INSTALL_LIBDIR}
        -P ${PROJECT_SOURCE_DIR}/cmake/PothosRunSelfTests.cmake)
set_tests_properties(pothos.selftests PROPERTIES
    FIXTURES_REQUIRED pothos_stage
    TIMEOUT ${POTHOS_SELFTEST_TIMEOUT})

add_test(NAME pothos.consumer
    COMMAND ${CMAKE_COMMAND}
        -DPREFIX=${POTHOS_STAGE_DIR}
        -DSRC=${PROJECT_SOURCE_DIR}/tests/consumer
        -DWORK=${CMAKE_BINARY_DIR}/consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DCONFIG=$<CONFIG>
        -DBINDIR=${CMAKE_INSTALL_BINDIR}
        -DLIBDIR=${CMAKE_INSTALL_LIBDIR}
        -P ${PROJECT_SOURCE_DIR}/cmake/PothosConsumerTest.cmake)
set_tests_properties(pothos.consumer PROPERTIES FIXTURES_REQUIRED pothos_stage)

########################################################################
# uninstall target
########################################################################
if (TARGET uninstall)
    # in-tree poco already defines uninstall: overwrite its script
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/poco/cmake_uninstall.cmake"
        @ONLY)
else ()
    add_custom_target(uninstall
        "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY)
endif ()

########################################################################
# print features summary
########################################################################
message(STATUS "")
message(STATUS "######################################################")
message(STATUS "## Enabled features")
message(STATUS "######################################################")
feature_summary(WHAT ENABLED_FEATURES)
message(STATUS "######################################################")
message(STATUS "## Disabled features")
message(STATUS "######################################################")
feature_summary(WHAT DISABLED_FEATURES)
message(STATUS "Pothos version: v${POTHOS_VERSION}")
message(STATUS "ABI/so version: v${POTHOS_ABI_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
