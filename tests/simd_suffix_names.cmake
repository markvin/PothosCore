########################################################################
# Check the SIMD copy names of all architectures (cmake -P script):
# each suffix of cmake/Modules/PothosConfigSIMD.cmake must be a feature
# name that lib/System/SIMD.cpp can report, else the copy is never chosen.
# This also checks the architectures that the build computer does not have.
#
# Required: -DSRC=<core source dir>
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
if (NOT DEFINED SRC)
    message(FATAL_ERROR "simd_suffix_names: SRC is not set")
endif ()
file(STRINGS "${SRC}/cmake/Modules/PothosConfigSIMD.cmake" suffix_lines REGEX "^set\\(POTHOS_[A-Z0-9_]+_SUFFIX \"-")
file(READ "${SRC}/lib/System/SIMD.cpp" simd_cpp)
set(errors)
foreach(line IN LISTS suffix_lines)
    string(REGEX REPLACE "^set\\(POTHOS_[A-Z0-9_]+_SUFFIX \"-([^\"]+)\"\\).*" "\\1" name "${line}")
    string(FIND "${simd_cpp}" "\"${name}\"" pos)
    if (pos EQUAL -1)
        list(APPEND errors "${name}")
    endif ()
endforeach()
list(LENGTH suffix_lines count)
if (count EQUAL 0)
    message(FATAL_ERROR "simd_suffix_names: no suffix found in PothosConfigSIMD.cmake")
endif ()
if (errors)
    message(FATAL_ERROR "simd_suffix_names: the CPU detection never reports these copy names: ${errors}")
endif ()
message(STATUS "simd_suffix_names: ${count} names OK")
