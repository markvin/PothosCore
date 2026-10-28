# Phase 11: PothosBase, the aggregate build of Pothos — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new repository `markvin/PothosBase` builds the core, blocks
and comms as submodules in one CMake tree, installs them into one stage
prefix and runs all their self-tests from it. Local clones can replace
the submodules.

**Architecture:** A top-level `CMakeLists.txt` calls `add_subdirectory`
for each repository. Its source directory comes from a cache option
(`POTHOS_BASE_<NAME>_DIR`, default the submodule). The core finds itself
for the toolkits through its `find_package` redirect (already works: the
spike). One `cmake -P` script of PothosBase runs the self-tests. It uses
the test environment of the core (`core/cmake/PothosTestEnv.cmake`) and
adds the checks that the core runner does not do.

**Tech Stack:** CMake ≥ 3.25 (tested on 4.4), git submodules, Conan 2
(`conanfile.txt`), GitHub Actions (Ubuntu with Conan, Arch container).

**Spec:** `docs/superpowers/specs/2026-10-28-phase11-pothosbase-design.md`

**Precondition:** phase 10 is merged into `markvin/PothosComms@pothos2`
(spec: "Phase 11 starts after phase 10 is merged"). Tasks 1–5 do not need
comms and can start earlier. Task 6 adds comms.

## Global Constraints

- Repository: `/home/mark/Projects/PothosBase` (new, local), remote
  `markvin/PothosBase`, branch `main`. Create the remote only after the
  user agrees (Task 7).
- Submodules: `core` → `https://github.com/markvin/PothosCore.git`,
  `blocks` → `https://github.com/markvin/PothosBlocks.git`, `comms` →
  `https://github.com/markvin/PothosComms.git`; `branch = pothos2` for
  each. Only these three (no disabled submodules for other toolkits).
- PothosBase contains no code for the toolkits: no patches, no copies of
  their sources or CMake. Fixes go into the toolkit repositories.
- Do not copy `core/cmake/PothosRunSelfTests.cmake` or
  `PothosTestEnv.cmake`. Include them from the core source directory.
- No FetchContent, ExternalProject or `file(DOWNLOAD)` in the CMake files
  of PothosBase, also not in comments. No word "conan" in the CMake files.
- Options have the prefix `POTHOS_BASE_`. Do not set global `CMAKE_*`
  variables except `CMAKE_EXPORT_COMPILE_COMMANDS`.
- `conanfile.txt` = the union of the submodule `[requires]`, same
  versions: `nlohmann_json/3.12.0`, `asio/1.38.2`, `xsimd/14.2.0`.
- English text is in ASD-STE100 (refer to `CLAUDE.md` of the core).
- Approval points (ask the user first): the PRs and pushes in the toolkit
  forks (Tasks 1 and 6), the creation of `markvin/PothosBase`, its first
  push, each PR and merge, the PR of the core docs (Task 7).

## Spec refinement

Spec "Tests" item 2 says that `base.selftests` uses
`core/cmake/PothosRunSelfTests.cmake` directly. Items 3 and 5 need checks
that this script does not do: `--self-tests` on a plugin path without tests
gives "All 0 tests passed!" and exit code 0 (seen in the spike), and the
script does not check the module paths. Thus PothosBase has its own
`cmake/PothosBaseSelfTests.cmake`. It includes the core
`PothosTestEnv.cmake` (no copy) and adds the checks. This keeps the intent
of item 2: no copy of the core runner.

## Facts from the spike (used by the steps)

- `PothosUtil --list-modules` prints one line for each loaded module:
  `<absolute path>   (<N> plugins) [<version>]`.
- `PothosUtil --load-module=<file>` prints "success!" and exits 0, or
  prints "Plugin Module Error: ..." and exits 64.
- `PothosUtil --self-tests` prints `Testing <path>... success!` for each
  test, and at the end `All <N> tests passed!`.
- `cmake_policy(SET CMP0144 ...)` is an error on CMake 3.25 and 3.26 (the
  policy comes with 3.27). Guard it with `if(POLICY CMP0144)`.
- The subprojects use `cmake_minimum_required(VERSION 3.25)`, thus CMP0077
  is NEW: a normal variable set before `add_subdirectory` gives the value
  of an `option()` in the subproject.

## Review Focus

1. A user clones without `--recursive` and configures. Expected: a
   configure error that names the submodule and the command
   `git submodule update --init <name>`, not a build without that toolkit.
   Test: Task 2, Step 1 (case `missing`).
2. A user has `POTHOS_PLUGIN_PATH` in the environment (for example from
   an old Pothos install). Expected: the tests load only modules from the stage
   prefix, and fail if any module comes from somewhere else. Test: Task 3,
   Step 1 (case `foreign_plugin_path`).
3. A toolkit module fails to load (for example a missing symbol).
   Expected: `base.modules` fails and names the module. Test: Task 3,
   Step 1 (case `broken_module`).
4. A developer gives `POTHOS_BASE_CORE_DIR` to a clone, and later removes
   the option (`-UPOTHOS_BASE_CORE_DIR`) to check the pointers. Expected:
   the build uses the submodule again, and the override warning goes away.
   Test: Task 2, Step 1 (case `override_reset`).
5. A developer configures with `POTHOS_BASE_<NAME>_DIR` pointing to a
   directory that does not exist or has no `CMakeLists.txt`. Expected: a
   configure error that gives the path. Test: Task 2, Step 1 (case
   `bad_dir`).

---

### Task 1: CMP0144 in blocks (OLD behavior)

**Files (repository `/home/mark/Projects/PothosBlocks`, new branch
`cmp0144-old` from `pothos2`):**
- Modify: `CMakeLists.txt` (before `find_package(Pothos CONFIG REQUIRED)`)

- [ ] **Step 1: Show the warning (RED)**

Configure the spike superbuild again
(`/tmp/claude-1000/-home-mark-Projects-PothosCore/403d5907-4b68-45cd-a0ec-c2013c1a73b6/scratchpad/base-spike`,
fresh build dir). Expected: `grep -c CMP0144 cfg.log` ≥ 1, the warning is
at `PothosBlocks/CMakeLists.txt:14`.

- [ ] **Step 2: Add the policy**

```cmake
#the core defines the variable POTHOS_ROOT (its install prefix). With
#CMP0144 NEW, find_package(Pothos) would also search POTHOS_ROOT and the
#POTHOS_ROOT environment variable (set for the runtime), and could find
#another core than CMAKE_PREFIX_PATH gives. Keep the old behavior.
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 OLD)
endif()
```

- [ ] **Step 3: Check (GREEN)**

1. The spike superbuild: no `CMP0144` in the configure log, no other new
   warning.
2. The standalone build of blocks against `$S/core-install` (refer to the
   plan of phase 10, "Local test setup") with `POTHOS_ROOT=/nonexistent`
   in the environment: configure finds the core from `CMAKE_PREFIX_PATH`
   (`Pothos_DIR` in the cache is under `$S/core-install`), no warning.

- [ ] **Step 4: Commit; ask the user; push and open the PR**

```sh
git commit -am "build: keep the old find_package behavior for POTHOS_ROOT (CMP0144)"
```

Ask the user. Then push `cmp0144-old` and open the PR into `pothos2` of
`markvin/PothosBlocks`. Tasks 2–5 do not wait for the merge: they use
`-DPOTHOS_BASE_BLOCKS_DIR=/home/mark/Projects/PothosBlocks` with this
branch checked out.

### Task 2: Repository, submodules, top-level CMake

**Files (repository `/home/mark/Projects/PothosBase`):**
- Create: `CMakeLists.txt`, `conanfile.txt`, `.gitignore`
  (`/build/`, `/CMakeUserPresets.json`), `.gitmodules` (by `git submodule add`)
- Create: `tests/configure_checks.cmake` (a `cmake -P` script)

**Interfaces:**
- Produces: options `POTHOS_BASE_CORE_DIR`, `POTHOS_BASE_BLOCKS_DIR`
  (`PATH`, default `${PROJECT_SOURCE_DIR}/<name>`),
  `POTHOS_BASE_WITH_BLOCKS` (default ON), `POTHOS_BASE_WARNINGS_AS_ERRORS`
  (default OFF), `POTHOS_BASE_BUILD_TESTS` (default
  `PROJECT_IS_TOP_LEVEL`); the list variable `POTHOS_BASE_TOOLKITS`
  (enabled toolkit names, for example `blocks`), used by Task 3; the
  function `pothos_base_add(<name>)`.
- The core has no `WITH` option: PothosBase always builds it.

- [ ] **Step 1: Write the configure checks (RED)**

`tests/configure_checks.cmake` takes `-DSRC=<PothosBase source>
-DWORK=<scratch dir>` and passes through `-DTOOLCHAIN=` and
`-DPREFIX_PATH=` (as `core/cmake/PothosSubprojectTest.cmake` does). It
runs `cmake -S ${SRC} -B ${WORK}/<case>` for each case and checks the
result code and the output:

| Case | Options | Expected |
|---|---|---|
| `default` | none | success; no `CMP0144`; no "not the fixed set" warning |
| `missing` | a copy of the top-level files of `SRC` (`CMakeLists.txt`, `conanfile.txt`, `cmake/`) in `${WORK}/src-missing`, with empty `core/` and `blocks/` dirs (as after a clone without `--recursive`); configure that copy | failure; output contains `git submodule update --init core` |
| `bad_dir` | `-DPOTHOS_BASE_CORE_DIR=${WORK}/nonexistent` | failure; output contains the path |
| `override` | `-DPOTHOS_BASE_BLOCKS_DIR=/home/mark/Projects/PothosBlocks` (from `-DBLOCKS_CLONE=`, the case is skipped when it is not given) | success; output contains the path and "not the fixed set of the submodule pointers" |
| `override_reset` | the `override` build dir again with `-UPOTHOS_BASE_BLOCKS_DIR` | success; no "not the fixed set" warning |
| `without_blocks` | `-DPOTHOS_BASE_WITH_BLOCKS=OFF` | success |

The script prints one line per case and stops with `FATAL_ERROR` at the
first wrong result. Run it now: Expected: FAIL (no `CMakeLists.txt` yet).

- [ ] **Step 2: Make the repository and the submodules**

```sh
mkdir /home/mark/Projects/PothosBase && cd /home/mark/Projects/PothosBase && git init -b main
git submodule add -b pothos2 https://github.com/markvin/PothosCore.git core
git submodule add -b pothos2 https://github.com/markvin/PothosBlocks.git blocks
```

- [ ] **Step 3: Write `CMakeLists.txt`**

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosBase C CXX)`,
  `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`.
- `pothos_base_add(<name>)`: reads `POTHOS_BASE_<NAME>_DIR`; if
  `<dir>/CMakeLists.txt` does not exist, `FATAL_ERROR` with
  "git submodule update --init <name>" for the default dir, or with the
  path for another dir; if the dir is not the default, a `STATUS` line
  with the path and `git -C <dir> describe --always --dirty` (when git is
  found), and the name goes into a list for one `WARNING` after all
  repositories: "This build uses local clones (<list>): it is not the
  fixed set of the submodule pointers."; then
  `add_subdirectory(<dir> <name>)`.
- Before the first `add_subdirectory`: `set(POTHOS_WARNINGS_AS_ERRORS
  ${POTHOS_BASE_WARNINGS_AS_ERRORS})`, the same for
  `BLOCKS_WARNINGS_AS_ERRORS` and `COMMS_WARNINGS_AS_ERRORS`.
- Order: core, then each enabled toolkit; append each enabled toolkit to
  `POTHOS_BASE_TOOLKITS`.
- The default of a `_DIR` option is used when the cache value is empty,
  so `-U` gives the submodule again (Review Focus 4).

`conanfile.txt`: the three requires of Global Constraints, `CMakeDeps`,
`CMakeToolchain`.

- [ ] **Step 4: Run the configure checks (GREEN)**

```sh
cmake -DSRC=$PWD -DWORK=$S/pbase-checks -DBLOCKS_CLONE=/home/mark/Projects/PothosBlocks -P tests/configure_checks.cmake
```

Expected: all 6 cases pass (`default` passes only with the Task 1 branch
in the blocks clone or after its merge; until the merge, run `default`
with `-DPOTHOS_BASE_BLOCKS_DIR` and record this in the ledger).

- [ ] **Step 5: Build**

`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
-DPOTHOS_BASE_WARNINGS_AS_ERRORS=ON
-DPOTHOS_BASE_BLOCKS_DIR=/home/mark/Projects/PothosBlocks` and
`cmake --build build`. Expected: no errors, no warnings;
`build/compile_commands.json` has entries under `core/` and under the
blocks dir.

- [ ] **Step 6: Commit**

```sh
git add -A && git commit -m "build: aggregate build of the core and blocks"
```

### Task 3: Tests from one stage prefix

**Files:**
- Create: `cmake/PothosBaseSelfTests.cmake`, `cmake/PothosBaseModulePaths.cmake`
- Modify: `CMakeLists.txt` (tests section), `tests/configure_checks.cmake`
  is not changed
- Create: `tests/runner_checks.cmake` (a `cmake -P` script)

**Interfaces:**
- Consumes: `POTHOS_BASE_TOOLKITS`, `POTHOS_BASE_CORE_DIR` (Task 2).
- Produces: CTest tests `base.stage` (fixture setup `base_stage`),
  `base.modules`, `base.selftests`; cache `POTHOS_BASE_SELFTEST_TIMEOUT`
  (default 1800); the script interface:
  `cmake -DPREFIX= -DTEST_HOME_DIR= -DBINDIR= -DLIBDIR= -DCORE_DIR=
  -DTOOLKITS=<;-list> -DMODE=modules|selftests [-DTEST_PATH=] -P cmake/PothosBaseSelfTests.cmake`.

- [ ] **Step 1: Write the runner checks (RED)**

`tests/runner_checks.cmake` takes `-DSTAGE=<a staged prefix of a
PothosBase build>` and the other script arguments. It copies the stage
prefix to scratch dirs and runs the script:

| Case | Setup | Expected |
|---|---|---|
| `good` | the copy, `MODE=modules`, then `MODE=selftests` | both succeed |
| `broken_module` | write a file `libBroken.so` (some bytes) into the module dir of one toolkit | `MODE=modules` fails and names `libBroken` |
| `foreign_plugin_path` | `POTHOS_PLUGIN_PATH` in the environment = a dir with a copy of one blocks module | `MODE=selftests` still loads only modules of the prefix (the script unsets the variable), and succeeds |
| `foreign_module` | call `pothos_base_check_module_paths()` (Step 2) directly with a fake `--list-modules` output that has one line outside the prefix | it stops with `FATAL_ERROR` that names the foreign path; with only paths inside the prefix it passes |
| `no_toolkit_tests` | `TOOLKITS=blocks;nosuch` | `MODE=selftests` fails: "no self-tests under /nosuch/tests" |

Run it: Expected: FAIL (no script yet).

- [ ] **Step 2: Write `cmake/PothosBaseSelfTests.cmake`**

1. `include(${CORE_DIR}/cmake/PothosTestEnv.cmake)` and
   `pothos_test_env(...)` as the core runner does; `unset(ENV{POTHOS_PLUGIN_PATH})`.
2. `MODE=modules`: glob the module files under
   `${PREFIX}/${LIBDIR}/Pothos/modules*/` (`.so`, `.dylib`, `.dll`); for
   each toolkit in `TOOLKITS` at least one file in its subdir; run
   `PothosUtil --load-module=<file>` for each; stop at the first non-zero
   exit with the file name and the output.
3. `MODE=selftests`: run `PothosUtil --list-modules` and give the output
   to `pothos_base_check_module_paths(<output> <prefix>)`, which is in its
   own file `cmake/PothosBaseModulePaths.cmake` (so that
   `tests/runner_checks.cmake` can call it with a fake output). Every path
   must begin with `${PREFIX}/`, else stop with the foreign path. Note:
   the loader searches only `<POTHOS_ROOT>/<libdir>/Pothos/modules<ABI>`,
   `/usr/local/...` when the root is `/usr`, and `POTHOS_PLUGIN_PATH`
   (`lib/System/Paths.in.cpp`). Thus the unset of `POTHOS_PLUGIN_PATH`
   is the main protection, and this check catches a future change of the
   loader. Then run
   `PothosUtil --self-tests` (or `--self-test1 ${TEST_PATH}`), keep the
   output, fail on a non-zero exit; without `TEST_PATH`, check that for
   each toolkit the output has at least one line `Testing /<toolkit>/tests/`.

- [ ] **Step 3: Add the CTest tests**

When `POTHOS_BASE_BUILD_TESTS`: `enable_testing()`; `base.stage` runs
`cmake --install ${PROJECT_BINARY_DIR} --prefix ${PROJECT_BINARY_DIR}/stage
--config $<CONFIG>`; `base.modules` and `base.selftests` run the script
with `FIXTURES_REQUIRED base_stage`; `base.selftests` gets the timeout.

- [ ] **Step 4: Run (GREEN)**

`ctest --test-dir build --output-on-failure`. Expected: 3 tests pass; the
self-test count is the count of the core and blocks tests. Then
`cmake -DSTAGE=build/stage ... -P tests/runner_checks.cmake`. Expected: all
5 cases pass.

- [ ] **Step 5: Commit**

```sh
git add -A && git commit -m "test: stage install, module load check and self-tests"
```

### Task 4: Debug environment, README, Changelog

**Files:**
- Create: `cmake/PothosBaseEnv.cmake` (a `cmake -P` script),
  `README.md`, `Changelog.txt`
- Modify: `CMakeLists.txt` (configure a small `build/pothos-env.sh` and
  `build/pothos-env.ps1` from the stage prefix, decision of the builder)

- [ ] **Step 1: Write the env output check (RED)**

Add a case `env` to `tests/runner_checks.cmake`: source
`build/pothos-env.sh` in `bash -c` and run
`PothosUtil --self-test1 /blocks/tests/test_copier_domain_crossing` (a
blocks test from the phase 9 list). Expected now: FAIL (no file).

- [ ] **Step 2: Write the env files**

They set `POTHOS_ROOT` to the stage prefix, add `<stage>/bin` to `PATH`,
`<stage>/<libdir>` to `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH`
(macOS), and unset `POTHOS_PLUGIN_PATH`. They do not set the isolated
`XDG_*` dirs (a developer uses the real ones).

- [ ] **Step 3: Run (GREEN)**

Expected: the `env` case passes.

- [ ] **Step 4: README and Changelog**

README sections (procedures in numbered steps): clone
(`--recurse-submodules`), dependencies (system packages or
`conan install`), build, test (`ctest`, one test with `-DTEST_PATH`),
debug (`source build/pothos-env.sh` then `gdb --args PothosUtil
--self-test1 ...`), local clones (`POTHOS_BASE_<NAME>_DIR`, the warning,
`-U` to go back), update of the submodule pointers (the 4 steps of the
spec), add a toolkit after its port.

- [ ] **Step 5: Commit**

```sh
git add -A && git commit -m "docs: build, test, debug and local clones"
```

### Task 5: CI

**Files:**
- Create: `.github/workflows/ci.yml`, `tests/check_conan_versions.cmake`

- [ ] **Step 1: Write the version check (RED)**

`tests/check_conan_versions.cmake` reads `conanfile.txt` of PothosBase
and of each submodule that has one, and fails when a package has two
versions. Test it once with a temporary edit of the PothosBase file
(`xsimd/14.3.0`). Expected: FAIL that names `xsimd`. Revert the edit.

- [ ] **Step 2: Write the CI**

- Triggers: `push`, `pull_request` on `main`, `workflow_dispatch`.
- Job `linux` (ubuntu-24.04, gcc-14, Debug): checkout with
  `submodules: recursive`; the guard step of the core CI for
  `CMakeLists.txt cmake tests`; the version check; the pointer check
  (`git -C <name> fetch origin pothos2` and
  `git -C <name> merge-base --is-ancestor HEAD FETCH_HEAD` for each
  submodule); Conan as in the core CI; configure with
  `-DPOTHOS_BASE_WARNINGS_AS_ERRORS=ON`; build; `ctest -V`; then
  `tests/configure_checks.cmake` (without `BLOCKS_CLONE`).
- Job `linux-system`: the Arch container of the core CI (`--init`,
  `pacman -S` of the toolchain, `asio nlohmann-json xsimd`), no Conan,
  same configure, build and `ctest`.
- No `POTHOS_BASE_<NAME>_DIR` anywhere in the CI.

- [ ] **Step 3: Check locally**

Run the version check and the pointer check commands locally. Expected:
both pass. (The workflow runs in Task 7.)

- [ ] **Step 4: Commit**

```sh
git add -A && git commit -m "ci: Linux with Conan and with Arch packages"
```

### Task 6: comms (after phase 10)

**Precondition:** phase 10 merged into `markvin/PothosComms@pothos2`.

**Files:**
- PothosComms (branch `cmp0144-old` from `pothos2`): `CMakeLists.txt`, the
  same change as Task 1
- PothosBase: `.gitmodules` (submodule `comms`), `CMakeLists.txt`
  (`POTHOS_BASE_COMMS_DIR`, `POTHOS_BASE_WITH_COMMS`, the error for comms
  without blocks), `tests/configure_checks.cmake` (case
  `comms_without_blocks`), README

- [ ] **Step 1: comms CMP0144**

Task 1 Steps 1–4 for comms. Ask the user before the push and the PR.

- [ ] **Step 2: Write the failing configure case (RED)**

Case `comms_without_blocks`: `-DPOTHOS_BASE_WITH_BLOCKS=OFF` (comms ON,
tests ON). Expected: failure, output "the comms tests need blocks". Run:
Expected now: the case FAILS (comms is not in the build yet).

- [ ] **Step 3: Add comms**

`git submodule add -b pothos2 https://github.com/markvin/PothosComms.git comms`;
`pothos_base_add(comms)` after blocks; the check: when
`POTHOS_BASE_BUILD_TESTS` and `POTHOS_BASE_WITH_COMMS` and not
`POTHOS_BASE_WITH_BLOCKS`, `FATAL_ERROR`.

- [ ] **Step 4: Run (GREEN)**

Configure checks: all cases pass. `ctest`: 3 tests pass, the self-test
output has `/comms/tests/` lines (the spike had 304 tests for core,
blocks and comms without `filter` and `window`; now more).

- [ ] **Step 5: Commit**

```sh
git add -A && git commit -m "build: add comms"
```

### Task 7: Publish, review, core docs

- [ ] **Step 1: Ask the user; create the repository and push**

```sh
gh repo create markvin/PothosBase --public --source . --remote origin --push
```

Expected: both CI jobs pass. Fix failures with follow-up pushes.

- [ ] **Step 2: Whole-branch review; fixes**

One fresh review of PothosBase `main` (all commits) and of the two
toolkit PRs. Fix the findings.

- [ ] **Step 3: Core docs (core branch with the phase 11 docs)**

Roadmap: phase 11 row and status. `CLAUDE.md`: PothosBase (repository,
local clone `/home/mark/Projects/PothosBase`, the `_DIR` options).
Commit `docs: phase 11 in the roadmap`. Ask the user, then push and open
the PR of the core docs.
