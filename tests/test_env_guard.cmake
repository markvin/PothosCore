########################################################################
# Regression test for cmake/PothosTestEnv.cmake (cmake -P script):
# pothos_test_env() must refuse to use the real home directory and
# must never delete anything but its own data/config subdirectories.
#
# Required: -DWORK=<scratch dir>
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
if (NOT DEFINED WORK)
    message(FATAL_ERROR "test_env_guard: WORK is not set")
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/PothosTestEnv.cmake)

set(fake_home "${WORK}/fake-home")
file(REMOVE_RECURSE "${fake_home}")
file(MAKE_DIRECTORY "${fake_home}")
file(WRITE "${fake_home}/precious.txt" "keep me")

#run pothos_test_env with the home directory as the test dir in a child process
set(wrapper "${WORK}/call_test_env.cmake")
file(WRITE "${wrapper}"
    "include(\"${CMAKE_CURRENT_LIST_DIR}/../cmake/PothosTestEnv.cmake\")\n"
    "pothos_test_env(\"${WORK}/stage\" \"${fake_home}\" bin lib)\n")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E env HOME=${fake_home} USERPROFILE=${fake_home}
        ${CMAKE_COMMAND} -P ${wrapper}
    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)

if (NOT EXISTS "${fake_home}/precious.txt")
    message(FATAL_ERROR "pothos_test_env deleted files in the home directory")
endif ()
if (rc EQUAL 0)
    message(FATAL_ERROR "pothos_test_env accepted the home directory as its test dir")
endif ()

#an existing unrelated file inside a legitimate test dir must survive too
set(test_dir "${WORK}/test-home")
file(REMOVE_RECURSE "${test_dir}")
file(MAKE_DIRECTORY "${test_dir}")
file(WRITE "${test_dir}/other.txt" "keep me")
pothos_test_env("${WORK}/stage" "${test_dir}" bin lib)
if (NOT EXISTS "${test_dir}/other.txt" OR NOT IS_DIRECTORY "${test_dir}/data")
    message(FATAL_ERROR "pothos_test_env removed unrelated files or did not create data/")
endif ()
message(STATUS "test_env_guard: OK")
