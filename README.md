# Pothos framework library and build environment

This is the project for the Pothos framework library: the core library,
its public headers, the PothosUtil application and the CMake support for
building toolkits. It has no submodules. The toolkits are in their own
repositories.

## Documentation

* https://github.com/pothosware/PothosCore/wiki

## Build Status

- GitHub Actions: [![CI](https://github.com/markvin/PothosCore/actions/workflows/ci.yml/badge.svg)](https://github.com/markvin/PothosCore/actions/workflows/ci.yml)

## Dependencies

The library is self-contained: its dependencies are private. The installed
headers and the CMake package of the core do not need them. Exception: the
headers Pothos/Util/XSIMDTraits.hpp and Pothos/Util/XSIMDCopy.hpp need
xsimd, and a toolkit that uses them gives its own xsimd.

* nlohmann_json 3.11 or higher
* standalone Asio 1.38 or higher
* xsimd 14.2 or higher

All three are headers only. CMake finds them with `find_package`: from the
system packages, or from Conan 2 (`conanfile.txt`). Refer to "Building".

## Toolkits

The toolkits are separate CMake projects in their own repositories. The
forks with the branch `pothos2` are ported to the pothos2 API of this
branch. The other toolkits are not yet ported:

* [PothosAudio](https://github.com/pothosware/PothosAudio)
* [PothosBlocks](https://github.com/markvin/PothosBlocks) - ported to
  pothos2 (branch `pothos2`)
* [PothosComms](https://github.com/markvin/PothosComms) - ported to
  pothos2 (branch `pothos2`)
* [PothosFlow](https://github.com/pothosware/PothosFlow)
* [PothosPlotters](https://github.com/pothosware/PothosPlotters)
* [PothosPython](https://github.com/pothosware/PothosPython)
* [PothosSoapy](https://github.com/markvin/PothosSoapy) - ported to
  pothos2 (branch `pothos2`), needs SoapySDR 0.8 or higher
* [PothosWidgets](https://github.com/markvin/PothosWidgets) - ported to
  pothos2 (branch `pothos2`), needs Qt 6 (6.5 or higher)

[PothosBase](https://github.com/markvin/PothosBase) builds the core and
the ported toolkits in one CMake tree (the repositories are submodules).

A toolkit finds the core with `find_package(Pothos)`, either installed or
next to it in the same parent project (see below).

Two kinds of topology flows insert blocks from PothosBlocks: a connection
between incompatible port domains (`/blocks/copier`) and a topology across
processes or hosts (`/blocks/network_sink`, `/blocks/network_source`). When
PothosBlocks is not installed, `Topology::commit()` throws a
`TopologyConnectError` that names the missing block.

## Building

Requirements: CMake 3.25 or newer, Ninja (or another generator) and a C++23
compiler (GCC 13+, Clang 17+, Xcode 15+, Visual Studio 2022 17.8+).
The build itself does not download files.

With system packages (for example Arch Linux: `asio nlohmann-json xsimd`):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

With Conan 2 (this step downloads the dependencies before CMake):

```sh
conan install . --output-folder=build/conan -s build_type=Release -s compiler.cppstd=23 --build=missing
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

When a dependency is missing, the configure step stops and names it.

Build options:

* `POTHOS_BUILD_APPS` (ON) - build PothosUtil (used by the self-tests and
  module safe-loading)
* `POTHOS_BUILD_DOCS` (ON for a top level build) - Doxygen documentation
  when Doxygen is found
* `POTHOS_BUILD_TESTS` (ON for a top level build) - register the CTest tests
* `POTHOS_ENABLE_LIBNUMA` (OFF) - use the system libnuma on Linux
* `POTHOS_ROOT`, `POTHOS_ROOT_ENV`, `POTHOS_APPDATA_ENV` - install root and
  the environment variables that override it and the user data directory

The core can also be a subproject of another CMake project:

```cmake
add_subdirectory(PothosCore)   # or FetchContent_MakeAvailable
target_link_libraries(myapp PRIVATE Pothos::Pothos)
add_subdirectory(PothosComms)  # its find_package(Pothos) finds the core above
```

The parent project must make the dependencies of the core available to
`find_package` (system packages, a toolchain file or `CMAKE_PREFIX_PATH`).
As a subproject its options and cache entries are prefixed `POTHOS_`, it
leaves the parent's global settings alone, registers no tests unless
`POTHOS_BUILD_TESTS=ON`, names its documentation target `pothos_docs` and
creates no uninstall target (that is the parent's). The parent and the
projects added after the core find it with `find_package(Pothos)`.

## Layout

* include/ - public API C++23 header files
* lib/ - C++ sources for the Pothos library
* docs/ - doxygen configuration and generation
* apps/ - the source for the PothosUtil binary
* cmake/ - cmake utilities for building projects
* tests/ - CTest projects (consumer, api_compat, subproject)
* conanfile.txt - the dependencies for Conan 2

## Licensing library

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)

## Licensing other

The dependencies have their own licenses (Asio: BSL-1.0, xsimd:
BSD-3-Clause, nlohmann/json: MIT). The Pothos toolkits, official and
unofficial, are separate projects and can have different licensing terms.
