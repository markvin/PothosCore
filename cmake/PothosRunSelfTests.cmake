########################################################################
# Run the plugin self-tests from a staged install (cmake -P script).
#
# Required: -DPREFIX=<staged install> -DTEST_HOME_DIR=<isolated user dirs>
#           -DBINDIR=<relative bin dir> -DLIBDIR=<relative lib dir>
# Optional: -DTEST_PATH=<plugin path> to run a single self test
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var PREFIX TEST_HOME_DIR BINDIR LIBDIR)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosRunSelfTests: ${var} is not set")
    endif ()
endforeach()

include(${CMAKE_CURRENT_LIST_DIR}/PothosTestEnv.cmake)
pothos_test_env("${PREFIX}" "${TEST_HOME_DIR}" "${BINDIR}" "${LIBDIR}")

set(util "${PREFIX}/${BINDIR}/PothosUtil")
if (WIN32)
    set(util "${PREFIX}/${BINDIR}/PothosUtil.exe")
endif ()

if (TEST_PATH)
    set(args --self-test1 "${TEST_PATH}")
else ()
    set(args --self-tests)
endif ()

execute_process(COMMAND "${util}" ${args} RESULT_VARIABLE rc)
if (NOT rc EQUAL 0)
    message(FATAL_ERROR "PothosUtil ${args} failed: ${rc}")
endif ()
