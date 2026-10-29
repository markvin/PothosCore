# Phase 1: build foundation — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The PothosCore core configures with modern CMake (also 4.x) and builds in C++23 on Linux, macOS and Windows. CTest runs the self-tests, and they pass in GitHub Actions. Poco and muparserx stay for a short time, but only as private dependencies.

**Architecture:** The root `CMakeLists.txt` builds only the core (the toolkits and spuce are removed). Poco and muparserx are in-tree submodules. The build adds them with `add_subdirectory` and `CMAKE_POLICY_VERSION_MINIMUM=3.5`, and links them `PRIVATE`. The build vendors `json.hpp` in `third_party/`. The self-tests need an installed layout (`Pothos::init()` checks `bin/PothosUtil`, the library and `include/`). Therefore CTest first runs `cmake --install` into `<build>/stage`. Then CTest runs `stage/bin/PothosUtil --self-tests` with isolated user directories. The package is exported as `Pothos::Pothos` with no trace of Poco.

**Tech Stack:** CMake ≥ 3.25 (tested on 4.4.4), Ninja, GCC 13+/Clang 17+/AppleClang (Xcode 15+)/MSVC 17.8+, nlohmann/json 3.11.3 (single header), libsimdpp (submodule), Poco 1.8.1 (temporary), muparserx (temporary), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`)

## Global Constraints

- Standard: C++23 (`cxx_std_23`, no compiler extensions).
- `cmake_minimum_required(VERSION 3.25)`. The project must configure with CMake 4.x and no flags from the user.
- Platforms: Linux, macOS (minimum 13.3, CI arm64), Windows (MSVC).
- Minimum compilers: GCC 13+, Clang 17+, Xcode 15+, Visual Studio 2022 17.8+.
- The build does not use the network. The build needs no system libraries except the standard libraries of the OS.
- No type or header of an internal dependency (Poco, muparserx, json, libsimdpp) can enter the `INTERFACE_*` properties of the exported target or `PothosConfig.cmake`.
- The modules (toolkits) are not built and are not changed.
- You can disable a self-test on a specific platform when it fails because of the CI environment (not because of the port). Add a comment that gives the reason.
- Push to `origin` (the fork `markvin/PothosCore`) only after the user gives explicit consent.

## Review Focus

1. **A consumer of the installed package without Poco:** An external project runs `find_package(Pothos CONFIG REQUIRED)` and links `Pothos::Pothos`. It must build and must not see the headers of Poco, json or muparserx. The `pothos.consumer` test covers this (Task 4).
2. **A build without the network:** There is no `file(DOWNLOAD)`, `FetchContent` or `ExternalProject` anywhere. A CI grep step covers this (Task 6).
3. **The debug postfix of the library** (`libPothosd.so`): `Pothos::init()` looks for the library by `$<TARGET_FILE_NAME:Pothos>`. This must work in Debug and in Release. The CI matrix Debug+Release covers this (Task 6).
4. **The self-tests do not touch the home directory of the user:** `XDG_DATA_HOME`, `XDG_CONFIG_HOME` and `APPDATA` point into the build directory. The run script covers this (Task 2).
5. **A non-standard `CMAKE_INSTALL_LIBDIR`** (`lib64`): `Paths.cpp` builds the paths relative to the root. A staged install with `-DCMAKE_INSTALL_LIBDIR=lib64` must pass the self-tests. One Linux CI configuration covers this (Task 6).

---

### Task 1: Configuration with CMake 4 and a core build in C++23

**Files:**
- Modify: `CMakeLists.txt` (the whole file: version, standard, the blocks for Poco/spuce/muparserx/json/toolkits/uninstall)
- Modify: `lib/CMakeLists.txt:1-60` (search for dependencies), `:242-254` (linking), `:259-269` (flags)
- Modify: `apps/CMakeLists.txt`
- Modify: `cmake/Modules/PothosConfig.cmake:62-75` (the in-tree branch: `IN_TREE_LIBRARIES`)
- Create: `third_party/nlohmann/json.hpp` (nlohmann/json v3.11.3, `single_include/nlohmann/json.hpp`, MIT — the license is inside the header)
- Create: `third_party/CMakeLists.txt`
- Modify: `lib/Framework/TopologyMakeJSON.cpp`, `lib/Framework/TopologyDumpJSON.cpp`, `lib/Framework/WorkerActor.cpp`, `lib/Framework/TopologyStatsJSON.cpp`, `lib/Framework/ThreadPool.cpp`, `lib/Util/Builtin/*.cpp`, `apps/*.cpp` — all files that include `<json.hpp>` (find them with: `grep -rln 'json.hpp' lib apps`)

**Interfaces:**
- Produces: the CMake target `pothos_json` (INTERFACE, not exported, include path `third_party/`). The include in the code is `#include <nlohmann/json.hpp>`.
- Produces: Pothos links the Poco targets `PocoFoundation PocoUtil PocoXML PocoNet` and `muparserx` as `PRIVATE`. `PothosUtil` links `PRIVATE Pothos PocoFoundation PocoUtil PocoNet pothos_json`.
- Produces: the option `POTHOS_ENABLE_LIBNUMA` (default `OFF`). It enables the current `find_library(numa)` branch in `lib/CMakeLists.txt`.

- [ ] **Step 1: Record the current failure**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
Expected: FAIL — a `cmake_minimum_required` compatibility error (CMake 4 does not support < 3.5) or `INTERFACE_INCLUDE_DIRECTORIES ... prefixed in the source directory` for `poco/*/include`.

- [ ] **Step 2: Rewrite the root `CMakeLists.txt`**

- Read the version from `Changelog.txt` **before** `project()`. Use `project(Pothos VERSION ${POTHOS_LIBVER} LANGUAGES C CXX)`.
- Set `set(CMAKE_CXX_STANDARD 23)`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` (global — Poco also builds in C++23, an experiment proved this).
- Poco: always in-tree (delete the branch for an external Poco and `SetupPoco`). Inside `block()`, set `CMAKE_POLICY_VERSION_MINIMUM 3.5`. Set the current group `ENABLE_* OFF` from `POCO_MIN_BUILD`, and also `ENABLE_JSON OFF`, `ENABLE_PAGECOMPILER OFF`, `ENABLE_PAGECOMPILER_FILE2PAGE OFF`, `ENABLE_REDIS OFF`. Then run `add_subdirectory(poco)`. Remove the global `include_directories(${Poco_INCLUDE_DIRS})`.
- muparserx: inside `block()` with `CMAKE_POLICY_VERSION_MINIMUM 3.5`, `BUILD_SHARED_LIBS OFF`, `CMAKE_POSITION_INDEPENDENT_CODE ON`, run `add_subdirectory(muparserx EXCLUDE_FROM_ALL)`. The library is static and is not installed. The muparserx example is not built.
- Delete the blocks for spuce, the json download (with `install(FILES json.hpp)`), `ENABLE_TOOLKITS` and the foreach over the modules.
- Add `add_subdirectory(third_party)` before `lib`.
- uninstall: keep only the branch of the own `uninstall` target (do not replace the Poco script).

- [ ] **Step 3: Update `lib/CMakeLists.txt` and `apps/CMakeLists.txt`**

- Delete the sections "locate Poco", "muparserx library", "json.hpp header" and the condition `Poco_FOUND;muparserx_FOUND;JSON_HPP_INCLUDE_DIR` in `cmake_dependent_option(ENABLE_LIBRARY_LIB ...)`.
- Use `target_link_libraries(Pothos PRIVATE PocoFoundation PocoUtil PocoXML PocoNet muparserx pothos_json)`. The muparserx include path is `PRIVATE ${PROJECT_SOURCE_DIR}/muparserx/parser`. Remove `PUBLIC ${Poco_INCLUDE_DIRS}`.
- Add `target_compile_features(Pothos PUBLIC cxx_std_23)`.
- libnuma goes under `if(POTHOS_ENABLE_LIBNUMA)`. Otherwise keep the current choice of `NumaInfoWindows/OSX/Other`.
- `apps/CMakeLists.txt`: link as in Interfaces above. Remove `${JSON_HPP_INCLUDE_DIR}`.
- `PothosConfig.cmake` (in-tree branch): remove the `MUPARSERX_IN_TREE` part from `IN_TREE_LIBRARIES` (muparserx is static). Keep the Poco part (`PothosUtil.sh/.bat` need its DLL/so files). The list is `PocoFoundation PocoUtil PocoXML PocoNet`.

- [ ] **Step 4: Move the code to `<nlohmann/json.hpp>`**

Replace the include in all files from the list. If the compiler shows differences between the nlohmann 2.1.1 API and the 3.11.3 API, fix them (do not change the behavior of the JSON output).

- [ ] **Step 5: Build**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
Expected: the configuration has no errors (policy warnings from the submodules are allowed), the build completes, and there is no `FAILED`.

- [ ] **Step 6: Check the executable file**

Run: `./build/apps/PothosUtil --success-code 200; echo $?`
Expected: `200`

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt lib/CMakeLists.txt apps/CMakeLists.txt cmake/Modules/PothosConfig.cmake third_party lib apps
git commit -m "build: configure with modern CMake, C++23, private in-tree Poco/muparserx, vendored json"
```

---

### Task 2: Self-tests through CTest (staged install)

**Files:**
- Create: `cmake/PothosRunSelfTests.cmake` (a script for `cmake -P`)
- Modify: `CMakeLists.txt` (the test section at the end)

**Interfaces:**
- Consumes: the install rules from Task 1.
- Produces: the CTest test `pothos.stage` (`FIXTURES_SETUP pothos_stage`): `${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${CMAKE_BINARY_DIR}/stage --config $<CONFIG>`.
- Produces: the CTest test `pothos.selftests` (`FIXTURES_REQUIRED pothos_stage`): `${CMAKE_COMMAND} -DPREFIX=${CMAKE_BINARY_DIR}/stage -DHOME_DIR=${CMAKE_BINARY_DIR}/test-home -DBINDIR=${CMAKE_INSTALL_BINDIR} -DLIBDIR=${CMAKE_INSTALL_LIBDIR} -P cmake/PothosRunSelfTests.cmake`. It accepts an optional `-DTEST_PATH=<path>` that gives `--self-test1 <path>` in place of `--self-tests`.
- Produces: the variable `POTHOS_SELFTEST_TIMEOUT` (default 900 s) that sets the property `TIMEOUT` of the test.

- [ ] **Step 1: Make sure that no tests exist**

Run: `ctest --test-dir build -N`
Expected: `Total Tests: 0`

- [ ] **Step 2: Write `cmake/PothosRunSelfTests.cmake`**

The script does these actions. It clears and creates `HOME_DIR/data` and `HOME_DIR/config`. It sets `ENV{POTHOS_ROOT}=PREFIX`, `ENV{XDG_DATA_HOME}`, `ENV{XDG_CONFIG_HOME}` and `ENV{APPDATA}=HOME_DIR/data`. On Windows it adds `PREFIX/BINDIR` to the start of `PATH`. On Linux it adds `PREFIX/LIBDIR` to `LD_LIBRARY_PATH`. On macOS it adds it to `DYLD_LIBRARY_PATH`. It runs `PREFIX/BINDIR/PothosUtil[.exe]` with `execute_process(... RESULT_VARIABLE rc)` and sends the output to the console. It runs `message(FATAL_ERROR ...)` when `rc` is not 0.

- [ ] **Step 3: Register the tests in `CMakeLists.txt`**

`enable_testing()` is already there. Add `pothos.stage` and `pothos.selftests` as in Interfaces.

- [ ] **Step 4: Run**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `pothos.stage` PASS. `pothos.selftests` FAIL. The output has `Failed 1 out of 88 tests` and `FAIL: /framework/tests/test_mmap_shared_buffer` (a defect from before the modernization, fixed in Task 3). The user data and config directories of Pothos get no files newer than the run.

- [ ] **Step 5: Commit**

```bash
git add cmake/PothosRunSelfTests.cmake CMakeLists.txt
git commit -m "test: run plugin self-tests through CTest against a staged install"
```

---

### Task 3: Fix `test_mmap_shared_buffer`

**Files:**
- Investigate: `lib/Framework/Builtin/TestSharedBuffer.cpp:60-100`, `lib/Framework/MemoryMappedBufferContainer.cpp`, `lib/Framework/SharedBufferUnix.cpp`, `lib/Framework/SharedBufferWindows.cpp`
- Modify: the file that contains the cause

**Interfaces:**
- Consumes: `ctest -R pothos.selftests` and `-DTEST_PATH` from Task 2.

Use superpowers:systematic-debugging. The symptom is `TestSharedBuffer.cpp:86`, `assert equal 4096 == 0`, `statement "(input.length) == (tempFile.getSize())"`. After a write through the mmap buffer, the file has size 0. The test already exists and fails, so it is the "failing test". Fix the code. Do not weaken the test, unless you prove that the test checks a wrong expectation.

- [ ] **Step 1: Reproduce the single test**

Run: `cmake -DPREFIX=build/stage -DHOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
Expected: FAIL with `assert equal 4096 == 0`

- [ ] **Step 2: Find the root cause and fix it**

- [ ] **Step 3: Verify**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `pothos.selftests` PASS, and the output has `All 88 tests passed!`.

- [ ] **Step 4: Commit**

```bash
git commit -am "fix: <root cause> in memory-mapped shared buffer"
```

---

### Task 4: Export of the `Pothos::Pothos` package without Poco

**Files:**
- Modify: `lib/CMakeLists.txt` (the install/export section), `cmake/Modules/CMakeLists.txt`, `cmake/Modules/PothosConfig.cmake:150-184` (the installed branch), `cmake/Modules/PothosUtil.cmake:102`, `lib/Pothos.in.pc`
- Delete: `cmake/Modules/FindPoco.cmake`, `cmake/Modules/SetupPoco.cmake`, `cmake/Modules/FindPkgMacros.cmake`
- Create: `tests/consumer/CMakeLists.txt`, `tests/consumer/main.cpp`, `cmake/PothosConsumerTest.cmake`
- Modify: `CMakeLists.txt` (the test `pothos.consumer`)

**Interfaces:**
- Produces: `add_library(Pothos::Pothos ALIAS Pothos)` in the tree. `install(EXPORT PothosExport NAMESPACE Pothos:: FILE PothosTargets.cmake DESTINATION ${POTHOS_CMAKE_DIRECTORY})`. The installed `PothosConfig.cmake` runs `include(${CMAKE_CURRENT_LIST_DIR}/PothosTargets.cmake)`. It does **not** include `SetupPoco` and does not call `enable_language(C)`.
- Produces: `POTHOS_MODULE_UTIL` links `Pothos::Pothos`. The old variable `Pothos_LIBRARIES` = `Pothos::Pothos`.
- Produces: the CTest test `pothos.consumer` (`FIXTURES_REQUIRED pothos_stage`): `cmake -DPREFIX=... -DSRC=${PROJECT_SOURCE_DIR}/tests/consumer -DWORK=${CMAKE_BINARY_DIR}/consumer -DGENERATOR=${CMAKE_GENERATOR} -DCONFIG=$<CONFIG> -P cmake/PothosConsumerTest.cmake`. It configures, builds and runs the consumer.

- [ ] **Step 1: Write the consumer**

`tests/consumer/CMakeLists.txt`: `cmake_minimum_required(VERSION 3.25)`, `project(PothosConsumer CXX)`, `find_package(Pothos CONFIG REQUIRED)`, `add_executable(consumer main.cpp)`, `target_link_libraries(consumer PRIVATE Pothos::Pothos)`. Then the leak check:

```cmake
foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LINK_LIBRARIES INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(val Pothos::Pothos ${prop})
    if(val MATCHES "Poco|muparserx|nlohmann|simdpp")
        message(FATAL_ERROR "Pothos::Pothos leaks a private dependency via ${prop}: ${val}")
    endif()
endforeach()
```

`tests/consumer/main.cpp`: creates `Pothos::Object(42)`. It checks that `obj.convert<int>() == 42` and that `Pothos::System::getApiVersion()` is not empty. It returns 0 on success.

- [ ] **Step 2: Write `cmake/PothosConsumerTest.cmake` and register `pothos.consumer`**

The script runs `execute_process` for configure (`-G GENERATOR`, `-DCMAKE_PREFIX_PATH=PREFIX`, `-DCMAKE_BUILD_TYPE=CONFIG`) and for build. Then it runs `consumer` with the same library path environment as in `PothosRunSelfTests.cmake` (move the common function to `cmake/PothosTestEnv.cmake`). It gives `FATAL_ERROR` on any step with a non-zero code.

- [ ] **Step 3: Run — it must fail**

Run: `cmake --build build && ctest --test-dir build -R pothos.consumer --output-on-failure`
Expected: FAIL — `Pothos::Pothos` is not found (an export without a namespace), or `SetupPoco`/Poco is required at `find_package`.

- [ ] **Step 4: Rebuild the export as in Interfaces**

Delete the three Poco modules and their `install(FILES ...)`. In `Pothos.in.pc`, remove Poco/muparserx from `Requires`/`Libs.private` (the static muparserx is inside libPothos).

- [ ] **Step 5: Verify**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: all three tests PASS.

- [ ] **Step 6: Commit**

```bash
git add -A cmake tests lib/CMakeLists.txt lib/Pothos.in.pc CMakeLists.txt
git commit -m "build: export Pothos::Pothos without private dependencies, add consumer test"
```

---

### Task 5: libsimdpp and flag hygiene

**Files:**
- Modify: the submodule `libsimdpp` (update it to the current `master` of upstream `https://github.com/p12tic/libsimdpp`; on 2026-10-26 this is `55860b50`, `v2.0-rc1-574`)
- Modify: `lib/CMakeLists.txt:259-283` (flags), `:432-445` (SIMD)

**Interfaces:**
- Consumes: `simdpp_get_runnable_archs`, `simdpp_multiarch` from `libsimdpp/cmake/SimdppMultiarch.cmake`.

- [ ] **Step 1: Record the current warnings**

Run: `cmake --build build --clean-first 2>&1 | grep -c 'libsimdpp/.*warning'`
Expected: a number > 0 (about 2100, mostly `-Wtemplate-id-cdtor`).

- [ ] **Step 2: Update the submodule, add the headers as SYSTEM**

`target_include_directories(Pothos SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/libsimdpp)`. If the API changed, adapt the calls of `simdpp_multiarch`.

- [ ] **Step 3: Make the flags that the build forces on consumers PRIVATE**

`-Wall -Wextra -Wnon-virtual-dtor` and the MSVC flag `/MP` become `PRIVATE`. `NOMINMAX`, `/wd4251 /wd4275 /wd4503` and `-flat_namespace` (Apple) stay `PUBLIC` (the consumers of the headers and the modules need them). Delete the FreeBSD branch with `-stdlib=libc++` only if it causes a problem. Otherwise keep it.

- [ ] **Step 4: Verify**

Run: `cmake --build build --clean-first 2>&1 | grep -c 'libsimdpp/.*warning'; ctest --test-dir build --output-on-failure`
Expected: `0`; all tests PASS (the SIMD tests `/util/tests/test_simd_dispatcher_utils_dump_header` and the buffer conversions are part of the self-tests).

- [ ] **Step 5: Commit**

```bash
git add libsimdpp lib/CMakeLists.txt
git commit -m "build: update libsimdpp, include it as system headers, keep warning flags private"
```

---

### Task 6: CI on GitHub Actions, removal of the old CI, documentation

**Files:**
- Create: `.github/workflows/ci.yml`
- Delete: `.travis.yml`, `appveyor.yml`
- Modify: `CLAUDE.md` (the Build section), `README.md` (the build section, if it exists)

**Interfaces:**
- Consumes: the tests `pothos.stage`, `pothos.selftests`, `pothos.consumer`.

The matrix follows. Check the runner names at the time of the implementation. If a listed runner is removed, take the current `*-latest`.

| job | runner | compiler | configuration | extra flags |
|---|---|---|---|---|
| linux-gcc | ubuntu-24.04 | GCC 14 | Debug | — |
| linux-clang | ubuntu-24.04 | Clang 18 | Release | `-DCMAKE_INSTALL_LIBDIR=lib64` |
| macos | macos-latest (arm64) | AppleClang (Xcode ≥ 15) | Debug | `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3` |
| windows | windows-latest | MSVC (VS 2022) through `ilammy/msvc-dev-cmd` | Release | — |

- [ ] **Step 1: Write the workflow**

Triggers: `push`, `pull_request`. Steps: checkout without submodules → `git submodule update --init poco muparserx libsimdpp` (without `--depth`: the pinned commits are not at the tip) → the check step "no network": `! grep -rnE 'file\s*\(\s*DOWNLOAD|FetchContent|ExternalProject' CMakeLists.txt lib apps cmake third_party` → configure `-G Ninja` → build → `ctest --output-on-failure`.

The rule (my decision): If a job fails **inside the code of Poco** (not Pothos) on a specific platform, you can mark this job `continue-on-error: true` for a short time. Add a comment that names the roadmap phase that removes this part of Poco. This does not hide a failure of the Pothos code.

- [ ] **Step 2: Check the syntax of the workflow locally**

Run: `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml'))" && echo ok`
Expected: `ok`

- [ ] **Step 3: Delete `.travis.yml` and `appveyor.yml`. Update `CLAUDE.md` (Build: the configure/build/ctest commands from this plan, the list of the submodules that the core needs, a note about the staged install and `POTHOS_ROOT`)**

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml CLAUDE.md README.md
git rm .travis.yml appveyor.yml
git commit -m "ci: GitHub Actions for Linux/macOS/Windows, drop Travis and AppVeyor"
```

- [ ] **Step 5: Ask the user for permission to push to `origin`. After the push, wait for the result of the workflow (`gh run watch`)**

Expected: all four jobs are green (or they have `continue-on-error` strictly by the rule above — list such jobs in the report).
