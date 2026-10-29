# Phase 12: PothosWidgets on the pothos2 core — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The fork `markvin/PothosWidgets` (branch `pothos2`) builds
against an installed pothos2 core with Qt 6, has no warnings, has a smoke
test of all 11 widgets and CI. Then PothosBase builds it as a submodule.

**Architecture:** The CMake of widgets follows PothosComms: CMake 3.25,
`find_package(Pothos ... NO_PACKAGE_ROOT_PATH)`, `find_package(Qt6 6.5)`,
a stage install and `--self-test1` runs of each test with the
`PothosUtil` of the core. The smoke test makes each widget through
`Pothos::BlockRegistry::make` in an `offscreen` `QApplication`. CI gets
the core dependencies from Conan and Qt from `install-qt-action`.

**Tech Stack:** C++23, CMake ≥ 3.25 (tested on 4.4), Qt 6 (≥ 6.5;
local Arch 6.12, CI 6.5.3), Conan 2 (core only), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-29-phase12-widgets-design.md`

## Global Constraints

- Repository: `/home/mark/Projects/PothosWidgets`, cloned from upstream
  `pothosware/PothosWidgets`. Branch `pothos2` from upstream `master`
  (`33a3a75`); work branch `phase12-widgets` from `pothos2`. The fork
  `markvin/PothosWidgets` is made in Task 4, after the user agrees.
- Qt 6 only, minimum 6.5: `find_package(Qt6 6.5 CONFIG COMPONENTS Widgets)`.
  No Qt 5 code, no `Concurrent`, no define for Qt < 5.11.
- A missing Qt 6 stops the configure step with a clear message. No
  `ENABLE_WIDGETS`, no `cmake_dependent_option`.
- `cmake_minimum_required(VERSION 3.25)`, `CMP0144` NEW,
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`. Target
  properties only (`AUTOMOC`, `CXX_EXTENSIONS OFF`); no global
  `CMAKE_*` except the default install prefix (the prefix of the core).
- Options: `WIDGETS_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `WIDGETS_WARNINGS_AS_ERRORS` (default OFF). `-Wall -Wextra` for GCC and
  Clang. Fix warnings, do not silence them.
- No `conanfile.txt` in widgets. No word "conan", no FetchContent,
  ExternalProject or `file(DOWNLOAD)` in its CMake files (also not in
  comments).
- No change of the block API (paths, calls, signals, `|PothosDoc`), except
  a bug fix recorded in the ledger. No change of the public API of the
  core: if one is necessary, stop and ask the user.
- English text in ASD-STE100 (refer to `CLAUDE.md` of the core). No
  "Generated with Claude Code" or `Co-Authored-By` lines in commits or PRs.
- Approval points (ask the user first): the fork, the first push, each PR
  and merge (widgets, PothosBase, core).
- Ledger: `.superpowers/sdd/2026-10-29-phase12-widgets/progress.md` in the
  core (not committed), as in phases 10 and 11.

## Facts from the recon (used by the steps)

- Spike: upstream builds against the PothosBase stage with Qt 6.12 except
  `indicator.cpp`: `findChildren<CustomDigit *>()` (lines 105, 164, 197,
  inside `foreach`) and `findChild<CustomDigit *>()` (line 131) fail with
  "No Q_OBJECT in the class passed to QObject::findChildren".
- The calls of each block (the smoke test uses them):

  | Block | Factory args | Setter → getter |
  |---|---|---|
  | `/widgets/chat_box` | — | `setUsername`, `setColor` (no getter) |
  | `/widgets/colored_box` | — | `setTitle`, `setFontSize`, `setText`, `setColor` (no getter) |
  | `/widgets/drop_down` | — | `setOptions(ObjectVector)`, `setValue(Object)` → `value()` (queued) |
  | `/widgets/numeric_entry` | — | `setMinimum`, `setMaximum`, `setValue(double)` → `value()` |
  | `/widgets/odometer` | — | `setValueMin`, `setValueMax`, `setValue(qint64)` → `value()` |
  | `/widgets/planar_select` | — | `setMinimum`, `setMaximum`, `setValue(std::vector<double>)` → `value()` |
  | `/widgets/push_button` | — | `setArgs(ObjectVector)` → `getArgs()` |
  | `/widgets/radio_group` | — | `setOptions(ObjectVector)`, `setValue(Object)` → `value()` (queued) |
  | `/widgets/slider` | `"Horizontal"` / `"Vertical"` | `setMinimum`, `setMaximum`, `setValue(double)` → `value()` |
  | `/widgets/text_display` | — | `setValue(Object)` (no getter) |
  | `/widgets/text_entry` | — | `setValue(QString)` → `value()` (queued) |

- Several setters use `QMetaObject::invokeMethod(..., Qt::QueuedConnection,
  Q_ARG(Pothos::Object, ...))`. A queued argument of a type that Qt does
  not know gives only a run-time warning ("Cannot queue arguments of
  type") and no call. The smoke test must detect this (the getter keeps
  the old value).
- `PothosUtil --self-tests` starts one child process for each test;
  `--self-test1` runs the test in the main thread of `PothosUtil`.
- The comms test runner: `PothosComms/cmake/PothosRunSelfTests.cmake`
  (with `TEST_PATHS`, `PLUGIN_PATH`) and `cmake/PothosTestEnv.cmake`.

## Review Focus

1. No display (no `DISPLAY`/`WAYLAND_DISPLAY`, CI and ssh): the tests
   still pass, because CTest and the test set `offscreen`. Test: Task 2
   Step 5.
2. Qt 6 is missing or older than 6.5: configure stops with the message,
   it does not give a build without the module. Test: Task 1 Step 6.
3. Qt is not in a system directory (`install-qt-action`, a user Qt in
   `~/Qt`): the installed module loads from the stage prefix. Test: the
   CI jobs with Conan run `widgets.selftests` from the stage (Task 3),
   and Task 1 Step 7 checks the install RPATH.
4. A queued setter with an argument type that Qt does not know: the
   getter keeps the old value. Test: the queued rows in Task 2 Step 1.
5. A host application already has a `QApplication` (PothosFlow later):
   the test uses it and does not make a second one. Test: Task 2 Step 1
   `ensureApplication()` checks `QApplication::instance()` first; a code
   review item, no separate test.

---

### Task 1: Build on CMake 3.25 and Qt 6, test runner

**Files:**
- Modify: `CMakeLists.txt` (rewrite)
- Create: `cmake/PothosRunSelfTests.cmake`, `cmake/PothosTestEnv.cmake`
  (copies from comms, without the PothosBlocks check)
- Modify: `indicator.cpp`, `indicator.h`

**Interfaces:**
- Produces: target `ControlWidgets` (module dir `widgets`);
  `widgets_warnings(<target>)`; global property `POTHOS_WIDGETS_TESTS`
  (list of `/widgets/tests/<name>`); CTest tests `widgets.stage`,
  `widgets.selftests` (environment `QT_QPA_PLATFORM=offscreen`).

- [ ] **Step 1: Clone and branch**

```sh
git clone https://github.com/pothosware/PothosWidgets.git /home/mark/Projects/PothosWidgets
cd /home/mark/Projects/PothosWidgets
git switch -c pothos2 33a3a75 && git switch -c phase12-widgets
```

- [ ] **Step 2: Rewrite `CMakeLists.txt`**

Structure as comms `CMakeLists.txt` lines 1–30 (header, options, CMP0144
comment, default install prefix), then `find_package(Qt6 6.5 CONFIG
COMPONENTS Widgets)` with a `FATAL_ERROR` when `NOT Qt6_FOUND`:
"PothosWidgets: Qt 6 (>= 6.5) with the Widgets component is missing.
Install the Qt 6 package of the system (for example qt6-base), or give
CMAKE_PREFIX_PATH or Qt6_DIR. Refer to README.md." Then
`POTHOS_MODULE_UTIL(TARGET ControlWidgets SOURCES ... LIBRARIES
Qt6::Widgets DESTINATION widgets ENABLE_DOCS)`, target properties
`AUTOMOC ON`, `CXX_EXTENSIONS OFF`, `widgets_warnings(ControlWidgets)`,
the tests list `test_qstring_convert test_qstring_serialize`, and the
test block of comms (lines 120–150) with `widgets.` names and no blocks
check. Add `ENVIRONMENT QT_QPA_PLATFORM=offscreen` to `widgets.selftests`.

- [ ] **Step 3: Install RPATH for a Qt outside the system directories**

Set `INSTALL_RPATH_USE_LINK_PATH ON` on `ControlWidgets` (the linker
directories outside the system directories, that is the Qt library
directory, go into the install RPATH). Keep the RPATH that
`POTHOS_MODULE_UTIL` gives.

- [ ] **Step 4: Build to see the Qt 6 failure (RED)**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/home/mark/Projects/PothosBase/build/stage -DWIDGETS_WARNINGS_AS_ERRORS=ON && cmake --build build`
Expected: FAIL in `indicator.cpp`, "No Q_OBJECT in the class passed to QObject::findChildren".

- [ ] **Step 5: Fix `indicator.cpp`, then build and test (GREEN)**

Keep the digits in a member `QList<CustomDigit *> _digits` of `Indicator`
(filled where the digits are made, cleared where they are deleted), and
use it in place of `findChildren`/`findChild`. Replace `foreach` with a
range `for`. Fix all warnings of `-Wall -Wextra -Werror`.
Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: build without warnings; `widgets.stage` and `widgets.selftests`
pass (2 tests).

- [ ] **Step 6: Check the missing-Qt error**

Run: `cmake -S . -B build-noqt -G Ninja -DCMAKE_PREFIX_PATH=/home/mark/Projects/PothosBase/build/stage -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON`
Expected: configure fails with "Qt 6 (>= 6.5) with the Widgets component is missing". Delete `build-noqt`.

- [ ] **Step 7: Check the install RPATH and CMake 3.25**

Run: `readelf -d build/stage/lib/Pothos/modules*/widgets/libControlWidgets.so | grep -E 'RUNPATH|RPATH'`
Expected: the RPATH of `POTHOS_MODULE_UTIL` (Arch Qt is in `/usr/lib`,
so no Qt directory here; CI checks the other case). If CMake 3.25 is
available (`pipx run cmake==3.25.*`), configure with it too: no error.

- [ ] **Step 8: Commit**

```sh
git add -A && git commit -m "build: CMake 3.25, Qt 6 only, stage and self-test runner"
```

### Task 2: Smoke test of all widgets

**Files:**
- Create: `TestWidgets.cpp` (add it to `SOURCES`, add the test names to
  the tests list)

**Interfaces:**
- Consumes: the block calls in the table of the facts section.
- Produces: tests `/widgets/tests/test_<block>` for each of the 11 blocks
  (for example `test_slider`, `test_drop_down`).

- [ ] **Step 1: Write the tests**

A file-local `static void ensureApplication()`: if
`QApplication::instance()` is null, set `QT_QPA_PLATFORM=offscreen`
when it is not set (`qputenv`), then make one `QApplication` with static
`argc`/`argv` that stays until the process ends. A file-local
`static void processEvents()` that calls
`QCoreApplication::processEvents()` until no posted events stay. Each
test calls `ensureApplication()`, makes the block with
`Pothos::BlockRegistry::make(path, args...)`, checks
`block.call<QWidget *>("widget") != nullptr`, calls the setters, calls
`processEvents()`, then checks the getters:

```cpp
// test_slider (both orientations): setMinimum(-10), setMaximum(10), setValue(5.0)
POTHOS_TEST_CLOSE(block.call<double>("value"), 5.0, 1e-9);
// test_numeric_entry: setMinimum(-10), setMaximum(10), setDecimals(2), setValue(7.25)
POTHOS_TEST_CLOSE(block.call<double>("value"), 7.25, 1e-9);
// test_odometer: setValueMin(0), setValueMax(99999), setValue(1234)
POTHOS_TEST_EQUAL(block.call<qint64>("value"), 1234);
// test_planar_select: setMinimum({-1,-1}), setMaximum({1,1}), setValue({0.25,-0.5})
POTHOS_TEST_EQUALV(block.call<std::vector<double>>("value"), (std::vector<double>{0.25, -0.5}));
// test_drop_down, test_radio_group: setOptions({"a","b","c"}), setValue("b")
POTHOS_TEST_EQUAL(block.call<Pothos::Object>("value").convert<std::string>(), "b");
// test_text_entry: setValue(QString("hello"))
POTHOS_TEST_TRUE(block.call<QString>("value") == "hello");
// test_push_button: setArgs({1, "x"})
POTHOS_TEST_EQUAL(block.call<Pothos::ObjectVector>("getArgs").size(), 2);
// test_chat_box, test_colored_box, test_text_display: setters only
```

Use the exact option and value types of each block (read its setter;
`DropDown`/`RadioGroup` options can be pairs of label and value). Each
setter value differs from the default value of the widget.

- [ ] **Step 2: Run the tests**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -V`
Expected: 13 widgets tests. If a queued getter fails ("Cannot queue
arguments of type" in the output), that is a bug of old code: register
the type (`Q_DECLARE_METATYPE` and `qRegisterMetaType` at module load)
and record it in the ledger. Do not change the expected value.

- [ ] **Step 3: Check that the queued checks can fail**

Temporarily remove the `processEvents()` call in `test_drop_down`.
Expected: `test_drop_down` fails. Put the call back.

- [ ] **Step 4: Run all tests again**

Run: `ctest --test-dir build --output-on-failure`
Expected: all pass, no Qt warnings in the output.

- [ ] **Step 5: Run without a display**

Run: `env -u DISPLAY -u WAYLAND_DISPLAY -u QT_QPA_PLATFORM ctest --test-dir build --output-on-failure`
Expected: all pass.

- [ ] **Step 6: Commit**

```sh
git add -A && git commit -m "test: smoke test of all widgets (offscreen)"
```

### Task 3: README, Changelog, debian, CI

**Files:**
- Delete: `debian/`
- Modify: `README.md`, `Changelog.txt`
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Docs**

`README.md`: dependencies (the pothos2 core, Qt 6 ≥ 6.5, CMake ≥ 3.25),
build against an installed core (`CMAKE_PREFIX_PATH`), run the tests
(`ctest`, `QT_QPA_PLATFORM=offscreen`). `Changelog.txt`: entry
"Release 0.6.0 (pending)": the port to the pothos2 core, Qt 6 only
(Qt ≥ 6.5), CMake ≥ 3.25, a smoke test of all widgets, plus each bug fix
from Task 2.

- [ ] **Step 2: CI**

Copy the comms `ci.yml`, then change: no PothosBlocks checkout and step;
Conan only for `pothos-core`; a step
`jurplel/install-qt-action@v4` with `version: '6.5.3'` and `cache: true`
after the Conan step in the 4 jobs; `-DWIDGETS_WARNINGS_AS_ERRORS=ON` on
Linux; `linux-system` adds `qt6-base` to the core packages
(`asio nlohmann-json xsimd`). Add one step
in the Linux GCC job after the test: print the install RPATH of the
staged module (`readelf -d`) and check that it contains the Qt library
directory of the action.

- [ ] **Step 3: Check the download guard and the CI file locally**

Run: `grep -rnE 'file\s*\(\s*DOWNLOAD|FetchContent|ExternalProject' --include=CMakeLists.txt --include='*.cmake' .`
Expected: no output. Check `ci.yml` with `actionlint` if available.

- [ ] **Step 4: Commit**

```sh
git add -A && git commit -m "ci: Linux, macOS, Windows with Qt 6.5.3 and Arch packages; docs"
```

### Task 4: Review, fork, PR in widgets

- [ ] **Step 1: Whole-branch review**

Dispatch one fresh reviewer on `pothos2..phase12-widgets` with the spec
and this plan. Fix the findings; record rulings in the ledger.

- [ ] **Step 2: Fork and push (ask the user first)**

```sh
gh repo fork pothosware/PothosWidgets --clone=false
git remote rename origin upstream
git remote add origin git@github.com:markvin/PothosWidgets.git
git push -u origin pothos2 phase12-widgets
```

- [ ] **Step 3: PR into `pothos2` of the fork, wait for CI**

Run: `gh pr create -R markvin/PothosWidgets -B pothos2 -H phase12-widgets ...`, then `gh pr checks <n> -R markvin/PothosWidgets --watch`
Expected: 5 jobs green. Fix CI failures with new commits on the branch.
If a platform (for example macOS) refuses the `QApplication` of the
smoke test, stop and ask the user; do not skip the test there.
Merge only on the user's word.

### Task 5: widgets in PothosBase

**Files (in `/home/mark/Projects/PothosBase`, branch `add-widgets` from `main`):**
- Modify: `.gitmodules` (submodule `widgets`), `CMakeLists.txt`
  (`pothos_base_all_toolkits blocks comms widgets`,
  `set(WIDGETS_WARNINGS_AS_ERRORS ...)`, `ENVIRONMENT
  QT_QPA_PLATFORM=offscreen` on `base.modules` and `base.selftests`),
  `tests/configure_checks.cmake`, `tests/runner_checks.cmake`,
  `.github/workflows/ci.yml` (Qt step 6.5.3, `qt6-base`,
  `TOOLKITS=blocks;comms;widgets`), `README.md`, `Changelog.txt`.

- [ ] **Step 1: Add the submodule**

```sh
git submodule add -b pothos2 https://github.com/markvin/PothosWidgets.git widgets
```

- [ ] **Step 2: Add a configure check `without_widgets`**

`-DPOTHOS_BASE_WITH_WIDGETS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON`
configures; with widgets ON and Qt disabled, configure fails with the
widgets Qt message. Run `tests/configure_checks.cmake` before the
`CMakeLists.txt` change: the new checks fail.

- [ ] **Step 3: Change `CMakeLists.txt`, CI, README, Changelog**

- [ ] **Step 4: Run all checks**

Run: configure, build with `-DPOTHOS_BASE_WARNINGS_AS_ERRORS=ON`, `ctest`,
`configure_checks`, `runner_checks` with `blocks;comms;widgets`.
Expected: 0 warnings, ctest 3/3, the self-tests count is 316 + 13, all
checks OK.

- [ ] **Step 5: Commit, push, PR (ask the user first), wait for CI**

Merge only on the user's word.

### Task 6: Core docs

**Files (core, branch `phase12-widgets`):**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`
  (row 12 and status), `CLAUDE.md` (a note about the widgets fork, branch
  `pothos2`, local clone `/home/mark/Projects/PothosWidgets`, Qt 6 from
  the system or `install-qt-action`, not Conan).

- [ ] **Step 1: Edit and commit**

```sh
git add CLAUDE.md docs/superpowers && git commit -m "docs: phase 12 in the roadmap and CLAUDE.md"
```

- [ ] **Step 2: Push and PR into `pothos2` (ask the user first)**

Run: `gh pr create -R markvin/PothosCore -B pothos2 -H phase12-widgets ...`
Expected: CI green. Merge only on the user's word.
