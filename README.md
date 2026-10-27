# Pothos framework library and build environment

This is the project for the Pothos framework library: the core library,
its public headers, the PothosUtil application and the CMake support for
building toolkits. It has no submodules; the toolkits live in their own
repositories.

## Documentation

* https://github.com/pothosware/PothosCore/wiki

## Build Status

- GitHub Actions: [![CI](https://github.com/markvin/PothosCore/actions/workflows/ci.yml/badge.svg)](https://github.com/markvin/PothosCore/actions/workflows/ci.yml)

## Dependencies

The framework is being made self-contained: every dependency it needs
is built from the source tree and kept private to the library.

* nlohmann/json (vendored in third_party/)
* standalone Asio networking library (vendored in third_party/asio)
* xsimd SIMD library (vendored in third_party/xsimd)

## Toolkits

The toolkits are separate CMake projects in their own repositories
(they are not yet ported to the pothos2 API of this branch):

* [PothosAudio](https://github.com/pothosware/PothosAudio)
* [PothosBlocks](https://github.com/pothosware/PothosBlocks)
* [PothosComms](https://github.com/pothosware/PothosComms)
* [PothosFlow](https://github.com/pothosware/PothosFlow)
* [PothosPlotters](https://github.com/pothosware/PothosPlotters)
* [PothosPython](https://github.com/pothosware/PothosPython)
* [PothosSoapy](https://github.com/pothosware/PothosSoapy)
* [PothosWidgets](https://github.com/pothosware/PothosWidgets)

A toolkit finds the core with `find_package(Pothos)`, either installed or
next to it in the same parent project (see below).

The core itself is self-contained, but two kinds of topology flows insert
blocks from PothosBlocks: a connection between incompatible port domains
(`/blocks/copier`) and a topology across hosts (`/blocks/network_sink`,
`/blocks/network_source`). Without PothosBlocks installed, `Topology::commit()`
throws a `TopologyConnectError` that names the missing block.

## Building

Requirements: CMake 3.25 or newer, Ninja (or another generator) and a C++23
compiler (GCC 13+, Clang 17+, Xcode 15+, Visual Studio 2022 17.8+).
The build needs no network access and no system libraries.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

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
add_subdirectory(PothosBlocks) # its find_package(Pothos) finds the core above
```

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
* third_party/ - vendored dependencies (private to the library)

## Licensing library

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)

## Licensing other

The libraries vendored in third_party/ keep their own licenses
(Asio: BSL-1.0, xsimd: BSD-3-Clause, nlohmann/json: MIT; see each directory).

Pothos toolkits, official and unofficial, are separate projects
and may have different licensing terms.
