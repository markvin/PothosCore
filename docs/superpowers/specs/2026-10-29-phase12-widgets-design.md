# Phase 12: PothosWidgets on the pothos2 core

Date: 2026-10-29. Status: design. Work happens in a new fork
`markvin/PothosWidgets`, then in `markvin/PothosBase`. The core changes
only in its docs.

## Intent

PothosWidgets gives the graphical control widgets of the Pothos GUI
(sliders, entries, buttons, displays). PothosFlow shows them. The roadmap
ports the toolkits one at a time, in the order widgets → soapy → python
→ plotters → flow. Widgets is the first toolkit with Qt. Its port sets
the method for Qt in the build and in CI, which plotters and flow use
later.

When this phase is complete, widgets builds against an installed
pothos2 core with Qt 6, has no compiler warnings, has CI, and is a
submodule of PothosBase.

## Decisions of the user

1. Qt 6 only. Remove the Qt 5 support.
2. CI gets Qt in a separate step: the action `install-qt-action`
   (binary Qt from the Qt mirrors). The job `linux-system` uses the Arch
   package `qt6-base`. Qt is not a Conan dependency. (This is the same
   method as SoapySDR, decision 4 of phase 11.)
3. Add a smoke test of all widgets. It runs without a screen
   (`QT_QPA_PLATFORM=offscreen`).

## Current state (recon on upstream `33a3a75`)

- 14 `.cpp` files at the repository root, one module `ControlWidgets`
  (`DESTINATION widgets`), 11 blocks: `/widgets/chat_box`,
  `colored_box`, `drop_down`, `numeric_entry`, `odometer`,
  `planar_select`, `push_button`, `radio_group`, `slider`,
  `text_display`, `text_entry`.
- No Poco. The code uses only `Pothos/Framework.hpp`,
  `Pothos/Object/Containers.hpp`, `Pothos/Object/Serialize.hpp`,
  `Pothos/Plugin.hpp` and `Pothos/Testing.hpp`.
- 2 self-tests in `QStringSupport.cpp`: `/widgets/tests/test_qstring_convert`
  and `test_qstring_serialize`. No test makes a widget.
- CMake: `cmake_minimum_required(VERSION 3.1.0)` (CMake 4 refuses it),
  `find_package(QT NAMES Qt6 Qt5)`, components `Widgets` and `Concurrent`
  (the code does not use `Concurrent`), `ENABLE_WIDGETS` with
  `cmake_dependent_option` (a missing Qt turns the module off without an
  error), a define for Qt older than 5.11.
- Spike (2026-10-29, scratchpad, no changes to the code except
  `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`): the build against the stage of
  PothosBase with the Arch Qt 6.12 compiled 13 of 14 files. The pothos2
  API gave no errors. One error from Qt 6: `indicator.cpp` calls
  `findChildren<CustomDigit *>()` and `findChild<CustomDigit *>()`, and
  `CustomDigit` has no `Q_OBJECT`. Qt 6.12 refuses this with a static
  assertion. The same lines use the Qt macro `foreach`.
- `debian/` exists (packages for Pothos 0.7).

## Goals

- The fork `markvin/PothosWidgets`, branch `pothos2`, builds against an
  installed pothos2 core and Qt 6 (6.5 or newer) with CMake 3.25 or newer
  (also CMake 4).
- No compiler warnings with `-Wall -Wextra` (GCC and Clang).
  `WIDGETS_WARNINGS_AS_ERRORS=ON` turns them into errors.
- All 11 widgets have a smoke test. The self-tests pass on Linux, macOS
  and Windows in CI.
- PothosBase builds widgets as a submodule. Its tests load the widgets
  module and run the widgets self-tests from the stage prefix.
- The core docs record phase 12.

## Non-goals

- No check of the look of the widgets (layout, colors, fonts). PothosFlow
  is not ported yet.
- No new widgets and no changes of the block API of the widgets (block
  paths, calls, signals, `|PothosDoc` comments), except a fix of a bug.
- No packages (Debian, Conan package, installer).
- No change of the public API of the core.

## Anti-goals

These results are failures, also when the build is correct:

- The configure step continues without Qt and gives a build without the
  widgets module. A missing Qt 6 must stop the configure step with a
  clear message.
- A CMake file refers to Conan, or the configure or build step downloads
  files.
- The smoke test passes on a platform because it skips the widgets
  without a message.
- widgets builds in PothosBase but not standalone against an installed
  core (or the opposite).
- The installed module loads only from the build tree. It must load from
  the install prefix (the Qt libraries must be found at run time).

## Repository and history

1. Fork `pothosware/PothosWidgets` to `markvin/PothosWidgets`. Create the
   fork only after the user agrees at that step of the plan.
2. Make the branch `pothos2` from upstream `master` (`33a3a75`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosWidgets`.
4. The work goes into a work branch and a PR into `pothos2` of the fork
   (the same as blocks and comms). The user approves the fork, the first
   push and each merge.

## Build (the same method as PothosComms)

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosWidgets CXX)`,
  C++23 (`CXX_EXTENSIONS OFF`).
- `cmake_policy(SET CMP0144 NEW)` and
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with the
  comment of comms that tells why.
- `find_package(Qt6 6.5 CONFIG COMPONENTS Widgets)`. If Qt 6 is missing,
  stop with one clear message: install `qt6-base` (or the package of the
  system), or give `CMAKE_PREFIX_PATH` or `Qt6_DIR`. Remove `Concurrent`,
  the Qt 5 code and the define for Qt older than 5.11.
- Remove `ENABLE_WIDGETS` and `cmake_dependent_option`: the module
  always builds.
- `AUTOMOC` as a target property, not the global `CMAKE_AUTOMOC`.
- The default install prefix is the prefix of the core when the user
  gives no `CMAKE_INSTALL_PREFIX` (as in comms).
- Options: `WIDGETS_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `WIDGETS_WARNINGS_AS_ERRORS` (default OFF).
- Warnings: `-Wall -Wextra` for GCC and Clang, `-Werror` with
  `WIDGETS_WARNINGS_AS_ERRORS` (a function as `comms_warnings`). Qt
  headers come as system includes from the imported Qt targets. Fix the
  warnings in the widgets code; do not turn them off.
- The installed module must find the Qt libraries at run time. On Linux
  and macOS use an install RPATH to the Qt library directory when Qt is
  not in a system directory (for example
  `INSTALL_RPATH_USE_LINK_PATH`). On Windows the Qt `bin` directory is on
  `PATH` (CI and the user). Decision of the builder: the exact method.
- widgets needs no Conan packages. It has no `conanfile.txt`. Conan in CI
  gives only the dependencies of the core.
- Keep the flat layout of the sources at the repository root.
- Delete `debian/`. Update `README.md` (dependencies: the pothos2 core and
  Qt 6; build; run the tests) and `Changelog.txt` (a "Release 0.6.0
  (pending)" entry: the port to the pothos2 core, Qt 6 only).

## Qt 6 fixes

- `indicator.cpp`: replace `findChildren<CustomDigit *>()` and
  `findChild<CustomDigit *>()`. Either add `Q_OBJECT` to `CustomDigit`
  (it then needs a moc run, so the class goes into a header or the `.cpp`
  includes its `.moc` file), or keep the digits in a member list of
  `Indicator`. Decision of the builder; the behavior must stay the same.
- Replace the Qt macro `foreach` with a range `for` where it occurs.
- Fix other Qt 6 deprecation warnings when the build shows them.

## Tests

- The method of comms:
  1. CTest test `widgets.stage` installs widgets into `build/stage`.
  2. CTest test `widgets.selftests` runs each `/widgets/tests/...` test
     with the `PothosUtil` of the installed core. It uses a copy of
     `cmake/PothosRunSelfTests.cmake` and `cmake/PothosTestEnv.cmake` of
     comms, without the check for PothosBlocks (widgets needs no blocks).
- The 2 upstream tests stay.
- New smoke test (decision 3), in a new file, for example
  `TestWidgets.cpp`, as one or more `POTHOS_TEST_BLOCK("/widgets/tests", ...)`:
  - Make sure that a `QApplication` exists. If none exists, set
    `QT_QPA_PLATFORM=offscreen` when the variable is not set, and make one.
  - For each of the 11 blocks: make it with
    `Pothos::BlockRegistry::make`, get its widget with the `widget` call,
    call its setters with typical values, and check the values of its
    getters. Then delete the block.
  - Some setters change the widget later through a queued call (for
    example `DropDown`/`RadioGroup` `__setValue`, `TextEntry`
    `handleSetValue`). Process the Qt events before the test reads a
    getter.
  - A block that has no getter for a value is only made, set and deleted.
  - The test must not skip a widget without a message.
- `CTest` sets `QT_QPA_PLATFORM=offscreen` for `widgets.selftests` too,
  so that a run without a display does not depend on the test code.
- Risk: Qt can refuse a `QApplication` that is not in the main thread of
  the process, for example on macOS. The self-test runner of the core
  runs a test in the main thread of `PothosUtil`. If a platform still
  refuses it, stop and ask the user. Do not skip the test on that
  platform.

## CI of widgets

- The same structure as the CI of comms, without the PothosBlocks step:
  linux-gcc (Debug, `-Werror`), linux-clang (Release, `-Werror`), macos
  (Intel runner, `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC
  Release).
  1. Check out widgets and `markvin/PothosCore@pothos2`.
  2. Install the dependencies of the core with Conan.
  3. Install Qt 6 with `install-qt-action` (Qt 6.5.3, the minimum
     version, for all 4 jobs, with its cache). `linux-system` tests the
     newest Qt of Arch. Ubuntu 24.04 has Qt 6.4, which is
     older than the minimum 6.5, so the Linux jobs use the action too.
  4. Build and install the core.
  5. Build widgets and run `ctest -V`.
- Job `linux-system` in an `archlinux` container (`--init`): system
  packages with `qt6-base`, no Conan.
- Keep the guard against downloads in the CMake files.

## PothosBase

After `pothos2` of the fork has widgets:

- Add the submodule `widgets` (HTTPS URL, `branch = pothos2`), the
  options `POTHOS_BASE_WITH_WIDGETS` and `POTHOS_BASE_WIDGETS_DIR`, and
  `WIDGETS_WARNINGS_AS_ERRORS` from `POTHOS_BASE_WARNINGS_AS_ERRORS`.
- The checks `base.modules` and `base.selftests` include widgets
  (module directory `/widgets/`, tests `/widgets/tests/`).
- `base.selftests` sets `QT_QPA_PLATFORM=offscreen`.
- CI of PothosBase: a Qt step with `install-qt-action` in the job with
  Conan, `qt6-base` in `linux-system`. The `conanfile.txt` of PothosBase
  does not change.
- README of PothosBase: Qt 6 is a dependency.

## Changes in the core

- Roadmap: phase 12 row and status.
- `CLAUDE.md`: a short note about the widgets fork, its branch and its
  local clone.
- No code changes are expected. If widgets needs a change of the public
  API, stop and ask the user.

## Decisions of the designer (the user must check them)

- Fork `markvin/PothosWidgets`, branch `pothos2` from upstream `33a3a75`,
  local clone `../PothosWidgets`.
- The minimum Qt version is 6.5 (LTS).
- The fix of `indicator.cpp` and other Qt 6 fixes: only what the build
  with Qt 6 needs, or what removes a warning.
- CMake as in comms: CMake 3.25, `CMP0144` NEW, `NO_PACKAGE_ROOT_PATH`,
  `-Wall -Wextra`, the options `WIDGETS_*`, a clear error without Qt 6.
- No `conanfile.txt` in widgets: it has no Conan dependencies.
- CI jobs as in comms; all 4 jobs with Conan get Qt 6.5.3 (the minimum)
  from `install-qt-action` (Ubuntu 24.04 Qt 6.4 is too old). The Arch job
  tests the newest Qt.
- `POTHOS_BASE_WITH_WIDGETS` is ON by default, as the other toolkits. Thus
  Qt 6 becomes a dependency of PothosBase. A missing Qt 6 stops the
  configure step (`-DPOTHOS_BASE_WITH_WIDGETS=OFF` builds without it).
- If no `QApplication` exists, the smoke test makes one with the
  `offscreen` platform.
- `debian/` is deleted. Changelog entry "Release 0.6.0 (pending)".
- After the merge in the fork: widgets into PothosBase, then the core
  docs.

## Recommended order of work

1. Fork and branch. New top-level CMake, the test runner, and the
   `indicator.cpp` fix. The build is clean locally with Arch Qt 6.12,
   and the 2 upstream tests pass.
2. The smoke test of the 11 widgets, locally with `offscreen`.
3. README, Changelog, delete `debian/`.
4. CI (4 jobs with Conan and `install-qt-action`, and `linux-system`).
   PR into `pothos2` of the fork.
5. PothosBase: the widgets submodule, checks, CI.
6. Core docs: roadmap and `CLAUDE.md`.
