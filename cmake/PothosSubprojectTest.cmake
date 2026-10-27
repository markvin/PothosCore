########################################################################
# The core as a CMake subproject (cmake -P script, test pothos.subproject).
#
# Required: -DSRC=<core source> -DWORK=<scratch dir> -DGENERATOR=<generator>
#           -DCXX=<C++ compiler> -DCC=<C compiler>
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

function(configure_parent dir)
    file(REMOVE_RECURSE "${dir}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -S "${SRC}/tests/subproject" -B "${dir}" -G "${GENERATOR}"
            "-DPOTHOS_SOURCE=${SRC}" "-DCMAKE_CXX_COMPILER=${CXX}" "-DCMAKE_C_COMPILER=${CC}" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if (NOT rc EQUAL 0 OR NOT out MATCHES "PARENT_CHECK OK" OR NOT out MATCHES "SIBLING_CHECK OK")
        message(FATAL_ERROR "configuring the parent (${ARGN}) failed (${rc}):\n${out}\n${err}")
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
configure_parent("${WORK}/docs" -DPOTHOS_BUILD_DOCS=ON)

#declared with OVERRIDE_FIND_PACKAGE: the core still finds its own configuration
configure_parent("${WORK}/fetchcontent" -DPOTHOS_SUBPROJECT_FETCHCONTENT=ON)
message(STATUS "subproject checks passed")
