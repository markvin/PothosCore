########################################################################
# A minimal external project built against an installed Pothos.
# It proves the package is usable on its own and that no private
# dependency of the library leaks into the exported target.
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosConsumer CXX)

find_package(Pothos CONFIG REQUIRED)

foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LINK_LIBRARIES INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(val Pothos::Pothos ${prop})
    if(val MATCHES "nlohmann|xsimd|asio")
        message(FATAL_ERROR "Pothos::Pothos leaks a private dependency via ${prop}: ${val}")
    endif()
endforeach()

#the installed include directory must hold nothing but the Pothos headers,
#otherwise a public header could silently depend on a private library
get_target_property(pothos_include_dir Pothos::Pothos INTERFACE_INCLUDE_DIRECTORIES)
file(GLOB include_entries RELATIVE "${pothos_include_dir}" "${pothos_include_dir}/*")
if (NOT include_entries STREQUAL "Pothos")
    message(FATAL_ERROR "installed include directory has more than Pothos/: ${include_entries}")
endif ()

#compile every public header on its own
file(GLOB_RECURSE public_headers RELATIVE "${pothos_include_dir}" "${pothos_include_dir}/Pothos/*.hpp")
set(header_sources)
foreach(header ${public_headers})
    string(MAKE_C_IDENTIFIER "${header}" name)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/headers/${name}.cpp")
    file(WRITE "${source}" "#include <${header}>\n")
    list(APPEND header_sources "${source}")
endforeach()

add_executable(consumer main.cpp ${header_sources})
target_link_libraries(consumer PRIVATE Pothos::Pothos)

#a fixed location for the executable with multi-config generators too
set_target_properties(consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin$<0:>")
