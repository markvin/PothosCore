########################################################################
# Third party libraries of the core: found with find_package, from the
# system or from a package manager that gives a toolchain file.
# All three are headers only. The wrapper targets below are private
# implementation details of the Pothos library: they are never installed
# or exported, and PothosConfig.cmake does not need these packages.
########################################################################
set(pothos_missing_dependencies)

find_package(nlohmann_json 3.11 CONFIG)
if(NOT nlohmann_json_FOUND)
    list(APPEND pothos_missing_dependencies "nlohmann_json (>= 3.11)")
endif()

#Asio: a package configuration when there is one, else the headers
find_package(asio 1.38 CONFIG QUIET)
if(asio_FOUND)
    message(STATUS "Asio: config package (${asio_DIR})")
else()
    set(pothos_saved_module_path ${CMAKE_MODULE_PATH})
    list(PREPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
    find_package(asio 1.38 MODULE)
    set(CMAKE_MODULE_PATH ${pothos_saved_module_path})
    if(asio_FOUND)
        message(STATUS "Asio: headers (Findasio.cmake, ${asio_INCLUDE_DIR})")
    else()
        list(APPEND pothos_missing_dependencies "asio (>= 1.38)")
    endif()
endif()

find_package(xsimd 14.2 CONFIG)
if(NOT xsimd_FOUND)
    list(APPEND pothos_missing_dependencies "xsimd (>= 14.2)")
endif()

if(pothos_missing_dependencies)
    list(JOIN pothos_missing_dependencies ", " pothos_missing_text)
    message(FATAL_ERROR
        "Pothos: missing build dependencies: ${pothos_missing_text}.\n"
        "Install the system packages (for example: asio, nlohmann-json, xsimd), "
        "or install them with a package manager and give its toolchain file "
        "(-DCMAKE_TOOLCHAIN_FILE=...) or CMAKE_PREFIX_PATH. Refer to README.md.")
endif()

#nlohmann/json
add_library(pothos_json INTERFACE)
target_link_libraries(pothos_json INTERFACE nlohmann_json::nlohmann_json)

#standalone Asio: used by lib/Net
add_library(pothos_asio INTERFACE)
target_link_libraries(pothos_asio INTERFACE asio::asio)
target_compile_definitions(pothos_asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
if (WIN32)
    target_compile_definitions(pothos_asio INTERFACE _WIN32_WINNT=0x0A00)
    target_link_libraries(pothos_asio INTERFACE ws2_32 mswsock)
endif ()

#xsimd: CPU features, SIMD conversion
add_library(pothos_xsimd INTERFACE)
target_link_libraries(pothos_xsimd INTERFACE xsimd)
