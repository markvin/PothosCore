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
#
# The core can be a subproject of another project (add_subdirectory,
# or a content fetch by the parent): options are prefixed with POTHOS_, and the globals,
# tests, summaries and the uninstall target are only for a top level build.
########################################################################
project(Pothos VERSION ${POTHOS_LIBVER} LANGUAGES C CXX)

option(POTHOS_BUILD_APPS "Build PothosUtil (needed by the self-tests and module safe-loading)" ON)
option(POTHOS_BUILD_DOCS "Build the Doxygen documentation (when Doxygen is found)" ${PROJECT_IS_TOP_LEVEL})
option(POTHOS_BUILD_TESTS "Register the CTest tests of the core" ${PROJECT_IS_TOP_LEVEL})

if(PROJECT_IS_TOP_LEVEL)
    #compile_commands.json in the build directory for code navigation (clangd)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    #select the release build type by default to get optimization flags
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
        message(STATUS "Build type not specified: defaulting to release.")
    endif()
endif()

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

#POTHOS_APPDATA_ENV is the name of the environment variable
#which tells Pothos where to find the user's data directory.
#By default, the environment variable APPDATA is used.
#Example: set -DPOTHOS_APPDATA_ENV=SNAP_USER_DATA for snappy packages.
set(POTHOS_APPDATA_ENV "APPDATA" CACHE STRING
    "Environment variable for Pothos::System::getUserDataPath()")

include(FeatureSummary)
include(GNUInstallDirs)

########################################################################
# Target settings instead of globals (CMAKE_CXX_STANDARD, CMAKE_INSTALL_RPATH...)
########################################################################
#C++23 comes from target_compile_features(cxx_std_23); no compiler extensions;
#the install RPATH is relative to the installed binary, so that an installation
#can be relocated (bin/ and lib/ are siblings)
function(pothos_target_settings target)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        INSTALL_RPATH_USE_LINK_PATH TRUE)
    if(APPLE)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
    elseif(UNIX)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
    endif()
endfunction()

########################################################################
# Third party libraries in the source tree
########################################################################
add_subdirectory(third_party)

########################################################################
# Build the framework library
########################################################################
find_package(Pothos CONFIG REQUIRED)
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(cmake/Modules)
if(POTHOS_BUILD_APPS)
    add_subdirectory(apps)
endif()
if(POTHOS_BUILD_DOCS)
    add_subdirectory(docs)
endif()

########################################################################
# Tests: the self-tests need an installed layout (see Pothos::init()),
# so the build is first staged into a prefix inside the build tree.
########################################################################
if(POTHOS_BUILD_TESTS)
    enable_testing()
    set(POTHOS_STAGE_DIR ${PROJECT_BINARY_DIR}/stage)
    set(POTHOS_SELFTEST_TIMEOUT 900 CACHE STRING "Timeout in seconds for the self-tests")

    add_test(NAME pothos.stage
        COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${POTHOS_STAGE_DIR} --config $<CONFIG>)
    set_tests_properties(pothos.stage PROPERTIES FIXTURES_SETUP pothos_stage)

    if(POTHOS_BUILD_APPS)
        add_test(NAME pothos.selftests
            COMMAND ${CMAKE_COMMAND}
                -DPREFIX=${POTHOS_STAGE_DIR}
                -DTEST_HOME_DIR=${PROJECT_BINARY_DIR}/test-home
                -DBINDIR=${CMAKE_INSTALL_BINDIR}
                -DLIBDIR=${CMAKE_INSTALL_LIBDIR}
                -P ${PROJECT_SOURCE_DIR}/cmake/PothosRunSelfTests.cmake)
        set_tests_properties(pothos.selftests PROPERTIES
            FIXTURES_REQUIRED pothos_stage
            TIMEOUT ${POTHOS_SELFTEST_TIMEOUT})
    endif()

    add_test(NAME pothos.consumer
        COMMAND ${CMAKE_COMMAND}
            -DPREFIX=${POTHOS_STAGE_DIR}
            -DSRC=${PROJECT_SOURCE_DIR}/tests/consumer
            -DWORK=${PROJECT_BINARY_DIR}/consumer
            -DGENERATOR=${CMAKE_GENERATOR}
            -DCONFIG=$<CONFIG>
            -DBINDIR=${CMAKE_INSTALL_BINDIR}
            -DLIBDIR=${CMAKE_INSTALL_LIBDIR}
            -P ${PROJECT_SOURCE_DIR}/cmake/PothosConsumerTest.cmake)
    set_tests_properties(pothos.consumer PROPERTIES FIXTURES_REQUIRED pothos_stage)

    #the core inside a parent project (not again from inside such a parent)
    if(PROJECT_IS_TOP_LEVEL)
        add_test(NAME pothos.subproject
            COMMAND ${CMAKE_COMMAND}
                -DSRC=${PROJECT_SOURCE_DIR}
                -DWORK=${PROJECT_BINARY_DIR}/subproject
                -DGENERATOR=${CMAKE_GENERATOR}
                -DCXX=${CMAKE_CXX_COMPILER}
                -DCC=${CMAKE_C_COMPILER}
                -P ${PROJECT_SOURCE_DIR}/cmake/PothosSubprojectTest.cmake)
    endif()

    add_test(NAME pothos.test_env_guard
        COMMAND ${CMAKE_COMMAND}
            -DWORK=${PROJECT_BINARY_DIR}/test-env-guard
            -P ${PROJECT_SOURCE_DIR}/tests/test_env_guard.cmake)
endif()

########################################################################
# uninstall target (top level only: inside a parent project the install
# manifest is the parent's, and uninstalling is the parent's business)
########################################################################
if(PROJECT_IS_TOP_LEVEL)
    add_custom_target(uninstall
        "${CMAKE_COMMAND}" -P "${PROJECT_BINARY_DIR}/cmake_uninstall.cmake")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
        @ONLY)
endif()

########################################################################
# print features summary (not into a parent project's output)
########################################################################
if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "")
    message(STATUS "######################################################")
    message(STATUS "## Enabled features")
    message(STATUS "######################################################")
    feature_summary(WHAT ENABLED_FEATURES)
    message(STATUS "######################################################")
    message(STATUS "## Disabled features")
    message(STATUS "######################################################")
    feature_summary(WHAT DISABLED_FEATURES)
endif()
message(STATUS "Pothos version: v${POTHOS_VERSION}")
message(STATUS "ABI/so version: v${POTHOS_ABI_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
