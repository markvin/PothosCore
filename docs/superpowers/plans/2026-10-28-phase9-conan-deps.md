# Phase 9: dependencies through find_package, blocks out of the core — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The core finds nlohmann_json, Asio and xsimd with `find_package`
(system or Conan 2). `third_party/` and `blocks/` go out of the core. Blocks
builds in the fork `markvin/PothosBlocks` against an installed core.

**Architecture:** One new CMake file `cmake/PothosDependencies.cmake` finds
the three packages and keeps the internal wrapper targets `pothos_json`,
`pothos_asio` and `pothos_xsimd`. Thus `lib/` and `apps/` do not change. Blocks
stops using core internals before the split: it uses the new public
`Pothos::Util::parseUri`/`resolveHost` and the public
`<Pothos/Util/XSIMDCopy.hpp>`. Then `git subtree split` moves `blocks/` with
its history to the fork, and the core removes `blocks/`.

**Tech Stack:** CMake ≥ 3.25, C++23, Conan 2 (`conanfile.txt`, CMakeDeps,
CMakeToolchain), nlohmann_json 3.12.0, Asio 1.38.2, xsimd 14.2.0 (Conan) /
14.3.0 (Arch), GitHub Actions, `git subtree`.

**Spec:** `docs/superpowers/specs/2026-10-28-phase9-conan-deps-design.md`

## Global Constraints

- CMake files do not refer to Conan: no `conan_*` calls, no `CONAN_*`
  variables, no word "conan" in `CMakeLists.txt`, `cmake/`, `lib/`, `apps/`.
- No download at configure or build time: no FetchContent, ExternalProject
  or `file(DOWNLOAD)` in `CMakeLists.txt lib apps cmake` (CI guard). This
  also applies to comments.
- `pothos_json`, `pothos_asio`, `pothos_xsimd`, `pothos_net` are never
  installed or exported. The installed `PothosConfig.cmake` does not call
  `find_dependency`.
- `Pothos::Pothos` interface properties do not match `nlohmann|xsimd|asio`
  (`tests/consumer`).
- Only two public headers need xsimd: `Pothos/Util/XSIMDTraits.hpp` and
  `Pothos/Util/XSIMDCopy.hpp`.
- `conanfile.txt` versions: `nlohmann_json/3.12.0`, `asio/1.38.2`,
  `xsimd/14.2.0`. `find_package` minimums: nlohmann_json 3.11, asio 1.38,
  xsimd 14.2.
- The plugin paths and block names of blocks do not change
  (`/blocks/copier`, `/blocks/network_*`, `/blocks/tests/test_*`).
- English text (comments, docs, commit messages, PR text) is in ASD-STE100
  (refer to `CLAUDE.md`).
- Approval points (ask the user first): the fork `markvin/PothosBlocks`,
  the first push of each branch, each PR, each merge.

## Review Focus

1. A user configures without the dependencies (no system package, no
   toolchain file). Expected: a fatal error that names the missing package
   and tells the two methods (system package, `conan install` +
   `-DCMAKE_TOOLCHAIN_FILE`). Test: Task 1, Step 5.
2. Asio is available as a config package (Conan) and as plain headers.
   Expected: CMake uses the config package; `Findasio.cmake` is only the
   fallback. Test: Task 1, Steps 4 and 6 (the configure log tells the mode).
3. `parseUri` gets bad input: no scheme, empty host, port 99999, a path,
   an IPv6 address without brackets. Expected: `InvalidArgumentException`,
   no crash, no truncated port. Test: Task 2, Step 1.
4. `resolveHost` gets a name that does not resolve, or a literal address.
   Expected: `IOException` for the bad name; the literal comes back
   unchanged and `asio::ip::make_address` accepts it. Test: Task 2, Step 1.
5. A consumer project without xsimd, Asio and json uses the installed core.
   Expected: all public headers except `XSIMDCopy.hpp` compile on their own.
   `XSIMDCopy.hpp` is the only skipped header. Test: Task 3, Step 1.

---

### Task 1: The core finds its dependencies with find_package

**Files:**
- Create: `cmake/PothosDependencies.cmake`, `cmake/Findasio.cmake` (lower case:
  `find_package(asio MODULE)` looks for `Findasio.cmake`), `conanfile.txt`
- Modify: `CMakeLists.txt:106-109` (replace `add_subdirectory(third_party)`),
  `CMakeLists.txt:199-208` (`pothos.subproject` arguments),
  `cmake/PothosSubprojectTest.cmake:24-33`, `.gitignore`, `.github/workflows/ci.yml`
- Delete: `third_party/`

**Interfaces:**
- Produces: the INTERFACE targets `pothos_json`, `pothos_asio`,
  `pothos_xsimd` with the same names and usage as before (`lib/`, `apps/`
  and `blocks/` do not change). `pothos_asio` links `asio::asio` and keeps
  `ASIO_STANDALONE`, `ASIO_NO_DEPRECATED` and, on Windows,
  `_WIN32_WINNT=0x0A00`, `ws2_32`, `mswsock`.
- Produces: `cmake/Findasio.cmake` defines `asio_FOUND`, `asio_VERSION`
  and the IMPORTED INTERFACE target `asio::asio`. Task 4 copies it to blocks.

- [ ] **Step 1: Get the dependencies from Conan**

Write `conanfile.txt` with `[requires]` `nlohmann_json/3.12.0`,
`asio/1.38.2`, `xsimd/14.2.0` and `[generators]` `CMakeDeps`,
`CMakeToolchain`. Do not add `[layout]`. Then run:

```sh
conan profile detect --exist-ok
conan install . --output-folder=build/conan -s build_type=Debug -s compiler.cppstd=23 --build=missing
```

Expected: `build/conan/conan_toolchain.cmake` exists. Add `build/` to
`.gitignore` if it is not there.

- [ ] **Step 2: Write `cmake/Findasio.cmake`**

Find `asio.hpp` with `find_path`, read `ASIO_VERSION` from
`asio/version.hpp` (format `XXYYZZ` → `X.Y.Z`), use
`find_package_handle_standard_args(asio REQUIRED_VARS asio_INCLUDE_DIR VERSION_VAR asio_VERSION)`,
create `asio::asio` (IMPORTED INTERFACE) when found and not defined.

- [ ] **Step 3: Write `cmake/PothosDependencies.cmake` and include it**

Order: `find_package(nlohmann_json 3.11 CONFIG)`, `find_package(asio 1.38 CONFIG QUIET)`,
then `find_package(asio 1.38 MODULE)` with `${PROJECT_SOURCE_DIR}/cmake`
in `CMAKE_MODULE_PATH` (local to this file, restore it after),
`find_package(xsimd 14.2 CONFIG)`. Print one status line for Asio:
`Asio: config package` or `Asio: headers (Findasio.cmake)`. For each package
that is not found, add its name to a list. When the list is not empty, stop
with `message(FATAL_ERROR)`. The text names the missing packages and tells
the two methods: install the system packages, or run `conan install` and
give `-DCMAKE_TOOLCHAIN_FILE=<output>/conan_toolchain.cmake`. Then define
the three wrapper targets (Interfaces above). In `CMakeLists.txt`, replace
`add_subdirectory(third_party)` with
`include(${PROJECT_SOURCE_DIR}/cmake/PothosDependencies.cmake)`. Delete
`third_party/` with `git rm -r`.

- [ ] **Step 4: Build and test with Conan (xsimd 14.2.0)**

```sh
cmake -S . -B build/conan-dbg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPOTHOS_WARNINGS_AS_ERRORS=ON -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake
cmake --build build/conan-dbg
ctest --test-dir build/conan-dbg --output-on-failure
```

Expected: the log shows `Asio: config package`; all tests pass, except
`pothos.subproject` (Step 6 repairs it).

Known incompatibility (checked 2026-10-28: core and blocks with xsimd 14.2.0,
all 7 tests pass except for this one call): xsimd 14.2 has no
`cpu_features::avx512vl()` (added in 14.3). In `lib/System/SIMD.cpp`, add a
function `static bool cpuHasAVX512VL(const xsimd::cpu_features &cpu)`:
- If `XSIMD_VERSION_MAJOR * 100 + XSIMD_VERSION_MINOR >= 1403`, return
  `cpu.avx512vl()`.
- If not, on x86 read CPUID leaf 7, subleaf 0, EBX bit 31 (`__cpuid_count`
  from `<cpuid.h>` on GCC/Clang, `__cpuidex` from `<intrin.h>` on MSVC).
  On other architectures, return false.

The caller already requires `avx512f`, which checks the OS state (XCR0).
Check: this machine has AVX-512VL. Thus `PothosUtil --simd-features` must
show `x86_avx512vl` with xsimd 14.2.0 (Conan) and with 14.3.0 (system).
Do not change the minimum version.

- [ ] **Step 5: Check the error without dependencies**

```sh
cmake -S . -B build/nodeps -G Ninja -DCMAKE_DISABLE_FIND_PACKAGE_xsimd=ON
```

Expected: configure fails, and the text names `xsimd` and the two methods.

- [ ] **Step 6: Give the dependencies to the nested configure of `pothos.subproject`**

In `CMakeLists.txt`, give `-DTOOLCHAIN=${CMAKE_TOOLCHAIN_FILE}` and
`-DPREFIX_PATH=<CMAKE_PREFIX_PATH with ; escaped>` to the test. In
`configure_parent()`, add `-DCMAKE_TOOLCHAIN_FILE=` and
`-DCMAKE_PREFIX_PATH=` only when they are not empty. Do not change
`pothos.consumer` and `pothos.api_compat`: they must work without the
dependencies. This is the leak check.
Run: `ctest --test-dir build/conan-dbg -R pothos.subproject --output-on-failure`.
Expected: PASS.

- [ ] **Step 7: Build with system packages**

The local Arch system has `asio` and `nlohmann-json`, but not `xsimd`. Ask the
user to run `! sudo pacman -S xsimd`. If the user does not agree, skip this step.
The CI job `linux-system` (Step 8) then does the check.

```sh
cmake -S . -B build/system -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPOTHOS_WARNINGS_AS_ERRORS=ON
cmake --build build/system && ctest --test-dir build/system --output-on-failure
```

Expected: `Asio: headers (Findasio.cmake)`; all tests pass.

- [ ] **Step 8: Change CI.** (Before the first push: without this step, CI fails because `third_party/` is gone.) For the jobs linux-gcc, linux-clang, macos and windows:
  1. Add the step `pip install conan` (Python from the runner image).
  2. Run `conan profile detect`.
  3. Run `conan install . --output-folder=build/conan -s build_type=${{ matrix.config }} -s compiler.cppstd=23 --build=missing -c tools.cmake.cmaketoolchain:generator=Ninja`.
  4. Add `-DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake` to Configure.

  Linux: give `CC`/`CXX` of the matrix in the `env` of the Conan steps, so that the profile
  detects that compiler. Add the job `linux-system` with `container: archlinux:latest`:
  1. Run `pacman -Syu --noconfirm base-devel cmake ninja git asio nlohmann-json xsimd`.
  2. Configure without a toolchain file and with `-DPOTHOS_WARNINGS_AS_ERRORS=ON`.
  3. Build and run `ctest`.

  Remove `third_party` from the guard paths.

- [ ] **Step 9: Commit**

```sh
git add -A cmake CMakeLists.txt conanfile.txt .gitignore third_party .github
git commit -m "build: find nlohmann_json, Asio and xsimd with find_package; remove third_party"
```

---

### Task 2: Public parseUri and resolveHost; blocks network code uses them

**Files:**
- Modify: `include/Pothos/Util/Network.hpp`, `lib/Util/Network.cpp`
- Create: `lib/Util/Builtin/TestNetworkUtils.cpp` (add to `lib/CMakeLists.txt` next to the other `Util/Builtin` tests)
- Modify: `blocks/network/SocketEndpoint.cpp`, `blocks/network/DatagramIO.cpp`,
  `blocks/network/TestNetworkBlocks.cpp`, `blocks/network/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `Pothos::Util`, all `POTHOS_API`):
  ```cpp
  struct Uri { std::string scheme; std::string host; std::uint16_t port = 0; };
  POTHOS_API Uri parseUri(std::string_view uri);
  POTHOS_API std::string toString(const Uri &uri);
  POTHOS_API std::string resolveHost(const std::string &host, std::chrono::microseconds timeout);
  ```
  The implementation calls `Pothos::Net::parseUri`/`toString`/`resolveHost`
  (`.to_string()` of the address). `Network.hpp` includes only `std` headers
  and `Pothos/Config.hpp`.

- [ ] **Step 1: Write the failing self-tests** (`/util/tests`)

```cpp
POTHOS_TEST_BLOCK("/util/tests", test_util_parse_uri)
{
    auto u = Pothos::Util::parseUri("tcp://[::1]:1234");
    POTHOS_TEST_EQUAL(u.scheme, "tcp"); POTHOS_TEST_EQUAL(u.host, "::1"); POTHOS_TEST_EQUAL(u.port, 1234);
    POTHOS_TEST_EQUAL(Pothos::Util::parseUri("udp://localhost").port, 0);
    POTHOS_TEST_EQUAL(Pothos::Util::toString(u), "tcp://[::1]:1234");
    for (const auto *bad : {"localhost:80", "tcp://", "tcp://h:99999", "tcp://h:80/path", "tcp://::1:80"})
        POTHOS_TEST_THROWS(Pothos::Util::parseUri(bad), Pothos::InvalidArgumentException);
}
POTHOS_TEST_BLOCK("/util/tests", test_util_resolve_host)
{
    POTHOS_TEST_EQUAL(Pothos::Util::resolveHost("127.0.0.1", std::chrono::seconds(5)), "127.0.0.1");
    POTHOS_TEST_TRUE(not Pothos::Util::resolveHost("localhost", std::chrono::seconds(5)).empty());
    POTHOS_TEST_THROWS(Pothos::Util::resolveHost("no-such-host.invalid", std::chrono::seconds(10)), Pothos::IOException);
}
```

Before you write the bad-input cases, check `lib/Net/Builtin/TestNetAddress.cpp` (`test_net_uri`). Each case must throw in `Pothos::Net::parseUri`. If a case does not throw, remove it and record this in the commit message. Do not change the parser.

- [ ] **Step 2: Run, expect a build failure** (`parseUri` is not declared)

- [ ] **Step 3: Implement** in `Network.hpp` / `Network.cpp`. Add Doxygen comments with the exceptions.

- [ ] **Step 4: Run the tests**

```sh
cmake --build build/conan-dbg && cmake -DPREFIX=build/conan-dbg/stage -DTEST_HOME_DIR=build/conan-dbg/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/util/tests/test_util_parse_uri -P cmake/PothosRunSelfTests.cmake
```

Run `ctest --test-dir build/conan-dbg -R pothos.stage` first to stage the
install. Do the same for `test_util_resolve_host`. Expected: PASS.

- [ ] **Step 5: Change the blocks network code**

Replace `Pothos::Net::parseUri` → `Pothos::Util::parseUri`,
`Pothos::Net::resolveHost(h, t)` → `asio::ip::make_address(Pothos::Util::resolveHost(h, t))`,
`Pothos::Net::ipv6Supported()` → `Pothos::Util::isIPv6Supported()`. Remove the
includes `"Net/Uri.hpp"` and `"Net/Address.hpp"`. In `blocks/network/CMakeLists.txt`,
replace `pothos_net` with `pothos_asio`.
Run: `ctest --test-dir build/conan-dbg -R "pothos.(selftests|blocks.selftests)" --output-on-failure`.
Expected: PASS.

- [ ] **Step 6: Commit**

```sh
git commit -am "util: public parseUri and resolveHost; blocks network uses them"
```

(`git add` the new test file first.)

---

### Task 3: Public XSIMDCopy.hpp; blocks uses no core internals

**Files:**
- Move: `lib/SIMD/XSIMDCopy.hpp` → `include/Pothos/Util/XSIMDCopy.hpp`
- Modify: `lib/SIMD/SIMDConvert.cpp:9`, `blocks/stream/SIMD/{IsX,Round,MinMax,Clamp}.cpp:6`,
  `tests/consumer/CMakeLists.txt:26-33`, `include/Pothos/Util/XSIMDTraits.hpp` (doc comment),
  `blocks/file/TestBinaryFileBlocks.cpp`, `blocks/CMakeLists.txt:24-27`, `CLAUDE.md` (SIMD line)

**Interfaces:**
- Produces: `#include <Pothos/Util/XSIMDCopy.hpp>`. Behavior and macros
  (`POTHOS_SIMD_NAMESPACE`, the AVX-512F workaround, the per-copy `xsimd` namespace)
  do not change. The header comment says: "Public. Include it first in a file
  that pothos_multiarch compiles; the module gives its own xsimd."

- [ ] **Step 1: Write the failing check in the consumer test**

In `tests/consumer/CMakeLists.txt`, before the loop over the headers:

```cmake
#XSIMDCopy.hpp needs the module's xsimd and POTHOS_SIMD_NAMESPACE (pothos_multiarch)
list(REMOVE_ITEM public_headers "Pothos/Util/XSIMDCopy.hpp")
if (NOT EXISTS "${pothos_include_dir}/Pothos/Util/XSIMDCopy.hpp")
    message(FATAL_ERROR "Pothos/Util/XSIMDCopy.hpp is not installed")
endif ()
```

- [ ] **Step 2: Run `ctest --test-dir build/conan-dbg -R pothos.consumer`. Expected:** FAIL ("XSIMDCopy.hpp is not installed").

- [ ] **Step 3: Move the header** with `git mv` and change the 5 includes.
  Update the comment in the header and in `XSIMDTraits.hpp`.

- [ ] **Step 4: Remove the TempFile dependency.** In `TestBinaryFileBlocks.cpp`,
  replace `Pothos::Util::Detail::TempFile` with a local RAII struct. The struct
  makes a unique path in `std::filesystem::temp_directory_path()` and removes
  the file in its destructor.

- [ ] **Step 5: Prove that blocks uses no core internals.** In `pothos_blocks_module()`,
  remove `${PROJECT_SOURCE_DIR}/lib` from `target_include_directories`. Then search:
  `grep -rn '"Net/\|"SIMD/\|"Util/\|pothos_net' blocks`. Expected: no result.

- [ ] **Step 6: Build and run all tests**

`cmake --build build/conan-dbg && ctest --test-dir build/conan-dbg --output-on-failure`. Expected: all PASS.

- [ ] **Step 7: Commit**

```sh
git commit -am "simd: public Pothos/Util/XSIMDCopy.hpp; blocks needs no core internals"
```

---

### Task 4: PothosBlocks as a separate repository

**Files (in `/home/mark/Projects/PothosBlocks`, branch `pothos2`):**
- Create: `CMakeLists.txt` (top level), `conanfile.txt`, `cmake/Findasio.cmake` (copy of the core),
  `cmake/PothosTestEnv.cmake` and `cmake/PothosRunSelfTests.cmake` (copies of the core),
  `.github/workflows/ci.yml`, `README.md` (build section)
- Modify: the old `CMakeLists.txt` of `blocks/` becomes the body of the top-level file

**Interfaces:**
- Consumes: the installed core (`find_package(Pothos CONFIG REQUIRED)`, `POTHOS_MODULE_UTIL`,
  `pothos_multiarch`, `PothosGenerateSIMDSources`, `POTHOS_UTIL_EXE`), `Pothos::Util::parseUri`,
  `resolveHost`, `<Pothos/Util/XSIMDCopy.hpp>`.
- Produces: the wrapper targets of blocks: `blocks_json` → `nlohmann_json::nlohmann_json`,
  `blocks_asio` → `asio::asio` + the same compile definitions as `pothos_asio`,
  `blocks_xsimd` → `xsimd`. The CTest test `blocks.selftests` runs every name in `TESTS`.

- [ ] **Step 1: Split the history.** In the core worktree:
  `git subtree split --prefix=blocks -b phase9-blocks-split`. Check:
  `git log --oneline phase9-blocks-split | wc -l` is about 600, and the
  upstream commit `4aef23f` is an ancestor
  (`git merge-base --is-ancestor 4aef23f phase9-blocks-split`).

- [ ] **Step 2: Make the local repository.** `git init /home/mark/Projects/PothosBlocks`, then
  `git fetch <core worktree> phase9-blocks-split:pothos2` and `git checkout pothos2`.
  Add `upstream` = `https://github.com/pothosware/PothosBlocks.git`.

- [ ] **Step 3: Write the build files.** The top-level `CMakeLists.txt` uses `cmake_minimum_required(VERSION 3.25)`,
  `project(PothosBlocks CXX)`, `find_package(Pothos CONFIG REQUIRED)` and the same
  three `find_package` calls with the same minimums as in Task 1 (Asio: config, then
  `Findasio.cmake`). It also has `pothos_blocks_module()` from Task 3 with `blocks_json`
  instead of `pothos_json`, the include path of the repository root only, and
  `POTHOS_WARNINGS_AS_ERRORS`. In the module `CMakeLists.txt` files, replace
  `pothos_asio` → `blocks_asio` and `pothos_xsimd` → `blocks_xsimd`.
  Tests (when `BUILD_TESTING`): `blocks.stage` installs into `${PROJECT_BINARY_DIR}/stage`.
  `blocks.selftests` runs `PothosRunSelfTests.cmake` with `PREFIX` = the core prefix
  (directory of `POTHOS_UTIL_EXE`/..) and `TEST_PATHS`. It also sets
  `POTHOS_PLUGIN_PATH` = `stage/${CMAKE_INSTALL_LIBDIR}/Pothos/modules${POTHOS_ABI_VERSION}`.
  On Windows, add the staged `bin` to `PATH`. Add an optional `-DPLUGIN_PATH=` to
  the copied runner. `conanfile.txt` is the same as in the core.

- [ ] **Step 4: Build and test against the staged core**

```sh
conan install . --output-folder=build/conan -s build_type=Debug -s compiler.cppstd=23 --build=missing
cmake -S . -B build/dbg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake -DCMAKE_PREFIX_PATH=<core>/build/conan-dbg/stage
cmake --build build/dbg && ctest --test-dir build/dbg --output-on-failure
```

Expected: `blocks.selftests` PASS. The same number of tests as `pothos.blocks.selftests` in the core.

- [ ] **Step 5: Write the CI of blocks.** Use the same 4-OS matrix as the core. Steps:
  1. Check out `markvin/PothosCore` at ref `phase9-conan-deps` (one env variable `POTHOS_CORE_REF`).
  2. Run Conan install, build and `cmake --install` the core into `${{ runner.temp }}/pothos`.
  3. Run Conan install and build blocks with `CMAKE_PREFIX_PATH` set to that prefix.
  4. Run `ctest`.

- [ ] **Step 6: Commit** in PothosBlocks:
  `git commit -m "build: standalone build against an installed core, dependencies through find_package"`.

- [ ] **Step 7: Ask the user.** Ask to fork `pothosware/PothosBlocks` to `markvin/PothosBlocks`
  (`gh repo fork pothosware/PothosBlocks --clone=false`). Ask to push `pothos2` to it.
  The CI of blocks needs the core branch on GitHub. Thus ask to push
  `phase9-conan-deps` first, or in the same step.

---

### Task 5: Remove blocks from the core; CI with Conan and system packages; docs

**Files:**
- Delete: `blocks/`
- Modify: `CMakeLists.txt` (option `POTHOS_BUILD_BLOCKS`, `add_subdirectory(blocks)`,
  test `pothos.blocks.selftests`), `lib/Framework/TopologyToolkitBlocks.hpp:24,33-34`,
  `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp:71`, `include/Pothos/Framework/Topology.hpp:191-192`,
  `.github/workflows/ci.yml` (guard paths), `README.md`, `CLAUDE.md`, `Changelog.txt`

**Interfaces:**
- Consumes: Task 1 (`conanfile.txt`, the configure command lines).

- [ ] **Step 1: Restore the phase 7 text (test first).** In `TestTopologyToolkitBlocks.cpp:71`, expect
  `error.find("PothosBlocks")` again. Run the test and expect FAIL. Then restore the text of
  `af8361bc` in `TopologyToolkitBlocks.hpp` and `Topology.hpp`
  (`git show af8361bc:<file>`). Run the test again. Expected: PASS.

- [ ] **Step 2: Remove `blocks/` and the CMake integration.** Use `git rm -r blocks`. Remove
  `POTHOS_BUILD_BLOCKS`, `add_subdirectory(blocks)` and `pothos.blocks.selftests`.
  Then search: `grep -rn "POTHOS_BUILD_BLOCKS\|pothos_blocks_module\|blocks\.selftests" -- . ':!docs/superpowers'`.
  Expected: no result.

- [ ] **Step 3: Change the CI guard.** The guard paths become `CMakeLists.txt lib apps cmake`.

- [ ] **Step 4: Update the docs.**
  - `README.md`: dependency list (no "vendored") and build section with the two
    methods, using the commands of Task 1.
  - `README.md`: remove the license line about the vendored directories.
  - `README.md`: tell that blocks is in `markvin/PothosBlocks`.
  - `CLAUDE.md`: layout, build and conventions.
    - Remove `third_party/`, `blocks/` and the `pothos_blocks_module` paragraph.
    - Add `conanfile.txt` and `cmake/PothosDependencies.cmake`.
    - Change the SIMD line to `<Pothos/Util/XSIMDCopy.hpp>`.
    - Change `lib/Net` to "standalone Asio (find_package)".
  - `Changelog.txt`: one entry. Write all text in ASD-STE100.

- [ ] **Step 5: Run the complete verification in Conan mode and in system mode (if Task 1 Step 7 was done)**

`cmake --build build/conan-dbg && ctest --test-dir build/conan-dbg --output-on-failure`.
Expected: all PASS, and no `pothos.blocks.selftests`.
`grep -rniE "conan" CMakeLists.txt cmake lib apps`: no result.

- [ ] **Step 6: Commit**

```sh
git commit -am "core: blocks moves to markvin/PothosBlocks"
```

- [ ] **Step 7: Review, push, PR.**
  1. One fresh review of the whole branch, then the fixes.
  2. Ask the user, then push `phase9-conan-deps`.
  3. Watch CI of the core (5 jobs) and of blocks.
  4. Ask the user, then open the PR into `pothos2`.
  5. After the merge, change `POTHOS_CORE_REF` in the blocks CI to `pothos2`.
  6. Update the memory files and `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 9 entry).
