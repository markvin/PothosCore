########################################################################
# Sources written against the pre-C++23 Pothos API, built against an
# installed Pothos. They prove that typical toolkit code still compiles
# and links after the public API modernization.
########################################################################
cmake_minimum_required(VERSION 3.25)
project(PothosApiCompat CXX)

find_package(Pothos CONFIG REQUIRED)

add_executable(api_compat
    main.cpp
    Block.cpp
    SpinLock.cpp
    Templates.cpp
    Testing.cpp)
target_link_libraries(api_compat PRIVATE Pothos::Pothos)

#a fixed location for the executable with multi-config generators too
set_target_properties(api_compat PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin$<0:>")
