# Phase 6a: CMake hygiene (the core as a subproject) — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Another project can include the core with `add_subdirectory`/`FetchContent`. The core does not change the options, the cache, the global settings, the tests and the targets of the parent. The standalone build of the core works as before.

**Architecture:** The root `CMakeLists.txt` splits the behavior with `PROJECT_IS_TOP_LEVEL`. The options get the prefix `POTHOS_`, and the extra "component" options are removed. The target properties replace the global `CMAKE_*` settings, and `PROJECT_BINARY_DIR` replaces `CMAKE_BINARY_DIR`. A new test `pothos.subproject` configures a small parent project with `add_subdirectory(<core>)` and checks that the core did not damage it.

**Tech Stack:** CMake >= 3.25 (`PROJECT_IS_TOP_LEVEL` exists since 3.21), CTest.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` ("The CMake structure ... is at the discretion of the implementer"; anti-goal: the consumer must not need the internal dependencies). Roadmap: phase 6, the item "CMake hygiene" (decision of the user 2026-10-27; phase 6 is split into 6a and 6b, and the namespace `Pothos::` stays).

## Global Constraints

- The standalone build does not change:
  - the same targets;
  - the same install (layout, `PothosConfig.cmake`, `Pothos.pc`);
  - the same CTest tests;
  - `compile_commands.json` is created;
  - the rpath of the installed `libPothos` and `PothosUtil` is the same as before.
- Linux GCC/Clang, macOS and Windows MSVC: all four CI jobs are green.
- The compatibility of the names of the CMake options is not necessary (spec: "The CMake structure ... is at the discretion of the implementer"). `Changelog.txt` and the README record the renames.
- The check for each task: `cmake --build build && ctest --test-dir build --output-on-failure` -> all PASS.
- Push to `origin` only with the consent of the user.

## Decisions (for approval)

1. **Options.**
   - These options are removed: `ENABLE_LIBRARY`, `ENABLE_LIBRARY_LIB`, `ENABLE_LIBRARY_INCLUDE`, `ENABLE_LIBRARY_CMAKE`. The library is the project, and it makes no sense to disable it.
   - New options:
     - `POTHOS_BUILD_APPS` (PothosUtil; the default is ON, because the self-tests and the safe load of the modules need it);
     - `POTHOS_BUILD_DOCS` (the default is ON if Doxygen is found and the project is top level);
     - `POTHOS_BUILD_TESTS` (the registration of the CTest tests; the default is `${PROJECT_IS_TOP_LEVEL}`).
   - These options already have the prefix: `POTHOS_ENABLE_LIBNUMA`, `POTHOS_SELFTEST_TIMEOUT`, `POTHOS_ROOT`, `POTHOS_ROOT_ENV`. `APPDATA_ENV` is renamed to `POTHOS_APPDATA_ENV`.
2. **Only at the top level:**
   - `enable_testing()` by default (through `POTHOS_BUILD_TESTS`);
   - `feature_summary`;
   - `CMAKE_EXPORT_COMPILE_COMMANDS`;
   - the selection of `Release` if `CMAKE_BUILD_TYPE` is empty;
   - the target `uninstall`. In a subproject, `pothos_uninstall` appears in its place.
3. **Global settings -> target properties.**
   - `CMAKE_CXX_STANDARD`/`_REQUIRED`/`_EXTENSIONS` are removed. The standard already comes through `target_compile_features(... cxx_std_23)`. `CXX_EXTENSIONS OFF` is set on the core targets.
   - `option(CMAKE_SKIP_BUILD_RPATH...)`, `option(CMAKE_BUILD_WITH_INSTALL_RPATH...)`, `option(CMAKE_INSTALL_RPATH_USE_LINK_PATH...)`, `CMAKE_INSTALL_RPATH` and `CMAKE_MACOSX_RPATH` are removed. `INSTALL_RPATH` (`$ORIGIN/../lib...`, `@loader_path/../lib...`) is set on `Pothos` and `PothosUtil`.
3a. **`PothosConfig.cmake` in a build of the core in the tree** (`POTHOS_IN_TREE_SOURCE_DIR`) does not touch `CMAKE_BUILD_TYPE`. For the external consumers (`find_package`) the behavior is the same as before.
4. **`CMAKE_BINARY_DIR` -> `PROJECT_BINARY_DIR`** in all paths of the core: stage, test-home, consumer, test-env-guard.

## Review Focus

1. **The standalone build did not change:**
   - the list of the installed files is the same before and after (diff of `install_manifest.txt` of the old and the new build);
   - the CTest tests are the same;
   - the rpath of `PothosUtil` and `libPothos` is the same as before (`readelf -d`/`otool -l`).

   The check step is in Task 2.
2. **A parent with options and targets of the same names.** A parent with its own `option(ENABLE_LIBRARY OFF)`, `option(BUILD_DOCS OFF)` and a target `uninstall`, and also with an empty `CMAKE_BUILD_TYPE`:
   - configures;
   - its values do not change;
   - `Pothos::Pothos` is available.

   Test `pothos.subproject` (Task 1).
3. **The tests of the core do not go into the `ctest` of the parent without `POTHOS_BUILD_TESTS=ON`.** With `ON`, the stage directory is in the binary directory of the core, not of the parent. The same test (Task 1).
4. **The global variables of the parent do not change:** `CMAKE_CXX_STANDARD`, `CMAKE_INSTALL_RPATH`, `CMAKE_EXPORT_COMPILE_COMMANDS` and `CMAKE_BUILD_TYPE` are the same before and after `add_subdirectory`. The same test (Task 1).
5. **A consumer through `find_package`** (`pothos.consumer`) and the self-tests from stage work on all platforms - CI.

---

### Task 1: Test "the core as a subproject" and the CMake corrections

**Files:**
- Create: `tests/subproject/CMakeLists.txt` (the parent project), `cmake/PothosSubprojectTest.cmake` (cmake -P: it configures the parent two times, without and with `-DPOTHOS_BUILD_TESTS=ON`. It checks the `message` output and `ctest -N`)
- Modify: `CMakeLists.txt`, `lib/CMakeLists.txt`, `apps/CMakeLists.txt`, `include/CMakeLists.txt`, `cmake/Modules/CMakeLists.txt`, `docs/CMakeLists.txt`, `cmake/Modules/PothosConfig.cmake`

**Interfaces:**
- `tests/subproject/CMakeLists.txt`: `project(Parent)`.
  - Before the core is included: `option(ENABLE_LIBRARY "" OFF)`, `option(BUILD_DOCS "" OFF)`, `add_custom_target(uninstall)`. It also saves `CMAKE_CXX_STANDARD`, `CMAKE_INSTALL_RPATH`, `CMAKE_EXPORT_COMPILE_COMMANDS` and `CMAKE_BUILD_TYPE`.
  - Then `add_subdirectory(${POTHOS_SOURCE} pothos)`.
  - After it, the project prints `PARENT_CHECK name=value` for each of these values, and also `HAS_TARGET=$<TARGET_EXISTS:Pothos::Pothos>` (through `if(TARGET)`).
- `pothos.subproject` (a CTest test, only at the top level with `POTHOS_BUILD_TESTS`), the configurations are in `${PROJECT_BINARY_DIR}/subproject[-tests]`.
  - Expected: the values of the parent did not change, and `Pothos::Pothos` exists.
  - Without `POTHOS_BUILD_TESTS`, `ctest -N` in the directory of the parent shows 0 tests.
  - With it, the tests are `pothos.*`, and the stage path in their commands is under `pothos/`.

- [ ] **Step 1: Write `tests/subproject/CMakeLists.txt`, `cmake/PothosSubprojectTest.cmake` and the registration of `pothos.subproject`.**
- [ ] **Step 2: Run** `ctest -R pothos.subproject` -> FAIL (the option `ENABLE_LIBRARY=OFF` of the parent disables the core / `uninstall` is in conflict / the tests of the core are in the parent).
- [ ] **Step 3: Implement** decisions 1-4.
- [ ] **Step 4: Run** the complete `ctest` -> PASS (5 tests).
- [ ] **Step 5: Commit** `build: the core works as a CMake subproject (POTHOS_ options, top-level only globals)`

---

### Task 2: The standalone build did not change; documentation

**Files:**
- Modify: `README.md` (the build options, the use as a subproject), `CLAUDE.md` (the options, the rule "no global `CMAKE_*`/common names"), `Changelog.txt` (pothos2: the renamed/removed options)

- [ ] **Step 1: Compare the install:** make a clean build of `pothos2` (without this branch) and of this branch in `/tmp`-like directories in the scratchpad -> the `install_manifest.txt` files are the same. `readelf -d build-*/stage/bin/PothosUtil | grep -E 'RUNPATH|RPATH'` and the same for `libPothos.so` give the same result. `ctest -N` gives the same list, except for the new `pothos.subproject`. Correct the differences or record the decision in the ledger.
- [ ] **Step 2: Update the documentation.**
- [ ] **Step 3: Clean GCC and Clang builds** -> the complete `ctest` PASS. Delete the directories.
- [ ] **Step 4: Commit** `docs: CMake options and use as a subproject`

---

### Task 3: End of the phase

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (6a is done; the next one is 6b), the ledger `docs/superpowers/ledgers/2026-10-27-phase6a-cmake-subproject.md`

- [ ] **Step 1: Update the roadmap and the ledger. Commit** `docs: phase 6a complete — the core as a CMake subproject`
- [ ] **Step 2: Do a fresh review of the complete branch** (the focus is Review Focus). Make the corrections in separate commits.
- [ ] **Step 3: With the consent of the user: push the branch `phase6-api-cmake`, make a PR to `pothos2`, and check that CI passes on all four jobs.**

## Out of scope

- The public API (phase 6b): `SpinLock`/`SpinLockRW` -> the standard primitives, `Templates.hpp` -> `std::index_sequence`, `throw()` -> `noexcept`, `typedef` -> `using`, `std::span` next to the existing buffer methods. The signatures that the blocks use do not break (decision of the user: "moderate").
- `PothosConfig.cmake` for external consumers (`find_package`) still sets `CMAKE_BUILD_TYPE=Release` by default. This is the behavior of the toolkits, phase 7.
