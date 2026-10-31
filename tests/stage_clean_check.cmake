########################################################################
# Regression test for cmake/PothosStage.cmake (cmake -P script):
# the stage install removes the files of its previous install that the
# new install does not have, and keeps all other files of the prefix
# (toolkits can be installed into the stage of the core).
#
# Required: -DWORK=<scratch dir> -DBUILD_DIR=<core build dir> -DCONFIG=<config>
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var WORK BUILD_DIR)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "stage_clean_check: ${var} is not set")
    endif ()
endforeach()

set(prefix "${WORK}/stage")
file(REMOVE_RECURSE "${prefix}")

#a previous install of the stage: a module that the new install does not have
set(old_file "${prefix}/lib/Pothos/modules-old/core/libRemoved.so")
file(WRITE "${old_file}" "old")
file(WRITE "${prefix}/.pothos-stage-manifest.txt" "${old_file}\n")

#a file of a toolkit in the same prefix (not in the manifest)
set(toolkit_file "${prefix}/lib/Pothos/modules-old/toolkit/libToolkit.so")
file(WRITE "${toolkit_file}" "toolkit")

execute_process(COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${BUILD_DIR} -DPREFIX=${prefix} -DCONFIG=${CONFIG}
    -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/PothosStage.cmake
    RESULT_VARIABLE rc OUTPUT_QUIET)
if (NOT rc EQUAL 0)
    message(FATAL_ERROR "stage_clean_check: PothosStage.cmake failed: ${rc}")
endif ()
if (EXISTS "${old_file}")
    message(FATAL_ERROR "stage_clean_check: a file of the previous install stays in the stage")
endif ()
if (NOT EXISTS "${toolkit_file}")
    message(FATAL_ERROR "stage_clean_check: a file that is not in the manifest was removed")
endif ()
if (NOT EXISTS "${prefix}/.pothos-stage-manifest.txt")
    message(FATAL_ERROR "stage_clean_check: no manifest of the new install")
endif ()

#a stage of an older build has no stage manifest: the install manifest of
#the build dir (the previous install into the stage) gives the old files
file(REMOVE_RECURSE "${prefix}")
set(older_file "${prefix}/lib/Pothos/modules-old/core/libOlder.so")
file(WRITE "${older_file}" "older")
file(WRITE "${WORK}/old_install_manifest.txt" "${older_file}\n")
execute_process(COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${BUILD_DIR} -DPREFIX=${prefix} -DCONFIG=${CONFIG}
    -DOLD_INSTALL_MANIFEST=${WORK}/old_install_manifest.txt
    -P ${CMAKE_CURRENT_LIST_DIR}/../cmake/PothosStage.cmake
    RESULT_VARIABLE rc OUTPUT_QUIET)
if (NOT rc EQUAL 0)
    message(FATAL_ERROR "stage_clean_check: PothosStage.cmake failed: ${rc}")
endif ()
if (EXISTS "${older_file}")
    message(FATAL_ERROR "stage_clean_check: a file of an older stage (no stage manifest) stays in the stage")
endif ()
message(STATUS "stage_clean_check: OK")
