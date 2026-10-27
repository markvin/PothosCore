# Pothos framework library and build environment

This is the project for the Pothos framework library.
It also contains submodules for various dependencies
and Pothos toolkits to make it easy to install
the entire Pothos software suite in one build.

## Documentation

* https://github.com/pothosware/PothosCore/wiki

## Build Status

- GitHub Actions: [![CI](https://github.com/markvin/PothosCore/actions/workflows/ci.yml/badge.svg)](https://github.com/markvin/PothosCore/actions/workflows/ci.yml)

## Dependencies

The framework is being made self-contained: every dependency it needs
is built from the source tree and kept private to the library.

* poco C++ utility library (transitional submodule, being removed;
  only its Net and Util libraries are still used)
* libsimdpp SIMD library (submodule, header only)
* nlohmann/json (vendored in third_party/)
* spuce filter design library (toolkit dependency, not used by the framework)

## Toolkits

Although all toolkits can be built as standalone cmake projects,
user may wish to build these toolkits alongside the framework
library to simplfy the effort involved building from source.
The following toolkits are available as git submodules:

* PothosAudio
* PothosBlocks
* PothosComms
* PothosFlow
* PothosPlotters
* PothosPython
* PothosSoapy
* PothosWidgets

## Building

Requirements: CMake 3.25 or newer, Ninja (or another generator) and a C++23
compiler (GCC 13+, Clang 17+, Xcode 15+, Visual Studio 2022 17.8+).
The build needs no network access and no system libraries.

```sh
git submodule update --init poco libsimdpp
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --install build
```

Only the framework library is built; the toolkit submodules are not part
of this build.

## Layout

* include/ - public API C++23 header files
* lib/ - C++ sources for the Pothos library
* docs/ - doxygen configuration and generation
* apps/ - the source for the PothosUtil binary
* cmake/ - cmake utilities for building projects

## Licensing library

Use, modification and distribution is subject to the Boost Software
License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)

## Licensing other

This repository is a meta-project for developing, building, and installing
the main Pothos framework, core dependency libraries, and official toolkits.
License files and source code are available in each project sub-directory.

The core Pothos library, Poco and libsimdpp C++ libraries,
and official toolkits found in this repository
are available under the Boost Software License - Version 1.0.

Please note that Pothos toolkits that are NOT found in this repository,
both official and unofficial, may have different licensing terms.

Several external libraries have been included into this meta-project
that are not licensed under the Boost Software License - Version 1.0,
however are available under a permissive compatible software license.

### kissfft

* Description: C and C++ Fast Fourier Transform library
* Homepage: https://github.com/itdaniher/kissfft/blob/master/README
* License: https://github.com/itdaniher/kissfft/blob/master/COPYING
* Usage: Pothos comms FFT transform blocks
* Source: comms/fft/ (embedded copy)

### Qwt

* Description: C++ plotting library based around Qt widgets
* Homepage: http://qwt.sourceforge.net/
* License: http://qwt.sourceforge.net/qwtlicense.html
* Usage: Pothos plotters toolkit support library
* Source: plotters/qwt/ (nested submodule)

### qtcolorpicker

* Description: C++ drop-down color selection Qt widget
* License: GNU Lesser General Public License Usage
* Usage: Pothos GUI designer affinity color selection
* Source: flow/qtcolorpicker/ (embedded copy)
