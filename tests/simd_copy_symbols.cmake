########################################################################
# Check the SIMD copies of the core (cmake -P script, Linux, GCC/Clang):
# a copy must define no weak symbol outside its own namespace and xsimd.
# The linker keeps one definition of a weak symbol (an inline std::
# function, for example), and that definition can come from an AVX copy:
# then a fallback path runs AVX code on a computer without AVX.
#
# Required: -DNM=<nm> -DOBJECTS=<|-separated object files of the target>
########################################################################
cmake_minimum_required(VERSION 3.25) #policies for cmake -P scripts
foreach(var NM OBJECTS)
    if (NOT DEFINED ${var})
        message(FATAL_ERROR "simd_copy_symbols: ${var} is not set")
    endif ()
endforeach()
string(REPLACE "|" ";" objects "${OBJECTS}")
set(errors)
set(count 0)
foreach(object IN LISTS objects)
    #the copies of a multiarch source: <name>-<hash>.cpp.o
    if (NOT object MATCHES "-[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]\\.cpp\\.o$")
        continue()
    endif ()
    math(EXPR count "${count}+1")
    execute_process(COMMAND ${NM} -C --defined-only "${object}" OUTPUT_VARIABLE symbols RESULT_VARIABLE rc)
    if (NOT rc EQUAL 0)
        message(FATAL_ERROR "simd_copy_symbols: ${NM} failed for ${object}")
    endif ()
    string(REPLACE "\n" ";" lines "${symbols}")
    foreach(line IN LISTS lines)
        #"<address> <type> <name>": W, V (weak) and u (unique global) are shared between copies
        if (NOT line MATCHES "^[0-9a-f]+ [WVu] (.*)$")
            continue()
        endif ()
        set(name "${CMAKE_MATCH_1}")
        if (name MATCHES "(^| )(PothosSIMDConvert|Pothos[A-Za-z]*SIMD)::|xsimd")
            continue()
        endif ()
        get_filename_component(file "${object}" NAME)
        list(APPEND errors "${file}: ${name}")
    endforeach()
endforeach()
if (count EQUAL 0)
    message(FATAL_ERROR "simd_copy_symbols: no SIMD copy object in the list")
endif ()
if (errors)
    string(REPLACE ";" "\n  " text "${errors}")
    message(FATAL_ERROR "simd_copy_symbols: weak symbols outside the copy namespace:\n  ${text}")
endif ()
message(STATUS "simd_copy_symbols: ${count} copies OK")
