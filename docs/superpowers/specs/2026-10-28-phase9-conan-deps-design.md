# Phase 9: dependencies through find_package, blocks out of the core

Date: 2026-10-28. Base branch: `pothos2` (after PR #10).
Work branch: `phase9-conan-deps`.

## Intent

The repository must not contain third party code. The core finds its build
dependencies with `find_package`. The dependencies come from the system or
from a Conan 2 install. The build scripts do not know about Conan.

The toolkits (blocks now, comms next) are separate repositories. Each toolkit
uses an installed core and declares its own dependencies in the same way.

## Decisions of the user

1. Use Conan 2 with a simple configuration: one `conanfile.txt` with
   `[requires]` and `[generators] CMakeDeps CMakeToolchain`. Do not use a
   `conanfile.py`.
2. CMake uses only `find_package`. CMake files do not contain `conan_*`
   calls, `CONAN_*` variables or checks for the Conan toolchain.
3. The dependencies come from the system. Alternatively, they come from
   Conan when the user gives
   `-DCMAKE_TOOLCHAIN_FILE=<conan output>/conan_toolchain.cmake`.
4. The toolkit modules (blocks, comms and the other toolkits) are external.
   Each module has its own `conanfile.txt`. **Phase 8 is cancelled**:
   `blocks/` goes out of the core into the fork `markvin/PothosBlocks`.
5. Delete `third_party/`.
6. The core stays self-contained. The build dependencies must not go into
   the API: the installed headers, the exported targets and
   `PothosConfig.cmake`.
7. `include/Pothos/Util/XSIMDTraits.hpp` stays public in the core. It does
   not include xsimd. A module that uses it gives its own xsimd. The
   modules do not get copies of this header.
8. The network code of blocks must not use the internals of the core.
   Add `parseUri` and `resolveHost` to the public
   `include/Pothos/Util/Network.hpp`. These functions use only `std` types.
   Asio must not be in the API.
9. CI: the 4 current jobs (linux-gcc, linux-clang, macos, windows) get
   their dependencies from Conan. One new job builds the core with system
   packages in an Arch Linux container (`asio`, `nlohmann-json` and `xsimd`
   from `extra`). This job checks the build without Conan.
10. Scope: the core and the external blocks. Comms is the next phase, with
    the same method.
11. Move `lib/SIMD/XSIMDCopy.hpp` to the public
    `include/Pothos/Util/XSIMDCopy.hpp`, next to `XSIMDTraits.hpp`. The SIMD
    kernels of blocks (and later of comms) use it with `pothos_multiarch`.
    Only a module with its own xsimd can use it. This header is the second
    documented exception to decision 6.

## Goals

- `third_party/` does not exist. The core finds `nlohmann_json`, `asio` and
  `xsimd` with `find_package`.
- The root `conanfile.txt` contains the three dependencies of the core.
- The installed `PothosConfig.cmake` does not call `find_dependency` for
  these dependencies. The exported target `Pothos::Pothos` does not refer to
  them. The test `pothos.consumer` continues to check this.
- The core builds and passes its tests with Conan and with system packages.
- The branch `pothos2` of `markvin/PothosBlocks` builds with an installed
  core and its own `conanfile.txt`. Its self-tests pass in its own CI.

## Non-goals

- Do not make a Conan package of the core or of blocks (no `conanfile.py`,
  no ConanCenter recipe).
- Do not port comms or the other toolkits (these are the next phases).
- Do not change the SIMD dispatch design or the plugin system.
- Do not change the public API, except for decisions 8 and 11.

## Anti-goals

These results are failures, also when the build is correct:

- A CMake file refers to Conan. Only the docs, the README and CI can
  refer to Conan.
- A header or target of a dependency is in the installed API of the core.
  Exceptions: `XSIMDTraits.hpp` and `XSIMDCopy.hpp` (decisions 7 and 11).
  The docs tell that these headers need the xsimd of the consumer. The
  leak check of `pothos.consumer` skips only these two headers.
- `PothosConfig.cmake` makes the consumers find the dependencies of the
  core.
- The configure step or the build step downloads files (FetchContent,
  ExternalProject, `file(DOWNLOAD)`). Conan downloads files only in a
  separate `conan install` step before CMake.
- The core loses the fixes that came with phase 8 (refer to "Cancel
  phase 8").

## Constraints and current state

- C++23 and CMake 3.25 or higher.
- The core must continue to work as a subproject (test
  `pothos.subproject`: `add_subdirectory` and FetchContent with
  `SOURCE_DIR`). The parent project makes the dependencies available.
  `cmake/PothosSubprojectTest.cmake` must give `CMAKE_TOOLCHAIN_FILE` and
  `CMAKE_PREFIX_PATH` to the nested configure step. Then the test works with
  Conan and without Conan.
- Versions:

  | Dependency | `third_party/` now | ConanCenter | Arch `extra` |
  |---|---|---|---|
  | nlohmann_json | 3.11.3 | 3.12.0 | 3.12.0 |
  | asio | 1.38.2 | 1.38.2 | 1.38.2 |
  | xsimd | 14.3.0 | 14.2.0 (no 14.3.0) | 14.3.0 |

- `third_party/CMakeLists.txt` defines the internal targets `pothos_json`,
  `pothos_asio` and `pothos_xsimd`. `pothos_asio` adds `ASIO_STANDALONE` and
  `ASIO_NO_DEPRECATED`. On Windows, it also adds `_WIN32_WINNT=0x0A00`,
  `ws2_32` and `mswsock`. `lib/CMakeLists.txt` and `apps/CMakeLists.txt` use
  these targets. `Pothos` links them as PRIVATE. `pothos_net` links
  `pothos_asio`.
- The Asio system packages (Arch, Debian, Ubuntu) do not have a CMake
  config. The Conan generator CMakeDeps makes `asio-config.cmake` with the
  target `asio::asio`. Thus the core has its own `cmake/FindAsio.cmake`. This
  module finds the headers, reads the version from `asio/version.hpp` and
  defines the same target `asio::asio`. CMake uses it only when it does not
  find a config package. This module is plain CMake. It does not refer to
  Conan.
- The core uses xsimd only internally:
  - CPU feature detection (`lib/System/SIMD.cpp`);
  - the multi-architecture buffer conversion (`lib/SIMD/SIMDConvert.cpp`
    with `XSIMDCopy.hpp`).
- The CI guard "No network access" searches the build scripts for
  FetchContent, ExternalProject and `file(DOWNLOAD)`. Keep this guard.
  Remove `third_party` and `blocks` from its list of paths. The guard also
  finds these words in comments.

### Cancel phase 8

Phase 8 (PR #10, merge `1f79019d`) added `blocks/`. It also fixed the core.

Remove:

- `blocks/`, the option `POTHOS_BUILD_BLOCKS` and the test
  `pothos.blocks.selftests`;
- the blocks text in `CLAUDE.md`, `README.md`, `Changelog.txt` and CI.

Restore the phase 7 text of the `Topology::commit()` error for a missing
`/blocks/copier` or `/blocks/network_*` ("install PothosBlocks"). This
applies to `lib/Framework/TopologyToolkitBlocks.hpp`, its test and the docs
in `Topology.hpp`.

Keep the fixes of the core from PR #10:

- `lib/Platform/Process*`, `lib/Net/TcpStream.cpp`,
  `lib/Plugin/ModuleSafeLoad.cpp`, `TestRemote.cpp`;
- `XSIMDTraits.hpp` on xsimd 14, `TestXSIMDTraits.cpp`, `XSIMDCopy.hpp`;
- the changes of the SIMD dispatcher utilities;
- the changes of `PothosUtil.cmake` and `PothosRunSelfTests.cmake` that are
  not specific to blocks.

The builder compares `af8361bc..1f79019d` outside `blocks/` and makes a
decision for each file.

## Changes in the core

- Delete `third_party/`. In `CMakeLists.txt`, replace
  `add_subdirectory(third_party)` with:
  - `find_package(nlohmann_json ... REQUIRED)`;
  - `find_package(asio ...)` (config package first, then `FindAsio.cmake`);
  - `find_package(xsimd ... REQUIRED)`.
- Link the imported targets as PRIVATE. The Asio compile definitions and
  the Windows libraries are the configuration of the core, not of the
  package. Keep them in the core. The internal wrapper targets can stay if
  they make the linking simpler. Do not install or export them.
- Minimum versions in `find_package`: not higher than the lowest version
  that CI tests (xsimd 14.2.0 from Conan). The code must compile with these
  versions. The builder selects the minimum versions of json and asio from
  the code.
- Root `conanfile.txt`:
  - `[requires]`: `nlohmann_json/3.12.0`, `asio/1.38.2`, `xsimd/14.2.0`;
  - `[generators]`: `CMakeDeps`, `CMakeToolchain`;
  - `[layout] cmake_layout` only if it makes the README command simpler
    (decision of the builder).
- Add these `POTHOS_API` functions to `include/Pothos/Util/Network.hpp`
  (namespace `Pothos::Util`, names as in the current header):
  - `parseUri(std::string_view)`. It returns a public struct
    `{std::string scheme; std::string host; std::uint16_t port;}`. The port
    is 0 when the string does not have a port. An IPv6 host has no
    brackets. The function throws `Pothos::InvalidArgumentException` for an
    incorrect string. Add `toString` for the struct if blocks needs it.
  - `resolveHost(const std::string &host, std::chrono::microseconds timeout)`.
    It returns the address as `std::string`. Its behavior is the same as
    `Pothos::Net::resolveHost`: IPv4 first, and the same timeout and IO
    exceptions.

  The functions use `lib/Net/Uri` and `lib/Net/Address`. These files stay
  internal.
- `pothos.api_compat` and `pothos.consumer` must pass. Add self-tests for
  the new functions.
- Docs:
  - The build section of the README shows the two methods: system packages,
    and `conan install` followed by `-DCMAKE_TOOLCHAIN_FILE=...`.
  - Update `CLAUDE.md` and add a Changelog entry.
  - The docs of `XSIMDTraits.hpp` and `XSIMDCopy.hpp` tell the user to
    include the xsimd of the module.

## CI of the core

- Jobs linux-gcc, linux-clang, macos and windows:
  1. Install Conan 2 with pip.
  2. Run `conan profile detect`.
  3. Run `conan install . --output-folder=... -s build_type=<config> --build=missing`.
  4. Configure with the toolchain file.

  The profile must use the compiler of the job (`CC`/`CXX` or
  `-s compiler...`). On Windows with Ninja, the generator of the toolchain
  must be Ninja. Keep `POTHOS_WARNINGS_AS_ERRORS` and the current extra
  options.
- New job `linux-system` in an `archlinux` container:
  1. Install the toolchain and `asio nlohmann-json xsimd` with `pacman -S`.
  2. Configure without a toolchain file.
  3. Build and run `ctest`.
- A cache for Conan is optional (decision of the builder).

## PothosBlocks (external)

- Repository: fork `pothosware/PothosBlocks` to `markvin/PothosBlocks`.
  Create the fork only after the user agrees at that step of the plan.
- Make the branch `pothos2` from the history of `blocks/`
  (`git subtree split --prefix=blocks` or an equivalent). The phase 8
  commits must stay. The upstream history must stay the ancestor.
- The repository has its own top-level `CMakeLists.txt`:
  - `project()` and `find_package(Pothos CONFIG REQUIRED)`;
  - `find_package` for its dependencies;
  - the helper `pothos_blocks_module()` from `blocks/CMakeLists.txt` of the
    core. Remove the include path `${PROJECT_SOURCE_DIR}/lib`. Replace
    `pothos_json` with `nlohmann_json::nlohmann_json`.
- The repository has its own `conanfile.txt` with `nlohmann_json`, `asio`
  and `xsimd` (the same versions as the core). The core is not a Conan
  package. The core comes from the system, or from `CMAKE_PREFIX_PATH` that
  points to an installed core.
- Replace the uses of the core internals:
  - `Net/Uri.hpp`, `Net/Address.hpp` and `Pothos::Net::ipv6Supported`:
    use `Pothos/Util/Network.hpp` (`parseUri`, `resolveHost`,
    `isIPv6Supported`) and the Asio of blocks (`asio::ip::make_address`);
  - `SIMD/XSIMDCopy.hpp` (4 files in `stream/SIMD/`): use
    `<Pothos/Util/XSIMDCopy.hpp>` (decision 11);
  - `Util/TempFile.hpp` (`file/TestBinaryFileBlocks.cpp`): use a small
    local helper in the test.
- `FindAsio.cmake`: blocks gets its own copy, or the core installs its
  copy (decision of the builder). `PothosConfig.cmake` must not need Asio.
- Tests:
  1. Install blocks into a stage prefix.
  2. For each blocks test, run `PothosUtil --self-test1`. Set `POTHOS_ROOT`
     to the installed core. Set `POTHOS_PLUGIN_PATH` to the staged blocks
     modules. Alternatively, install blocks into the prefix of the core in
     CI.

  Use the method of `PothosRunSelfTests.cmake` of the core. A copy of this
  CMake script is permitted.
- CI of blocks:
  1. Build and install the core from `markvin/PothosCore` (Conan).
  2. Build blocks (Conan).
  3. Run the tests.

  Linux is mandatory. Use the 4-OS matrix if the cost is low (decision of
  the builder).

## Decisions of the designer (the user must check them)

- Phase 9 is one PR into `pothos2`. Before the merge, the CI of blocks
  builds the core from `markvin/PothosCore@phase9-conan-deps`. After the
  merge, it builds the core from `pothos2`.
- The user approves these steps before they occur: the fork, the first push
  of each branch (core, blocks), the PR and the merge.
- The minimum version of xsimd is 14.2.0 (the newest version on
  ConanCenter). If the core uses a feature of 14.3, change the internal code
  to use only 14.2. Do not make a custom Conan recipe. `XSIMDTraits.hpp`
  needs only `has_simd_register`.
- A local helper replaces `Util/TempFile.hpp` in one blocks test.

## Recommended order of work

1. Core: `find_package`, `FindAsio.cmake` and `conanfile.txt`. Delete
   `third_party/`. Keep blocks in the tree, so that the tests stay green.
2. Core: the public `parseUri`, `resolveHost` and
   `Pothos/Util/XSIMDCopy.hpp`. Change the blocks code to use them. Replace
   `TempFile.hpp` in the blocks test.
3. Move the blocks history to `markvin/PothosBlocks@pothos2`. Add its build
   files, its `conanfile.txt` and its CI.
4. Core: remove `blocks/` and the phase 8 integration. Update the docs and
   the Changelog.
5. CI: Conan in the 4 jobs and the new job `linux-system`.
