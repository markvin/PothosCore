# Phase 14: PothosPlotters on the pothos2 core — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The fork `markvin/PothosPlotters` (branch `pothos2`) builds
against an installed pothos2 core with Qt 6 and Qwt 6.3, has no warnings,
has self-tests (windows, smoke test of 9 blocks, data through 5 plotters)
and CI. Then PothosBase builds it as a submodule.

**Architecture:** First a core fix: `BufferChunk::operator ElementType`
accepts only pointer types, so Qt 6 moc can use `BufferChunk` in slots.
The CMake of plotters follows widgets (CMake 3.25, `NO_PACKAGE_ROOT_PATH`,
Qt 6.5, stage install, `--self-test1` runs). Qwt comes from a new
`FindQwt.cmake` (pkg-config, else `find_library`), which refuses a Qwt
for Qt 5. The window code is a copy of the comms port of spuce. CI builds
Qwt 6.3.0 with qmake from the SourceForge archive and caches it.

**Tech Stack:** C++23, CMake ≥ 3.25 (tested on 4.4), Qt 6 (≥ 6.5; local
Arch 6.12, CI 6.5.3 / 6.8.3 on Windows), Qwt 6.3.0, Conan 2 (core, blocks,
comms only), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-30-phase14-plotters-design.md`

## Global Constraints

- Repository: `../PothosPlotters` (upstream clone, `4a9e7d1`). Branch
  `pothos2` from upstream `master` (`4a9e7d1`); work branch
  `phase14-plotters` from `pothos2`. The fork `markvin/PothosPlotters` is
  made in Task 4, after the user agrees.
- Qt 6 only, minimum 6.5: `find_package(Qt6 6.5 CONFIG COMPONENTS Widgets)`.
- Qwt only from `find_package(Qwt 6.3.0)`: no Qwt sources, no submodule,
  no build of Qwt in the plotters CMake. A Qwt for Qt 5 is refused at the
  configure step.
- A missing Qt 6 or Qwt stops the configure step with a clear message. No
  `ENABLE_PLOTTERS*`, no `cmake_dependent_option`, no spuce.
- `cmake_minimum_required(VERSION 3.25)`, `CMP0144` NEW,
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`. Target
  properties only (`AUTOMOC`, `CXX_EXTENSIONS OFF`); no global `CMAKE_*`
  except the default install prefix (the prefix of the core).
- Options: `PLOTTERS_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `PLOTTERS_WARNINGS_AS_ERRORS` (default OFF). `-Wall -Wextra` for GCC and
  Clang. Fix warnings, do not silence them.
- No `conanfile.txt` in plotters. No word "conan", no FetchContent,
  ExternalProject or `file(DOWNLOAD)` in its CMake files (also not in
  comments).
- No change of the block API (paths, the old `/widgets/` paths, calls,
  signals, slots, `|PothosDoc`), except a bug fix recorded in the ledger.
  The only change of the public API of the core is the `BufferChunk` fix
  (Task 1).
- English text in ASD-STE100 (refer to `CLAUDE.md` of the core). No
  "Generated with Claude Code" or `Co-Authored-By` lines in commits or PRs.
  No home directory paths in committed text: write `../<Repository>`.
- Approval points (ask the user first): the fork, the first push, each PR
  and merge (core fix, plotters, PothosBase, core docs).
- Ledger: `.superpowers/sdd/2026-10-30-phase14-plotters/progress.md` in the
  core (not committed), as in the earlier phases.

## Facts from the recon (used by the steps)

- Spike (scratchpad, Arch Qt 6.12 + Qwt 6.3.0): Qwt 6.3 errors in
  `PlotterUtils/PothosPlotPicker.cpp` (`QwtText`, `QwtInterval`
  incomplete), `PlotterUtils/PothosPlotter.cpp` (`QPen`, `QStack`,
  `QPalette::Background`), `Constellation/ConstellationDisplay.cpp:127`,
  `Periodogram/PeriodogramDisplay.cpp:168,210`,
  `Spectrogram/ColorMapEntry.cpp:64`, `Spectrogram/SpectrogramDisplay.cpp:26,219,231-233`,
  `Spectrogram/SpectrogramRaster.hpp:44-54`,
  `WaveMonitor/WaveMonitorDisplay.cpp:148` (`setAxis` → `setAxes`).
  `ColorMapEntry.cpp:25`: `[=]` captures `this`.
- Each moc of a display class with a slot `const Pothos::BufferChunk &`
  fails in `BufferChunk.hpp` (`operator ElementType` with `int`, from
  `QtPrivate::QLessThanOperatorForType`).
- Blocks and calls (smoke test). The 5 plotters are topologies; calls that
  the topology does not know go to the display:

  | Block | Setter → getter |
  |---|---|
  | `/plotters/constellation` | `setTitle` → `title`, `setAutoScale` → `autoScale`, `setXRange`, `setYRange`, `setCurveStyle`, `setDisplayRate`, `setNumPoints` |
  | `/plotters/logic_analyzer` | `setNumInputs` → `numInputs`, `setSampleRate` → `sampleRate`, `setChannelLabel`, `setXAxisMode`, `setDisplayRate`, `setNumPoints` |
  | `/plotters/periodogram` | `setTitle` → `title`, `setSampleRate` → `sampleRate`, `setCenterFrequency` → `centerFrequency`, `setNumFFTBins` → `numFFTBins`, `setReferenceLevel` → `referenceLevel`, `setDynamicRange` → `dynamicRange`, `setAutoScale` → `autoScale`, `setWindowType("kaiser", {5.0})` |
  | `/plotters/spectrogram` | `setTitle` → `title`, `setDisplayRate` → `displayRate`, `setSampleRate` → `sampleRate`, `setCenterFrequency` → `centerFrequency`, `setNumFFTBins` → `numFFTBins`, `setTimeSpan` → `timeSpan`, `setReferenceLevel` → `referenceLevel`, `setDynamicRange` → `dynamicRange`, `setColorMap` |
  | `/plotters/wave_monitor` | `setTitle` → `title`, `setSampleRate` → `sampleRate`, `setNumPoints` → `numPoints`, `setNumInputs` → `numInputs` |
  | `/widgets/qwt_slider`, `qwt_thermo`, `qwt_knob`, `qwt_dial` | `setTitle`, `setLowerBound`, `setUpperBound`, `setValue` → `value` |

- Old paths: `/widgets/constellation`, `logic_analyzer`, `periodogram`,
  `spectrogram`, `wave_monitor`.
- The plotters make `/comms/wave_trigger` through the block registry of
  the environment. Data: `/blocks/feeder_source(dtype)` with the call
  `feedBuffer(BufferChunk)`.
- comms window code: `../PothosComms/filter/design/Window.cpp` (200
  lines, `designWindow(std::string_view type, int numTaps, double arg)`),
  reference data `windowCases` (84 cases, `ReferenceData.hpp` near line
  1265) with arrays `Detail::dN`. comms commit `71cddaf`. Plotters calls
  the window with `arg = windowArgs.empty() ? 0.0 : windowArgs[0]`.
- Models to copy: `../PothosWidgets/CMakeLists.txt` (`5f520c8`),
  `../PothosWidgets/cmake/PothosRunSelfTests.cmake` and
  `PothosTestEnv.cmake`, the check for blocks in
  `../PothosComms/cmake/PothosRunSelfTests.cmake`, the SoapySDR cache
  steps in `../PothosSoapy/.github/workflows/ci.yml`.
- Local: Arch `qwt` 6.3.0 (`/usr/lib/pkgconfig/Qt6Qwt6.pc`), `qt5-base`
  is installed (`/usr/lib/libQt5Widgets.so` is a Qt 5 library for the
  Qt 5 check test).

## Review Focus

- A Qwt for Qt 5 found before the Qwt for Qt 6 (for example in
  `/usr/local`): the configure step must stop, not crash at run time.
  Test: Task 2 Step 3.
- `windowArgs` empty for `kaiser` or `chebyshev` (the default of the
  blocks is `[]`, thus `arg = 0.0`): the window must have finite values,
  else the whole plot is NaN. Test: Task 2 Step 6
  (`test_window_default_args`).
- The installed modules must find `PothosPlotterUtils` from the stage
  prefix and from the install prefix, not only from the build tree: a
  relative RPATH. Test: Task 2 Step 9 and the CI check in Task 4.
- A plotter without PothosComms in the prefix: the runner stops with a
  clear message, and the block make gives an error that names
  `/comms/wave_trigger`, not a crash. Test: Task 3 Step 1 and Step 6.
- A queued setter (display slots called through `invokeMethod`) must not
  fail without a sign ("Cannot queue arguments"): the getter after the
  Qt events must show the new value. Test: Task 3 smoke test.

---

### Task 1: Core fix of `BufferChunk::operator ElementType`

**Files:**
- Modify: `include/Pothos/Framework/BufferChunk.hpp:155-164` and `:444-448`
- Create: `lib/Framework/Builtin/TestBufferChunkConversion.cpp`
- Modify: `lib/CMakeLists.txt:89` (add the test file next to `TestBufferChunkSpan.cpp`)
- Modify: `Changelog.txt` (section "API changes:" of pothos2)

**Interfaces:**
- Produces: `template <typename ElementType> requires std::is_pointer_v<ElementType> operator ElementType(void) const;`

- [ ] **Step 1: Search the toolkits for a conversion to a type that is not a pointer**

Clone upstream `pothosware/PothosFlow` into the scratchpad. Search
`../PothosBlocks`, `../PothosComms`, `../PothosSoapy`, `../PothosWidgets`,
`../PothosPlotters` and the PothosFlow clone for an implicit conversion of
a `BufferChunk` (or `buffer()`, `.payload`) to an integer, `bool` or a
class type. Expected: none. If one exists, stop and ask the user.

- [ ] **Step 2: Write the test**

`POTHOS_TEST_BLOCK("/framework/tests", test_buffer_chunk_conversion)` with
compile-time checks and one run-time check:

```cpp
static_assert(std::is_convertible_v<const Pothos::BufferChunk &, const float *>);
static_assert(std::is_convertible_v<const Pothos::BufferChunk &, void *>);
static_assert(not std::is_convertible_v<const Pothos::BufferChunk &, int>);
static_assert(not std::is_convertible_v<const Pothos::BufferChunk &, size_t>);
static_assert(not std::totally_ordered<Pothos::BufferChunk>);
//run time: const float *p = chunk; p == chunk.as<const float *>()
```

- [ ] **Step 3: Build and see the compile error**

Run: `cmake --build build/conan-dbg`
Expected: FAIL at the `not std::is_convertible_v<..., int>` assertion.

- [ ] **Step 4: Add the constraint**

Add `requires std::is_pointer_v<ElementType>` to the declaration and the
definition. Update the Doxygen comment: only pointer types.

- [ ] **Step 5: Run the core tests**

Run: `cmake --build build/conan-dbg && ctest --test-dir build/conan-dbg --output-on-failure`
Expected: all pass, also `pothos.api_compat` and `pothos.consumer`.
Then rebuild the upstream plotters spike copy against the new stage:
the `BufferChunk.hpp:448` errors are gone.

- [ ] **Step 6: Changelog and commit**

Changelog line: "BufferChunk converts implicitly only to pointer types
(Qt 6 moc refused BufferChunk in slots)".

```bash
git add include/Pothos/Framework/BufferChunk.hpp lib/Framework/Builtin/TestBufferChunkConversion.cpp lib/CMakeLists.txt Changelog.txt
git commit -m "BufferChunk: the conversion operator accepts only pointer types"
```

- [ ] **Step 7: PR (ask the user first)**

Push the core branch `phase14-plotters` (spec, plan, fix) with
`git push -u origin phase14-plotters`. Open a PR into `pothos2`
(`gh pr create -R markvin/PothosCore --base pothos2`). Merge only on the
word of the user. Tasks 2-3 can continue locally against the local
staged core.

---

### Task 2: Build, Qwt 6.3 fixes and the window code

**Files (in `../PothosPlotters`):**
- Modify: `CMakeLists.txt` (rewrite), `*/CMakeLists.txt` (6 modules)
- Create: `cmake/FindQwt.cmake` (replaces the upstream file),
  `cmake/QwtCheckQt.cmake`, `cmake/PothosRunSelfTests.cmake`,
  `cmake/PothosTestEnv.cmake`
- Create: `PlotterUtils/Window.cpp`, `PlotterUtils/Window.hpp`
- Create: `Tests/CMakeLists.txt` (module `PlotterTests`, `DESTINATION
  plotters`, links `Qt6::Widgets Qwt::Qwt PothosPlotterUtils`),
  `Tests/WindowReferenceData.hpp`, `Tests/TestWindow.cpp`
- Modify: `PlotterUtils/PothosPlotterFFTUtils.hpp` and the files of the
  spike errors (refer to the facts)

**Interfaces:**
- Consumes: Task 1 (local core build with the fix, installed into a
  prefix).
- Produces: targets `PothosPlotterUtils` (shared), `PlotterTests`
  (module), `Qwt::Qwt` (imported),
  the function `plotters_warnings(<target>)`, the CTest tests
  `plotters.stage` and `plotters.selftests`, the CMake list
  `PLOTTERS_TESTS` of the test names in `/plotters/tests`;
  `POTHOS_PLOTTER_UTILS_EXPORT std::vector<double> PothosPlotters::designWindow(std::string_view type, int numTaps, double arg);`

- [ ] **Step 1: Branches**

In `../PothosPlotters`: `git switch -c pothos2 4a9e7d1`, then
`git switch -c phase14-plotters`. `git rm qwt6 .gitmodules`.

- [ ] **Step 2: Top-level CMake**

Copy the structure of `../PothosWidgets/CMakeLists.txt`: project
`PothosPlotters`, options `PLOTTERS_*`, `CMP0144`, default install prefix,
Qt 6 with the same error text, `plotters_warnings()`. Then
`list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)` and
`find_package(Qwt 6.3.0)` with a `FATAL_ERROR` that names `qwt` (Arch), the
Qt 6 Qwt package of the system, `CMAKE_PREFIX_PATH` and `Qwt_ROOT`, and
README.md. `PothosPlotterUtils`: `add_library(... SHARED)`, `AUTOMOC`,
`DEFINE_SYMBOL POTHOS_PLOTTER_UTILS_MAKEDLL`, links
`PRIVATE Pothos::Pothos PUBLIC Qt6::Widgets Qwt::Qwt`, installed with
`LIBRARY/ARCHIVE/RUNTIME` destinations. Module CMake files: remove the
`cmake_dependent_option`/`add_feature_info` lines; `LIBRARIES Qt6::Widgets
Qwt::Qwt PothosPlotterUtils`; `AUTOMOC`, `CXX_EXTENSIONS OFF` and
`plotters_warnings()` on each module target.

- [ ] **Step 3: `FindQwt.cmake` and the Qt 5 check, with a failing test first**

`cmake/QwtCheckQt.cmake` is a script (`cmake -DQWT_LIBRARY=<file> -P`):
it calls `file(GET_RUNTIME_DEPENDENCIES)` on the Qwt shared library and
fails with `message(FATAL_ERROR ...)` when a resolved or unresolved
dependency matches `Qt5` (`libQt5`, `Qt5Core.dll`,
`QtCore.framework/Versions/5`). First run it on a Qt 5 library and on the
Qwt for Qt 6:

Run: `cmake -DQWT_LIBRARY=/usr/lib/libQt5Widgets.so -P cmake/QwtCheckQt.cmake`
Expected: FAIL with a message that names Qt 5.
Run: `cmake -DQWT_LIBRARY=/usr/lib/libqwt.so -P cmake/QwtCheckQt.cmake`
Expected: success.

`FindQwt.cmake`: with `PkgConfig`, `pkg_check_modules(Qt6Qwt6)` first;
when only `Qt5Qwt6` exists, stop with the Qt 5 message. Else
`find_path(qwt_global.h PATH_SUFFIXES qwt qwt6 include)`,
`find_library(NAMES qwt qwtd)`, a macOS framework (`qwt.framework`). The
version from `QWT_VERSION_STR` in `qwt_global.h`. Without pkg-config, run
`QwtCheckQt.cmake` with `execute_process` (on Windows on the `qwt.dll`
next to the import library, in `../bin` or the same directory). Make
`Qwt::Qwt` (`UNKNOWN IMPORTED`, `INTERFACE_INCLUDE_DIRECTORIES`,
`INTERFACE_COMPILE_DEFINITIONS QWT_DLL` on Windows, interface link to
`Qt6::Widgets`), `find_package_handle_standard_args(Qwt REQUIRED_VARS ...
VERSION_VAR Qwt_VERSION)`.

- [ ] **Step 4: Configure**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<core prefix> -DPLOTTERS_WARNINGS_AS_ERRORS=ON`
Expected: "Qwt 6.3.0" found through pkg-config; configure OK.
Also: `-DCMAKE_DISABLE_FIND_PACKAGE_PkgConfig=ON` finds the same Qwt
through `find_library` and runs the Qt check.

- [ ] **Step 5: Window code and reference data**

Copy `../PothosComms/filter/design/Window.cpp` to `PlotterUtils/Window.cpp`
(keep the copyright and SPDX lines, add "Copied from PothosComms 71cddaf"),
namespace `PothosPlotters`, exported with `POTHOS_PLOTTER_UTILS_EXPORT`
(the macro of `PlotUtilsConfig.hpp`). `Window.hpp` declares it (not
installed). `Tests/WindowReferenceData.hpp`: copy `WindowCase`, `windowCases`
and only the `Detail::dN` arrays that they use, with a short script in
the scratchpad (not committed); header comment: generated from
`ReferenceData.hpp` of PothosComms `71cddaf` (spuce `ad08df7` values).

- [ ] **Step 6: Window tests**

`Tests/TestWindow.cpp` (in the module `PlotterTests`: a test block must
be in a module, not in the shared library):
- `test_window_reference`: for each case, `designWindow(type, numTaps, arg)`
  has the reference size, and each tap differs by at most 1e-9 (NaN equals
  NaN), as `test_filter_design_window` of comms.
- `test_window_default_args`: for each of `rectangular hann hamming
  blackman bartlett flattop kaiser chebyshev` with `arg = 0.0` and 1024
  taps, all taps are finite. If a type gives a value that is not finite,
  stop and ask the user (upstream behavior).

Add both names to `PLOTTERS_TESTS`.

- [ ] **Step 7: Use the window code**

`PothosPlotterFFTUtils.hpp`: include `Window.hpp`, call
`PothosPlotters::designWindow(_windowType, int(fftBins.size()), arg)`.
Remove the spuce include.

- [ ] **Step 8: Qwt 6.3 and Qt 6 fixes until the build is clean**

Fix the spike errors (the facts). Spectrogram raster data: keep the 3
intervals (x, y, z) as members of `MySpectrogramRasterData`, override
`QwtInterval interval(Qt::Axis) const`. Replace `[=]` captures of `this`
with `[=, this]`. Then fix each warning of the build.

Run: `cmake --build build`
Expected: no errors and no warnings (with `-Werror`).

- [ ] **Step 9: Run time paths and test runner**

Copy the runner files of widgets. Add the check of comms: the runner stops
with a clear message when `<PREFIX>/<libdir>/Pothos/modules*/comms` or
`.../blocks` does not exist (the text of the comms runner). On Windows
the runner puts `<stage>/bin` on `PATH`. CTest `plotters.stage` and
`plotters.selftests` as in widgets (with `QT_QPA_PLATFORM=offscreen`).
Install RPATH: the modules get `$ORIGIN/../../..` (Linux) or
`@loader_path/../../..` (macOS) to the library directory
(`<libdir>/Pothos/modules<ABI>/plotters` → `<libdir>`); the Qt and Qwt
library directories when they are not in
`CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES`.

Run: `ctest --test-dir build --output-on-failure` (core prefix with blocks
and comms installed)
Expected: `plotters.stage`, `plotters.selftests` pass (2 window tests).
Run: `readelf -d build/stage/lib/Pothos/modules*/plotters/libWaveMonitor.so | grep RUNPATH`
Expected: `$ORIGIN/../../..`.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "Build against the pothos2 core with Qt 6 and Qwt 6.3; own window code"
```

---

### Task 3: Smoke test and data test

**Files:**
- Create: `Tests/TestPlotters.cpp`
- Modify: `Tests/CMakeLists.txt` (the new source), `PLOTTERS_TESTS`

**Interfaces:**
- Consumes: Task 2 targets and `PLOTTERS_TESTS`.
- Produces: tests `/plotters/tests/test_<block>` for the 9 blocks
  (`test_constellation`, `test_logic_analyzer`, `test_periodogram`,
  `test_spectrogram`, `test_wave_monitor`, `test_qwt_slider`,
  `test_qwt_thermo`, `test_qwt_knob`, `test_qwt_dial`),
  `test_old_paths`, and `test_data_<plotter>` for the 5 plotters.

- [ ] **Step 1: Without comms, the runner stops**

Run the runner with `-DPREFIX=` a core prefix without comms.
Expected: FAIL with the message that names PothosComms.

- [ ] **Step 2: Smoke tests**

A helper makes sure that a `QApplication` exists (as `TestWidgets.cpp` of
widgets: `offscreen` when `QT_QPA_PLATFORM` is not set). For each block
of the facts table: `Pothos::BlockRegistry::make(path)`,
`block.call<QWidget *>("widget")` is not null, call each setter with a
typical value, process the Qt events, `POTHOS_TEST_EQUAL` on each getter.
`test_old_paths`: each old `/widgets/` path makes a block with a widget.

- [ ] **Step 3: Run the smoke tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all pass, no "Cannot queue arguments" in the output.

- [ ] **Step 4: Data tests**

For each plotter: a `Pothos::Topology` with
`/blocks/feeder_source("complex_float32")` (logic analyzer:
`"int32"`) → plotter input 0. Set `setNumPoints(1024)` (or
`setNumFFTBins(1024)`) and `setDisplayRate(100.0)`. Feed 64 × 1024
samples (a tone). `commit()`, then process the Qt events until the plot
has data or 10 s pass. The check, through the widget, with no new block
call: `widget->findChild<QwtPlot *>()` has a `QwtPlotCurve` with
`dataSize() > 0` (wave monitor, constellation, periodogram), a
`QwtPlotSpectrogram` whose raster data has a not empty interval
(spectrogram), the `QTableWidget` has rows with text (logic analyzer).
Then disconnect all, commit, and delete the topology.

- [ ] **Step 5: Run the data tests**

Run: `ctest --test-dir build --output-on-failure`
Expected: all pass in less than 60 s.

- [ ] **Step 6: The error without `/comms/wave_trigger`**

Check by hand: run `PothosUtil --self-test1=/plotters/tests/test_wave_monitor`
with a `POTHOS_PLUGIN_PATH` that does not have comms. Expected: an error
that names `/comms/wave_trigger`, no crash. Record the output in the
ledger.

- [ ] **Step 7: Commit**

```bash
git add Tests CMakeLists.txt
git commit -m "Smoke tests of the 9 blocks and data tests of the 5 plotters"
```

---

### Task 4: Docs, CI, review, fork and PR

**Files:**
- Delete: `debian/`, `cmake/` upstream files that are not used
- Modify: `README.md`, `Changelog.txt`
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: README and Changelog**

README: what plotters is; dependencies (the pothos2 core, Qt 6 ≥ 6.5,
Qwt ≥ 6.3 for Qt 6; PothosComms and PothosBlocks at run time and for the
tests); build and test commands; the Qwt package names (Arch `qwt`) and
`CMAKE_PREFIX_PATH`/`Qwt_ROOT`. Changelog: "Release 0.5.0 (pending)": the
port to the pothos2 core, Qt 6 and Qwt 6.3 only, own window code (no
spuce), the self-tests. `git rm -r debian`.

- [ ] **Step 2: CI**

Copy `../PothosWidgets/.github/workflows/ci.yml`. Add:
1. Check out `markvin/PothosBlocks@pothos2` and
   `markvin/PothosComms@pothos2`. Build and install them into the core
   prefix with their tests off.
2. Qwt: env `QWT_VERSION: '6.3.0'`, the archive
   `https://downloads.sourceforge.net/project/qwt/qwt/6.3.0/qwt-6.3.0.tar.bz2`
   with its SHA-256 (download it once locally, put the sum in the
   workflow). Restore and save with `actions/cache` as soapy (key: Qwt
   version, job, Qt version, hash of the workflow file). Build: edit
   `qwtconfig.pri` (`QWT_INSTALL_PREFIX`; remove `QwtDesigner`,
   `QwtExamples`, `QwtPlayground`, `QwtTests`, `QwtSvg`, `QwtOpenGL`,
   `QwtFramework`), then `qmake`, `make`/`nmake`/`jom` (Windows in the
   MSVC environment), install.
3. `CMAKE_PREFIX_PATH` with the Qwt prefix; Windows: the Qwt `lib`
   directory on `PATH`.
4. The RPATH check of widgets, with `$ORIGIN/../../..` and the Qwt prefix.
- `linux-system`: add `qwt` to the pacman line, and build blocks and
  comms there too.

- [ ] **Step 3: Whole-branch review**

One fresh reviewer reads the whole branch (`git diff 4a9e7d1..HEAD`)
against the spec. Fix what it finds; record the rulings in the ledger.

- [ ] **Step 4: Fork, push, CI (ask the user first)**

`gh repo fork pothosware/PothosPlotters --clone=false` (account
markvin), remote `origin` to the fork, push `pothos2` and
`phase14-plotters`. Expected: CI 5/5 green. Fix CI failures with
follow-up commits.

- [ ] **Step 5: PR (ask the user first)**

`gh pr create -R markvin/PothosPlotters --base pothos2`. Merge only on
the word of the user, after the core PR of Task 1 is merged.

---

### Task 5: PothosBase

**Files (in `../PothosBase`, branch `add-plotters` from `main`):**
- Modify: `.gitmodules`, `CMakeLists.txt`, `cmake/PothosBaseSelfTests.cmake`,
  `tests/configure_checks.cmake`, `.github/workflows/ci.yml`, `README.md`,
  `Changelog.txt`
- Create: submodule `plotters`

- [ ] **Step 1: Submodule and options**

`git submodule add -b pothos2 https://github.com/markvin/PothosPlotters.git plotters`.
Add `plotters` to `pothos_base_all_toolkits`, `POTHOS_BASE_WITH_PLOTTERS`
(ON), `POTHOS_BASE_PLOTTERS_DIR`, `PLOTTERS_WARNINGS_AS_ERRORS`, plotters in
the check of the toolkits whose tests need blocks, and a check: plotters
ON with comms OFF stops with a clear message.

- [ ] **Step 2: Configure checks**

Add to `tests/configure_checks.cmake`: `without_plotters`
(`-DPOTHOS_BASE_WITH_PLOTTERS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qwt=ON`
configures), `plotters_without_comms` (stops with the message). Run the
configure checks. Expected: all OK.

- [ ] **Step 3: Build and test locally**

Run: build PothosBase with `-DPOTHOS_BASE_PLOTTERS_DIR=../PothosPlotters`
and `ctest`. Expected: `base.modules` loads the plotters modules;
`base.selftests` runs the `/plotters/tests` tests; all pass.

- [ ] **Step 4: CI, README, Changelog, PR (ask the user first)**

CI: the Qwt step of Task 4 in the Conan job, `qwt` in `linux-system`,
`plotters` in `-DTOOLKITS=...` and the Qwt prefix in `-DPREFIX_PATH=...`.
README: Qwt 6.3 for Qt 6 is a dependency. Commit, push `add-plotters`,
PR into `main`. Expected: CI 2/2 green.

---

### Task 6: Core docs

**Files (core, branch `phase14-plotters-docs` from `pothos2` after the merge of Task 1):**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`,
  `CLAUDE.md`, `README.md`, `Changelog.txt`

- [ ] **Step 1: Roadmap**

Phase 14 row (plotters; spec and plan names) and a status paragraph:
the PRs and merges, the CI results, the number of self-tests in
PothosBase, the core fix. The order of the next toolkits: flow, then
python (skipped for now, user decision 2026-10-30).

- [ ] **Step 2: `CLAUDE.md`, README and Changelog of the core**

`CLAUDE.md`: a paragraph about PothosPlotters as for soapy (fork, branch
`pothos2`, local clone `../PothosPlotters`, Qwt 6.3 for Qt 6 from the
system or built from source in CI, not from Conan; comms and blocks at
run time and for the tests). README and Changelog: plotters in the list
of the ported toolkits.

- [ ] **Step 3: Check and PR (ask the user first)**

Run: `git grep -n /home/` → no output. Commit, push, PR into `pothos2`.
