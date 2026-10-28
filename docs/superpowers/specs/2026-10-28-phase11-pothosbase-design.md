# Phase 11: PothosBase, the aggregate build of Pothos

Date: 2026-10-28. Status: design. Work happens in a new repository
`markvin/PothosBase`. The core changes only in its docs. The toolkits
get small CMake changes.

## Intent

Pothos is now several repositories: the core and one repository for each
toolkit. Each toolkit builds against an installed core. This is correct
for users, but it is slow for development:

- A change of the core API has no fast check against the real toolkits.
  `pothos.api_compat` checks only old-style code.
- To debug from a block into the core, the developer installs the core,
  then installs each toolkit, then runs `PothosUtil` from that prefix.
- The comms self-tests need PothosBlocks in the core prefix. The CI of
  comms must build three repositories in a chain.
- There is no record of which commits of the repositories work together.

PothosBase is one repository with one top-level `CMakeLists.txt`. It
contains the core and the toolkits as git submodules and builds them in
one CMake tree with `add_subdirectory`. It gives a consistent build, one
`compile_commands.json`, one install prefix and one self-test run.

## Decisions of the user

1. Make a separate repository with the name PothosBase. It aggregates the
   base of Pothos.
2. A top-level `CMakeLists.txt` in PothosBase builds the submodules. The
   target list is: core, blocks, comms, python, soapy, widgets, flow,
   plotters.
3. plotters gets its own small copy of the window code (it uses spuce
   `design_window`). The spuce replacement of comms stays internal to
   comms. (Decision for the plotters port, recorded in the roadmap.)
4. SoapySDR is not on ConanCenter. CI builds SoapySDR from the upstream
   source `pothosware/SoapySDR` (a fixed tag or commit) in a separate step
   before the toolkit, and caches it. The other dependencies come from
   Conan. The job `linux-system` uses the Arch package `extra/soapysdr`.
   (Decision for the soapy port, recorded in the roadmap.)
5. PothosBase can build existing local clones in place of its submodules,
   with one source directory option for each repository. The developer
   edits in the usual clones (`/home/mark/Projects/Pothos*`) and checks
   them together. CI always builds the submodules.

## Current state (spike, 2026-10-28)

A throwaway spike in the scratchpad had a 5-line `CMakeLists.txt`:
`project(PothosBase C CXX)` and `add_subdirectory` of the local core,
blocks and comms. No change was made in the three repositories.

- Configure and build (Debug, GCC, Arch system packages): no errors, no
  compiler warnings, 2.5 minutes on the development machine.
- `cmake --install` into one prefix, then `PothosUtil --self-tests` with
  `cmake/PothosRunSelfTests.cmake` of the core: 304 tests passed in
  1.5 minutes (core, blocks and comms; comms without `filter` and
  `window`, from the work tree of phase 10).
- Why it works: the core as a subproject writes a `PothosConfig.cmake`
  into `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`. Thus `find_package(Pothos)` in
  a toolkit finds the core in the tree. This in-tree configuration sets
  `POTHOS_ROOT` (the install prefix) and adds the CMake modules of the
  core to `CMAKE_MODULE_PATH` (`PothosConfigSIMD`, `PothosUtil`,
  `POTHOS_MODULE_UTIL`). There were no target name conflicts.
- The tests of the core and of the toolkits are off in the subproject
  build. Their options default to `PROJECT_IS_TOP_LEVEL`
  (`POTHOS_BUILD_TESTS`, `BLOCKS_BUILD_TESTS`, `COMMS_BUILD_TESTS`).
- One defect: CMake warns about policy `CMP0144` at
  `find_package(Pothos)` in blocks and comms. The core sets the variable
  `POTHOS_ROOT`, and this name agrees with the `<PackageName>_ROOT`
  convention of `find_package`. The toolkits need
  `cmake_minimum_required(VERSION 3.25)` policies only, and CMP0144 is
  from CMake 3.27.
- The core sets `CMAKE_EXPORT_COMPILE_COMMANDS` only when it is the top
  level. PothosBase must set it.

State of the toolkits:

| Toolkit | Ported to pothos2 | Notes for its port |
|---|---|---|
| blocks | yes (phase 9, `markvin/PothosBlocks@pothos2`) | — |
| comms | phase 10 in progress (fork `markvin/PothosComms` made in its Task 7) | — |
| widgets | no | Qt5/Qt6 (already `QT NAMES Qt6 Qt5`), no Poco, 14 sources |
| soapy | no | SoapySDR (decision 4), 5 files with Poco |
| python | no | `FindPythonLibs`/`FindPythonInterp` (removed in CMake 4), 7 files with Poco |
| plotters | no | Qt, Qwt (Conan 6.3.0, Arch 6.3.0; upstream has a Qwt submodule), spuce (decision 3) |
| flow | no | Qt5/Qt6 GUI application, 31 files with Poco |

## Goals

- The repository `markvin/PothosBase` (branch `main`) contains the core
  and the ported toolkits as submodules, on their `pothos2` branches.
- One configure and one build give all modules of these repositories,
  with no warnings. `-DPOTHOS_BASE_WARNINGS_AS_ERRORS=ON` turns warnings
  into errors in all of them.
- `build/compile_commands.json` contains the sources of all submodules.
- `ctest` installs everything into one stage prefix and runs all
  self-tests (core and toolkits) from it.
- A developer can run and debug `PothosUtil` and the toolkit modules
  from the stage prefix with one documented command.
- The submodule commits in `main` are a set that builds and passes the
  tests. CI checks this for each change of a submodule pointer.
- The dependencies of all submodules come from the system or from one
  `conanfile.txt` of PothosBase.

## Non-goals

- Do not port the toolkits that are not ported (widgets, soapy, python,
  plotters, flow). Each port is its own phase. PothosBase adds a toolkit
  after its port.
- PothosBase does not replace the standalone build of a toolkit against
  an installed core. That build, and the CI of each repository, stay the
  primary check.
- Do not make packages or installers (Conan package, Debian, Windows
  installer) of PothosBase.
- Do not move code between repositories. Do not change the public API
  of the core.
- No bot that moves the submodule pointers automatically (a later step,
  if necessary).

## Anti-goals

These results are failures, also when the build is correct:

- A toolkit builds in PothosBase but not standalone against an installed
  core. Example: it uses a target of the core tree, `POTHOS_IN_TREE_*`, or
  a dependency that only PothosBase finds.
- PothosBase contains its own code for the toolkits (patches, copies of
  sources, overrides of their CMake). Fixes go into the repositories of the
  submodules.
- The configure step or the build step downloads files (FetchContent,
  ExternalProject with a download, `file(DOWNLOAD)`). Submodules and
  `conan install` are separate steps before CMake.
- A CMake file refers to Conan.
- A submodule pointer in `main` points to a commit that is not on the
  `pothos2` branch of that repository.
- The tests in PothosBase pass because a module of an earlier install is
  on the machine (a module outside the stage prefix is loaded).

## Repository layout

```
PothosBase/
  CMakeLists.txt          aggregate build
  conanfile.txt           union of the dependencies of the submodules
  cmake/                  helpers of PothosBase only (test runner, env)
  core/                   submodule markvin/PothosCore, branch pothos2
  blocks/                 submodule markvin/PothosBlocks, branch pothos2
  comms/                  submodule markvin/PothosComms, branch pothos2
  README.md               clone, build, test, debug, update the pointers
  Changelog.txt
  .github/workflows/ci.yml
```

- `.gitmodules` gives `branch = pothos2` for each submodule. Then
  `git submodule update --remote <name>` moves a pointer to the newest
  commit of that branch.
- Use HTTPS URLs in `.gitmodules` (CI and other users need no SSH key).
- A toolkit that is not ported is not a submodule yet (non-goals).

## Build

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosBase C CXX)`.
  C++23 comes from the subprojects (target properties). PothosBase does
  not set `CMAKE_CXX_STANDARD`.
- `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` at the top level. Copy or link
  the file to `build/compile_commands.json` only if the build directory is
  not `build/` (decision of the builder).
- Options (prefix `POTHOS_BASE_`):
  - `POTHOS_BASE_WITH_<TOOLKIT>` for each toolkit, default ON. When the
    option is ON and its source directory (`POTHOS_BASE_<NAME>_DIR`) has
    no `CMakeLists.txt`, stop with a clear message. For the submodule
    directory, the message is "run git submodule update --init <name>".
    For a different directory, the message gives the path. A missing submodule never turns a toolkit off without a
    message (decision of the designer: a forgotten `--init` must not give
    a green build with fewer modules).
  - When the tests are on, `POTHOS_BASE_WITH_COMMS=ON` with
    `POTHOS_BASE_WITH_BLOCKS=OFF` stops with a clear message: the comms
    tests need blocks (decision of the designer).
  - `POTHOS_BASE_<NAME>_DIR` for each repository (`CORE`, `BLOCKS`,
    `COMMS`, ...): the source directory (cache `PATH`). The default is the
    submodule directory `${PROJECT_SOURCE_DIR}/<name>`. A developer gives
    a local clone, for example
    `-DPOTHOS_BASE_CORE_DIR=/home/mark/Projects/PothosCore` (decision 5).
    `add_subdirectory(<dir> <name>)` always gives the binary directory,
    because the source can be outside the PothosBase tree.
  - When a `_DIR` option is not the submodule directory, configure prints
    one status line for each such repository (path and `git describe` of
    that clone, if git is available), and one warning that this build is
    not the fixed set of the submodule pointers. The configure step does
    not stop.
  - `POTHOS_BASE_WARNINGS_AS_ERRORS` (default OFF). It sets
    `POTHOS_WARNINGS_AS_ERRORS`, `BLOCKS_WARNINGS_AS_ERRORS` and
    `COMMS_WARNINGS_AS_ERRORS` before the `add_subdirectory` calls.
  - `POTHOS_BASE_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`).
- Order: `add_subdirectory(core)` first, then the toolkits. A toolkit may
  need another toolkit only at test time (comms needs blocks), not at
  build time.
- The core keeps its own tests off (subproject default). Its CTest tests
  `pothos.subproject`, `pothos.consumer` and `pothos.api_compat` check the
  standalone use of the core. They stay in the CI of the core.
- `conanfile.txt`: the union of the `[requires]` of the submodules, with
  the same versions (now `nlohmann_json/3.12.0`, `asio/1.38.2`,
  `xsimd/14.2.0`). CI fails when a submodule asks for a different version
  (a small check script; decision of the builder).

### Changes in the toolkits (in their repositories)

- blocks and comms: remove the `CMP0144` warning. Set the policy to NEW
  (`if(POLICY CMP0144)`, the policy comes with CMake 3.27) and call
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with a
  comment that tells why. `find_package` must not search the `POTHOS_ROOT`
  variable or the `POTHOS_ROOT` environment variable: users and the test
  environment set `POTHOS_ROOT` for the runtime, so a standalone configure
  could find a different core than `CMAKE_PREFIX_PATH` gives. (Execution
  ruling of 2026-10-28: the first design said the OLD behavior, but CMake
  4.4 gives a "deprecated" warning for a policy set to OLD.) The builder
  checks that the standalone build and the PothosBase build do not change
  and give no warning.
- A toolkit must not depend on its top-level position for its build.
  The spike found no such problem in blocks and comms. Each new toolkit
  port gets a check "builds in PothosBase" in its definition of done.
- These are commits on the `pothos2` branch of each toolkit, through a PR
  in its repository, before PothosBase uses them. The comms change is a
  separate small PR after phase 10 is merged. It does not go into the
  open phase 10 work (decision of the designer: phase 10 keeps its
  scope).

## Tests

1. CTest test `base.stage` installs the whole tree into
   `<build>/stage` (`cmake --install . --prefix <build>/stage`).
2. CTest test `base.selftests` runs `PothosUtil --self-tests` from the
   stage prefix with the isolated environment of the core
   (`POTHOS_ROOT`, `XDG_*`, `APPDATA`). It uses
   `core/cmake/PothosRunSelfTests.cmake` directly. PothosBase does not
   copy it.
3. The stage prefix must contain only modules of this build. The test
   runner must not set `POTHOS_PLUGIN_PATH`, and must stop when a module
   loads from outside the stage prefix (decision of the builder:
   `PothosUtil --module-info` or `--list-modules`, or another method that
   the core gives).
4. A module that does not load must fail the tests. `PothosUtil
   --self-tests` runs only the tests that it finds, and the plugin loader
   only logs a module that fails to load. Thus CTest test `base.modules`
   runs `PothosUtil --load-module=<file>` for each module file under the
   module directory of the stage prefix, and fails on the first error
   (decision of the designer). PothosBase does not read the internal test
   lists of the toolkits (`POTHOS_BLOCKS_TESTS`, `POTHOS_COMMS_TESTS`).
5. Each enabled toolkit must give at least one self-test under its
   plugin path (`/blocks/tests`, `/comms/tests`). If it gives none,
   `base.selftests` fails (method: decision of the builder).
6. A single self-test runs with the same script and `-DTEST_PATH=...`.
   The README shows this.
7. Timeout: a cache variable, default 1800 s.

## Debug

- The README gives one command that prints or sources the environment
  for the stage prefix (`POTHOS_ROOT`, the library path). Then
  `gdb --args <build>/stage/bin/PothosUtil --self-test1 /comms/tests/...`
  works. The builder selects the form: a small script in `cmake/` that
  PothosBase configures, or a CMake `-P` script.
- The build type comes from `CMAKE_BUILD_TYPE` (Debug for development).

## CI of PothosBase

- Triggers: push and PR on `main`, and a manual trigger.
- Job `linux` (Ubuntu, GCC, Conan, Debug,
  `POTHOS_BASE_WARNINGS_AS_ERRORS=ON`): checkout with
  `submodules: recursive`, `conan install`, configure, build, `ctest -V`.
- Job `linux-system` (Arch container with `--init`, system packages, no
  Conan): the same steps.
- macOS and Windows are not in the CI of PothosBase. The CI of each
  repository covers them.
- A guard step: no FetchContent, ExternalProject or `file(DOWNLOAD)` in
  the CMake files of PothosBase (as in the core).
- CI does not set the `POTHOS_BASE_<NAME>_DIR` options.
- A check step: every submodule pointer is on the `pothos2` branch of
  its repository (`git merge-base --is-ancestor <pointer>
  origin/pothos2` in each submodule).

## Local development with own clones

1. Configure once with the `_DIR` options of the clones that you change,
   for example the core and comms:
   `cmake -S . -B build -DPOTHOS_BASE_CORE_DIR=... -DPOTHOS_BASE_COMMS_DIR=...`.
2. Edit, build and test in this build directory.
3. Merge the changes in the repositories of the clones.
4. Move the submodule pointers (next section).

The README tells that a build with `_DIR` options is not the fixed set.
Only a build without these options proves the set of the pointers.

## Update of the submodule pointers

1. Merge the change into the `pothos2` branch of the submodule
   repository.
2. In PothosBase, run `git submodule update --remote <name>`.
3. Build and run the tests locally, or open a PR and let CI do it.
4. Merge the PR into `main` of PothosBase.

A change that needs the core and a toolkit together uses this order:
merge the core change, then the toolkit change, then one PothosBase PR
that moves both pointers.

## Order and dependencies of this phase

- Phase 10 (comms) is in progress. The fork `markvin/PothosComms` comes
  in its Task 7. Thus PothosBase starts with core and blocks, and adds
  comms when phase 10 is merged into `markvin/PothosComms@pothos2`.
  Alternatively, phase 11 starts after phase 10 (decision of the
  designer: start after phase 10, then all three are there at the start).
- The user approves these steps before they occur: the creation of the
  repository `markvin/PothosBase`, the first push, each PR and merge, and
  the PRs of the toolkit changes (CMP0144).

## Decisions of the designer (the user must check them)

- Only the ported toolkits are submodules now (core, blocks, comms). The
  other five come with their ports, not as disabled submodules.
- `add_subdirectory`, not ExternalProject. The standalone build of each
  toolkit, with its own CI, checks the installed-core path.
- Submodules on the `pothos2` branches, HTTPS URLs, branch `main` in
  PothosBase.
- PothosBase CI: Linux only (Conan and Arch). The full OS matrix stays
  in the repositories.
- Phase 11 starts after phase 10 is merged.
- A missing submodule with its option ON is a configure error, not a
  silent OFF. comms without blocks (tests on) is a configure error.
- CMP0144: NEW with `NO_PACKAGE_ROOT_PATH` in the toolkits (the
  standalone build does not change, no warning). The comms change is a
  separate PR after phase 10.
- `base.modules` loads every module file of the stage prefix, so a
  module that does not load fails the tests.
- SoapySDR becomes a submodule of PothosBase (upstream, fixed tag) when
  soapy is ported. This agrees with decision 4: it is the build from the
  upstream source, with the version fixed by the pointer.

## Recommended order of work

1. Toolkits: the CMP0144 change in blocks and comms (PRs in their forks).
2. Create `markvin/PothosBase` (after the approval of the user). Add the
   submodules, `CMakeLists.txt`, `conanfile.txt` and `.gitignore`.
3. The options, the warnings forward and `compile_commands.json`.
4. The tests (`base.stage`, `base.modules`, `base.selftests`, the check
   of the module paths).
5. The debug environment command and the README.
6. CI (2 jobs, the guard, the check of the pointers).
7. Core docs: roadmap (phase 11) and `CLAUDE.md` (PothosBase, its local
   clone).
