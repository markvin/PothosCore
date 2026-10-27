########################################################################
# The core as a CMake subproject (cmake -P script, test pothos.subproject).
#
# Required: -DSRC=<core source> -DWORK=<scratch dir> -DGENERATOR=<generator>
#           -DCXX=<C++ compiler> -DCC=<C compiler>
#
# Configures tests/subproject twice:
#  - default: the parent sees no Pothos test
#  - -DPOTHOS_BUILD_TESTS=ON: the core's tests exist and stage into the
#    core's own binary directory
########################################################################
cmake_minimum_required(VERSION 3.25)
foreach(var SRC WORK GENERATOR CXX CC)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosSubprojectTest: ${var} is not set")
    endif ()
endforeach()

function(configure_parent dir)
    file(REMOVE_RECURSE "${dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${SRC}/tests/subproject" -B "${dir}" -G "${GENERATOR}"
            "-DPOTHOS_SOURCE=${SRC}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if (NOT rc EQUAL 0 OR NOT out MATCHES "PARENT_CHECK OK")
        message(FATAL_ERROR "configuring the parent failed (${rc}):\n${out}\n${err}")
    endif ()
endfunction()

function(count_tests dir result)
    execute_process(COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${dir}" -N
        OUTPUT_VARIABLE out RESULT_VARIABLE rc)
    string(REGEX MATCH "Total Tests: ([0-9]+)" match "${out}")
    set(${result} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

#default: no test of the core in the parent
configure_parent("${WORK}/default")
count_tests("${WORK}/default" n)
if (NOT n STREQUAL "0")
    message(FATAL_ERROR "the parent has ${n} tests of the core without POTHOS_BUILD_TESTS")
endif ()

#tests requested: they stage into the core's binary directory
configure_parent("${WORK}/tests" -DPOTHOS_BUILD_TESTS=ON)
count_tests("${WORK}/tests/pothos" n)
if (n STREQUAL "" OR n EQUAL 0)
    message(FATAL_ERROR "POTHOS_BUILD_TESTS=ON registered no test of the core")
endif ()
file(READ "${WORK}/tests/pothos/CTestTestfile.cmake" testfile)
if (NOT testfile MATCHES "${WORK}/tests/pothos/stage")
    message(FATAL_ERROR "the core's stage directory is not in its own binary directory:\n${testfile}")
endif ()
message(STATUS "subproject checks passed")
