########################################################################
# Install a build into a stage prefix for the tests (cmake -P script).
# Before the install, remove the files of the previous install of this
# script (their list is in <prefix>/.pothos-stage-manifest.txt): a file
# that the new build does not install (a removed header or module) must
# not stay and be loaded. Other files of the prefix stay (toolkits can be
# installed into the stage of the core).
#
# A stage of an older build has no such list: then the install manifest of
# the build dir gives the files of the previous install.
#
# Required: -DBUILD_DIR=<build dir> -DPREFIX=<stage prefix> -DCONFIG=<config>
# Optional: -DOLD_INSTALL_MANIFEST=<file> in place of the install manifest
#           of the build dir (for the tests)
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var BUILD_DIR PREFIX)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "PothosStage: ${var} is not set")
    endif ()
endforeach()

set(stage_manifest "${PREFIX}/.pothos-stage-manifest.txt")
set(old_manifest "${stage_manifest}")
if (NOT EXISTS "${old_manifest}")
    if (DEFINED OLD_INSTALL_MANIFEST)
        set(old_manifest "${OLD_INSTALL_MANIFEST}")
    else ()
        set(old_manifest "${BUILD_DIR}/install_manifest.txt")
    endif ()
endif ()
if (EXISTS "${old_manifest}")
    file(STRINGS "${old_manifest}" old_files)
    foreach(file IN LISTS old_files)
        #only the files inside the prefix
        cmake_path(IS_PREFIX PREFIX "${file}" NORMALIZE inside)
        if (inside)
            file(REMOVE "${file}")
        endif ()
    endforeach()
endif ()

set(config_args)
if (CONFIG)
    set(config_args --config ${CONFIG})
endif ()
execute_process(COMMAND ${CMAKE_COMMAND} --install ${BUILD_DIR} --prefix ${PREFIX} ${config_args}
    RESULT_VARIABLE rc)
if (NOT rc EQUAL 0)
    message(FATAL_ERROR "PothosStage: the install failed: ${rc}")
endif ()

#the list of this install, for the next one
file(COPY_FILE "${BUILD_DIR}/install_manifest.txt" "${stage_manifest}")
