########################################################################
# Configure, build and run an external project (tests/consumer,
# tests/api_compat) against a staged install (cmake -P script).
#
# Required: -DPREFIX=<staged install> -DSRC=<consumer source dir>
#           -DWORK=<scratch dir> -DGENERATOR=<cmake generator>
#           -DCONFIG=<build config> -DBINDIR=<relative bin dir>
#           -DLIBDIR=<relative lib dir>
# Optional: -DEXE=<executable to run> (default: consumer)
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var PREFIX SRC WORK GENERATOR CONFIG BINDIR LIBDIR)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosConsumerTest: ${var} is not set")
    endif ()
endforeach()

if (NOT DEFINED EXE)
    set(EXE consumer)
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/PothosTestEnv.cmake)
pothos_test_env("${PREFIX}" "${WORK}/home" "${BINDIR}" "${LIBDIR}")

function(run_step name)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE rc)
    if (NOT rc EQUAL 0)
        message(FATAL_ERROR "${EXE} ${name} failed: ${rc}")
    endif ()
endfunction()

file(REMOVE_RECURSE "${WORK}/build")
run_step(configure ${CMAKE_COMMAND} -S "${SRC}" -B "${WORK}/build" -G "${GENERATOR}"
    "-DCMAKE_PREFIX_PATH=${PREFIX}" "-DCMAKE_BUILD_TYPE=${CONFIG}")
run_step(build ${CMAKE_COMMAND} --build "${WORK}/build" --config "${CONFIG}")

set(exe "${WORK}/build/bin/${EXE}")
if (WIN32)
    set(exe "${WORK}/build/bin/${EXE}.exe")
endif ()
run_step(run "${exe}")
