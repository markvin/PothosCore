########################################################################
# The core as a CMake subproject (cmake -P script, test pothos.subproject).
#
# Required: -DSRC=<core source> -DWORK=<scratch dir> -DGENERATOR=<generator>
#           -DCXX=<C++ compiler> -DCC=<C compiler>
# Optional: -DTOOLCHAIN=<toolchain file> -DPREFIX_PATH=<|-separated prefixes>
#           -DBUILD_TYPE=<build type> to find the dependencies of the core as
#           the calling build does (the package configurations of a toolchain
#           file can need the build type)
#
# Configures tests/subproject (which checks the parent's cache and targets):
#  - default: the parent's ctest lists only its own test
#  - -DPOTHOS_BUILD_TESTS=ON: the core's tests are listed too and stage
#    into the core's own binary directory
#  - -DPOTHOS_BUILD_APPS=OFF: configures without PothosUtil
#  - -DPOTHOS_BUILD_DOCS=ON: no clash with the parent's docs target
#  - the core declared with OVERRIDE_FIND_PACKAGE instead of add_subdirectory
# In each one a toolkit next to the core (tests/subproject/sibling) and the
# parent find the core with find_package(Pothos).
########################################################################
cmake_minimum_required(VERSION 3.25)
foreach(var SRC WORK GENERATOR CXX CC)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosSubprojectTest: ${var} is not set")
    endif ()
endforeach()

#an initial cache: a list of prefixes stays one value
#the generator settings of the calling build 
set(generator_args)
if (MAKE_PROGRAM)
    list(APPEND generator_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif ()
if (PLATFORM)
    list(APPEND generator_args -A "${PLATFORM}")
endif ()
if (TOOLSET)
    list(APPEND generator_args -T "${TOOLSET}")
endif ()

set(dependency_args)
if (TOOLCHAIN OR PREFIX_PATH)
    string(REPLACE "|" ";" prefix_path "${PREFIX_PATH}")
    file(WRITE "${WORK}/dependencies.cmake"
        "set(CMAKE_TOOLCHAIN_FILE \"${TOOLCHAIN}\" CACHE FILEPATH \"\")\n"
        "set(CMAKE_PREFIX_PATH \"${prefix_path}\" CACHE STRING \"\")\n"
        "set(CMAKE_BUILD_TYPE \"${BUILD_TYPE}\" CACHE STRING \"\")\n")
    set(dependency_args -C "${WORK}/dependencies.cmake")
endif ()

function(configure_parent dir)
    file(REMOVE_RECURSE "${dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${SRC}/tests/subproject" -B "${dir}" -G "${GENERATOR}" ${generator_args}
            "-DPOTHOS_SOURCE=${SRC}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}"
            ${dependency_args} ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if (NOT rc EQUAL 0 OR NOT out MATCHES "PARENT_CHECK OK" OR NOT out MATCHES "SIBLING_CHECK OK")
        message(FATAL_ERROR "configuring the parent (${ARGN}) failed (${rc}):\n${out}\n${err}")
    endif ()
endfunction()

#configure the parent and expect a failure with a text
function(configure_parent_fails dir text)
    file(REMOVE_RECURSE "${dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${SRC}/tests/subproject" -B "${dir}" -G "${GENERATOR}" ${generator_args}
            "-DPOTHOS_SOURCE=${SRC}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}"
            ${dependency_args} ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(REGEX REPLACE "[ \n\r]+" " " all "${out}${err}")
    if (rc EQUAL 0 OR NOT all MATCHES "${text}")
        message(FATAL_ERROR "configuring the parent (${ARGN}) should fail with \"${text}\" (${rc}):\n${out}\n${err}")
    endif ()
endfunction()

function(list_tests dir result)
    execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${dir}" -N OUTPUT_VARIABLE out)
    set(${result} "${out}" PARENT_SCOPE)
endfunction()

#default: the parent's test only
configure_parent("${WORK}/default")
list_tests("${WORK}/default" out)
string(FIND "${out}" "parent.test" parentPos)
string(FIND "${out}" "pothos." pothosPos)
if (parentPos EQUAL -1 OR NOT pothosPos EQUAL -1)
    message(FATAL_ERROR "the parent's ctest should list parent.test and no pothos test:\n${out}")
endif ()

#tests requested: listed by the parent, staged into the core's binary directory
configure_parent("${WORK}/tests" -DPOTHOS_BUILD_TESTS=ON)
list_tests("${WORK}/tests" out)
string(FIND "${out}" "pothos.stage" stagePos)
if (stagePos EQUAL -1)
    message(FATAL_ERROR "POTHOS_BUILD_TESTS=ON: the parent's ctest lists no test of the core:\n${out}")
endif ()
file(READ "${WORK}/tests/pothos/CTestTestfile.cmake" testfile)
string(FIND "${testfile}" "${WORK}/tests/pothos/stage" pathPos)
if (pathPos EQUAL -1)
    message(FATAL_ERROR "the core's stage directory is not in its own binary directory:\n${testfile}")
endif ()

#optional parts of the core
configure_parent("${WORK}/no-apps" -DPOTHOS_BUILD_APPS=OFF)
#a module with docs needs PothosUtil: a configure error, not an empty build command
configure_parent_fails("${WORK}/no-apps-docs" "docs need PothosUtil" -DPOTHOS_BUILD_APPS=OFF -DSIBLING_DOCS=ON)

#an installed core found by the parent before add_subdirectory of the core
#(STAGE_CONFIG_DIR: share/cmake/Pothos, or cmake on Windows)
if (STAGE AND STAGE_CONFIG_DIR)
    configure_parent_fails("${WORK}/find-first" "Pothos::Pothos already exists"
        -DPOTHOS_SUBPROJECT_FIND_FIRST=ON "-DPothos_DIR=${STAGE_CONFIG_DIR}")
endif ()

#macOS: the core needs a deployment target of 13.3 or higher (std::format)
if (APPLE)
    configure_parent_fails("${WORK}/macos-12" "13.3" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
endif ()

#a copy of the core without .git inside another git work tree: the version
#has no hash (not the hash of the other repository)
find_program(GIT_EXECUTABLE git)
if (GIT_EXECUTABLE)
    set(outer "${WORK}/vendored")
    file(REMOVE_RECURSE "${outer}")
    file(MAKE_DIRECTORY "${outer}/core")
    #the files of the work tree (not HEAD: the check must see changes that are not committed)
    #safe.directory: in a CI container the checkout has another owner
    execute_process(COMMAND ${GIT_EXECUTABLE} -c "safe.directory=${SRC}" -C "${SRC}" ls-files
        RESULT_VARIABLE rc OUTPUT_VARIABLE tracked ERROR_VARIABLE err OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT rc EQUAL 0 OR tracked STREQUAL "")
        message(FATAL_ERROR "git ls-files of ${SRC} failed (${rc}): ${err}")
    endif ()
    string(REPLACE "\n" ";" tracked "${tracked}")
    foreach(file IN LISTS tracked)
        if (EXISTS "${SRC}/${file}")
            get_filename_component(dir "${outer}/core/${file}" DIRECTORY)
            file(COPY "${SRC}/${file}" DESTINATION "${dir}")
        endif ()
    endforeach()
    execute_process(COMMAND ${GIT_EXECUTABLE} init -q "${outer}")
    execute_process(COMMAND ${GIT_EXECUTABLE} -C "${outer}" -c user.name=test -c user.email=test@test
        commit -q --allow-empty -m outer)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${outer}/core" -B "${outer}/build" -G "${GENERATOR}" ${generator_args}
            "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}" -DPOTHOS_BUILD_DOCS=OFF
            ${dependency_args}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if (NOT rc EQUAL 0 OR NOT out MATCHES "Pothos version: v[0-9.]+-unknown")
        message(FATAL_ERROR "a vendored core (no .git) should have the version suffix -unknown (${rc}):\n${out}\n${err}")
    endif ()
else ()
    message(STATUS "no git: the vendored core check is skipped")
endif ()
configure_parent("${WORK}/docs" -DPOTHOS_BUILD_DOCS=ON)

#declared with OVERRIDE_FIND_PACKAGE: the core still finds its own configuration
configure_parent("${WORK}/fetchcontent" -DPOTHOS_SUBPROJECT_FETCHCONTENT=ON)
message(STATUS "subproject checks passed")
