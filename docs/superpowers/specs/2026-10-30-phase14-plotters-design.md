# Phase 14: PothosPlotters on the pothos2 core

Date: 2026-10-30. Status: design. Work happens in a new fork
`markvin/PothosPlotters`, then in `markvin/PothosBase`. The core gets one
small fix of the public header `BufferChunk.hpp` and its docs.

## Intent

PothosPlotters gives the graphical plotters of the Pothos GUI: the wave
monitor (time domain), the periodogram and the spectrogram (frequency
domain), the constellation (IQ) and the logic analyzer. It also gives
4 control widgets on Qwt (slider, thermo, knob, dial). PothosFlow shows
them. The roadmap ports the toolkits one at a time. python is skipped
for now (user decision, 2026-10-30), thus plotters is phase 14 and flow
comes after it.

When this phase is complete, plotters builds against an installed
pothos2 core with Qt 6 and Qwt 6, has no compiler warnings, has
self-tests and CI, and is a submodule of PothosBase.

## Decisions of the user

1. python is skipped for now. plotters is phase 14 (2026-10-30).
2. Qwt comes only from `find_package`: from the system or from a prefix
   that the user gives. The plotters build does not compile Qwt and has no
   copy of the Qwt sources. CI builds Qwt from the upstream source in a
   separate step with a cache (the same method as SoapySDR in phase 13).
   (2026-10-30, variant A.)
3. plotters has its own small copy of the window code (spuce
   `design_window`). The spuce replacement in comms stays internal to
   comms (2026-10-28).
4. Qt 6 only, as in widgets: Qt from the system or from
   `install-qt-action` in CI, not from Conan (phase 12).

## Current state (recon on upstream `4a9e7d1`, 2026-10-30)

- 6 modules, all with `DESTINATION plotters`:
  - `Constellation` — `/plotters/constellation`
  - `LogicAnalyzer` — `/plotters/logic_analyzer`
  - `Periodogram` — `/plotters/periodogram`
  - `Spectrogram` — `/plotters/spectrogram`
  - `WaveMonitor` — `/plotters/wave_monitor`
  - `QwtWidgets` — `/widgets/qwt_slider`, `qwt_thermo`, `qwt_knob`,
    `qwt_dial`
- The 5 plotters also register an old path under `/widgets/` (for
  example `/widgets/wave_monitor`).
- A shared library `PothosPlotterUtils` (`PlotterUtils/`: the plot
  widget, the picker, the styles, the FFT helper). All plotters except
  `QwtWidgets` link it. It is installed into the library directory.
- Each plotter is a topology: the display block and a
  `/comms/wave_trigger` block, made at run time through the block
  registry. Thus the plotters need PothosComms at run time (and comms
  needs PothosBlocks for its own self-tests). No build dependency.
- No Poco. No self-tests.
- spuce: only `spuce::design_window` in
  `PlotterUtils/PothosPlotterFFTUtils.hpp` (Periodogram, Spectrogram).
  The window types in the block docs: rectangular, hann, hamming,
  blackman, bartlett, flattop, kaiser (beta), chebyshev (atten).
- Qwt: the submodule `qwt6` (`opencor/qwt`) and a fallback that compiles
  Qwt from it, or `FindQwt.cmake`. Qt 5 or Qt 6 with
  `find_package(QT NAMES Qt6 Qt5)`, components `Widgets Concurrent`.
  The plotters code uses only Qt Widgets (no Concurrent, OpenGL, Svg,
  PrintSupport).
- CMake: `cmake_minimum_required(VERSION 3.1.0)` (CMake 4 refuses it),
  `ENABLE_PLOTTERS*` with `cmake_dependent_option` (a missing Qt, Qwt or
  spuce turns modules off without an error), global `CMAKE_AUTOMOC`,
  links the target `Pothos` (the pothos2 core exports `Pothos::Pothos`).
- `debian/` exists. `Spectrogram/gen_colormaps.py` generated
  `GeneratedColorMaps.cpp` (the generated file is in git).
- Spike (2026-10-30, scratchpad: core `pothos2` `1e7047a9`, Arch Qt 6.12,
  Arch Qwt 6.3.0 built for Qt 6, spuce `ad08df7`, `Pothos` →
  `Pothos::Pothos`):
  - Qwt 6.3 API changes, about 20 errors in 7 files: missing includes
    (`qwt_text.h`, `qwt_interval.h`, `QPen`, `QStack`),
    `QwtPlotZoomer::setAxis` → `setAxes`, `QwtRasterData::setInterval`
    is removed (the subclass must give `interval()`; the spectrogram
    raster class is abstract now), `QPalette::Background` →
    `QPalette::Window`.
  - C++20 warnings: implicit capture of `this` with `[=]`.
  - An error in the core header: refer to the next section.

## The core fix: `BufferChunk` conversion operator

`Pothos::BufferChunk` has a template conversion operator without a
constraint:

```cpp
template <typename ElementType>
operator ElementType(void) const; // reinterpret_cast<ElementType>(address)
```

Qt 6 moc makes a meta type for each type in a signal or slot. The meta
type checks if `a < b` is a valid expression. With the operator, the
compiler accepts `a < b` through a conversion to `int`. Then it
instantiates `operator int`, and `reinterpret_cast<int>(size_t)` does not
compile. Each plotter slot with `const Pothos::BufferChunk &` has this
error. The old API on `master` has the same operator.

The fix:

- Constrain the operator with a `requires` clause so that only pointer
  types are accepted (`std::is_pointer_v<ElementType>`). Then the
  compiler cannot find `a < b` or `a == b` through the operator.
- Before the change, examine the toolkits (`../PothosBlocks`,
  `../PothosComms`, `../PothosSoapy`, `../PothosWidgets`,
  `../PothosPlotters`, and upstream PothosFlow) for an implicit
  conversion of a `BufferChunk` to a type that is not a pointer (for
  example `size_t x = chunk;`). PothosFlow has no local clone: clone it
  into the scratchpad for this check. If a toolkit uses one, stop and ask
  the user.
- Add a core test that is not dependent on Qt: a `static_assert` (or a
  self-test) that `BufferChunk` converts to `const float *` and `void *`,
  and that `a < b` and conversion to `int` are not valid.
- `pothos.api_compat` and `pothos.consumer` must pass.
- The fix is a PR into the core `pothos2`, merged before the plotters CI
  can pass (the plotters CI builds the core `pothos2`). The Changelog of
  the core records it.

## Goals

- The fork `markvin/PothosPlotters`, branch `pothos2`, builds against an
  installed pothos2 core, Qt 6 (6.5 or newer) and Qwt 6.3 or newer for
  Qt 6, with CMake 3.25 or newer (also CMake 4).
- No compiler warnings with `-Wall -Wextra` (GCC and Clang).
  `PLOTTERS_WARNINGS_AS_ERRORS=ON` turns them into errors.
- Self-tests: the window code against reference data, and a smoke test
  of all 9 blocks with data through the 5 plotters. They pass on Linux,
  macOS and Windows in CI.
- PothosBase builds plotters as a submodule. Its tests load the plotters
  modules and run the plotters self-tests from the stage prefix.
- The core docs record phase 14.

## Non-goals

- No check of the look of the plots (colors, layout, fonts).
- No new plotters, no new block features, no change of the block API
  (block paths, the old `/widgets/` paths, calls, signals, slots,
  `|PothosDoc` comments), except a fix of a bug.
- No Qwt from Conan, no Conan recipe for Qwt.
- No packages (Debian, Conan package, installer).
- No other change of the public API of the core than the `BufferChunk`
  fix.

## Anti-goals

These results are failures, also when the build is correct:

- The configure step continues without Qt 6 or without Qwt and gives a
  build without some modules. A missing Qt 6 or Qwt must stop the
  configure step with a clear message.
- The plotters build compiles Qwt, or the repository keeps a copy of Qwt
  (submodule or sources).
- Qwt built for Qt 5 is accepted. It must be refused with a clear message
  at the configure step, not with a crash at run time.
- A CMake file refers to Conan, or the configure or build step downloads
  files.
- The smoke test passes on a platform because it skips a block without a
  message. A missing `/comms/wave_trigger` must be an error, not a skip.
- plotters builds in PothosBase but not standalone against an installed
  core (or the opposite).
- The installed modules load only from the build tree. They must load
  from the install prefix (Qt, Qwt and `PothosPlotterUtils` must be found
  at run time).

## Repository and history

1. Fork `pothosware/PothosPlotters` to `markvin/PothosPlotters`. Create
   the fork only after the user agrees at that step of the plan.
2. Make the branch `pothos2` from upstream `master` (`4a9e7d1`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosPlotters` (it exists, upstream clone).
4. The work goes into a work branch and a PR into `pothos2` of the fork.
   The user approves the fork, the first push and each merge.
5. Remove the submodule `qwt6` and `.gitmodules`.

## Build (the same method as widgets)

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosPlotters CXX)`,
  C++23 (`CXX_EXTENSIONS OFF`).
- `cmake_policy(SET CMP0144 NEW)` and
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with the
  comment of widgets that tells why.
- `find_package(Qt6 6.5 CONFIG COMPONENTS Widgets)` with the clear error
  of widgets.
- Qwt (decision 2):
  - A new `cmake/FindQwt.cmake` gives the imported target `Qwt::Qwt` and
    `Qwt_VERSION`. Minimum version 6.3.0.
  - It finds Qwt through pkg-config (`Qt6Qwt6`) when pkg-config is
    available, and else through `find_path`/`find_library` in
    `CMAKE_PREFIX_PATH` (Windows, a Qwt installed with qmake). It also
    finds a Qwt framework on macOS (Homebrew).
  - It must refuse a Qwt that is built for Qt 5. Decision of the builder:
    the method (for example the pkg-config name `Qt6Qwt6`, or the Qt
    library that the Qwt library links to).
  - `QWT_DLL` is an interface define of `Qwt::Qwt` on Windows.
  - If Qwt is missing or too old, stop with one clear message: install
    `qwt` (Arch), the Qt 6 Qwt package of the system, or give
    `CMAKE_PREFIX_PATH` (or `Qwt_ROOT`) to a Qwt for Qt 6. Refer to the
    README.
- Remove `ENABLE_PLOTTERS*`, `cmake_dependent_option`, the spuce search,
  the Qt 5 code, `Concurrent`, and the built-in Qwt. All modules always
  build.
- `PothosPlotterUtils` stays a shared library, installed into the library
  directory of the prefix (`bin` for the DLL on Windows). It links
  `Pothos::Pothos`, `Qt6::Widgets` and `Qwt::Qwt`.
- `AUTOMOC` as a target property, not the global `CMAKE_AUTOMOC`.
- The default install prefix is the prefix of the core when the user
  gives no `CMAKE_INSTALL_PREFIX` (as in widgets).
- Options: `PLOTTERS_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `PLOTTERS_WARNINGS_AS_ERRORS` (default OFF).
- Warnings: `-Wall -Wextra` for GCC and Clang, `-Werror` with
  `PLOTTERS_WARNINGS_AS_ERRORS`. Qt and Qwt headers come as system
  includes from the imported targets. Fix the warnings in the plotters
  code; do not turn them off.
- Run time: the installed modules must find Qt, Qwt and
  `PothosPlotterUtils`. On Linux and macOS use an install RPATH to the Qt
  and Qwt library directories when they are not system directories, and
  to the library directory of the prefix. On Windows the Qt and Qwt `bin`
  directories are on `PATH`. The RPATH to the library directory of the
  prefix is relative (`$ORIGIN` on Linux, `@loader_path` on macOS): the
  stage tests load the modules from `build/stage`, and an absolute path
  goes to the core prefix. Decision of the builder: the exact method.
- plotters needs no Conan packages. It has no `conanfile.txt`.
- Keep the layout of the sources (one directory for each module).
- Delete `debian/`. Update `README.md` (dependencies: the pothos2 core,
  Qt 6, Qwt 6.3 for Qt 6, PothosComms at run time; build; run the tests)
  and `Changelog.txt` (a "Release 0.5.0 (pending)" entry: the port to the
  pothos2 core, Qt 6 and Qwt 6.3 only, no spuce).

## Window code (decision 3)

- Copy `filter/design/Window.cpp` of PothosComms (the port of spuce
  `design_window`, BSL-1.0, with its copyright lines) into
  `PlotterUtils/`. Keep only what `designWindow` needs, in a plotters
  namespace. `PothosPlotterFFTUtils.hpp` is a header that the modules
  include, thus the window function is compiled into `PothosPlotterUtils`
  and exported from it. Its header is not installed.
- `PothosPlotterFFTUtils.hpp` calls it in place of `spuce::design_window`.
  The same window types and arguments as before.
- A self-test compares the windows with reference data: copy the
  `windowCases` of the comms `ReferenceData.hpp` (spuce `ad08df7`
  values), with the same tolerance as comms (1e-9).

## Qwt 6.3 and Qt 6 fixes

- Fix the errors of the spike: the includes, `setAxes`, the spectrogram
  raster data (give `interval(Qt::Axis)` from member values in place of
  `setInterval`), `QPalette::Window`.
- Replace `[=]` with an explicit capture where `this` is captured.
- Fix other Qt 6 and Qwt 6.3 deprecation warnings when the build shows
  them.
- Keep the behavior the same: the zoom axes, the color map range of the
  spectrogram, the background color.

## Tests

- The method of widgets:
  1. CTest test `plotters.stage` installs plotters into `build/stage`.
  2. CTest test `plotters.selftests` runs each `/plotters/tests/...` test
     with the `PothosUtil` of the installed core, with
     `QT_QPA_PLATFORM=offscreen`.
- The test runner (a copy of `cmake/PothosRunSelfTests.cmake` of comms)
  checks that PothosComms and PothosBlocks are installed in the prefix of
  the core. If not, it stops with a clear message.
- On Windows the test runner puts the `bin` directory of the stage prefix
  (`PothosPlotterUtils.dll`) on `PATH`. `PothosUtil.exe` comes from the
  core prefix and does not find that DLL otherwise. CI puts the Qwt DLL
  directory on `PATH` (qmake installs `qwt.dll` into `lib`).
- Self-tests in `/plotters/tests`:
  - The window test (refer to "Window code").
  - Smoke test of the 9 blocks: make sure that a `QApplication` exists
    (`offscreen` when `QT_QPA_PLATFORM` is not set, as in widgets). For
    each block: make it with `Pothos::BlockRegistry::make`, get its
    widget with the `widget` call, call its setters with typical values,
    and check the values of the getters that exist. Then delete the
    block. Make each plotter also once through its old `/widgets/` path.
  - Data test of the 5 plotters: a topology `/blocks/feeder_source` →
    plotter for each plotter (complex float samples for the
    constellation, periodogram, spectrogram and wave monitor; a type that
    the logic analyzer accepts). Commit it, process the Qt events in the
    test thread until the display gets samples or a timeout comes. The
    check: the topology runs without an error and the display got data.
    Decision of the builder: how the test sees that the display got data
    (for example the plot curve data through the widget, or a Qt
    property). It must not add a registered call to a block (non-goals).
- The test must not skip a block without a message.
- Risk: Qt can refuse a `QApplication` that is not in the main thread,
  for example on macOS. If a platform refuses it, stop and ask the user.
  Do not skip the test on that platform.

## CI of plotters

- The same structure as the CI of widgets: linux-gcc (Debug, `-Werror`),
  linux-clang (Release, `-Werror`), macos (Intel runner,
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC Release). The Qt
  versions as in widgets (6.5.3; 6.8.3 on Windows).
  1. Check out plotters, `markvin/PothosCore@pothos2`,
     `markvin/PothosBlocks@pothos2` and `markvin/PothosComms@pothos2`.
  2. Install the dependencies of the core, blocks and comms with Conan.
  3. Install Qt 6 with `install-qt-action`.
  4. Build Qwt 6.3.0 from the upstream source (SourceForge release
     archive, checked with its SHA-256) with the qmake of that Qt. Build
     only the library: no designer plugin, no examples, no tests, no
     framework on macOS, no `QwtSvg` and no `QwtOpenGL`
     (`install-qt-action` does not install Qt Svg by default). Install it into its own prefix. Cache the prefix
     with `actions/cache` (key: Qwt version, job, Qt version, hash of the
     workflow file); save the cache before the plotters tests, as soapy
     does.
  5. Build and install the core, then blocks and comms into the core
     prefix (their own tests off).
  6. Build plotters and run `ctest -V`.
- Job `linux-system` in an `archlinux` container (`--init`): system
  packages with `qt6-base` and `qwt`, no Conan.
- Keep the guard against downloads in the CMake files.
- Decision of the builder: the Windows qmake build (`nmake` or `jom` in
  the MSVC environment).

## PothosBase

After `pothos2` of the fork has plotters:

- Add the submodule `plotters` (HTTPS URL, `branch = pothos2`), the
  options `POTHOS_BASE_WITH_PLOTTERS` (ON by default) and
  `POTHOS_BASE_PLOTTERS_DIR`, and `PLOTTERS_WARNINGS_AS_ERRORS` from
  `POTHOS_BASE_WARNINGS_AS_ERRORS`.
- If `POTHOS_BASE_WITH_PLOTTERS` is ON, comms must be ON too. Else stop
  the configure step with a clear message.
- The checks `base.modules` and `base.selftests` include plotters (module
  directory `/plotters/`, tests `/plotters/tests/`).
- CI of PothosBase: the Qwt step (as in plotters) in the job with Conan,
  `qwt` in `linux-system`. `tests/configure_checks.cmake` gets the Qwt
  prefix. Add plotters to the check of the tests that need blocks.
- README of PothosBase: Qwt 6.3 for Qt 6 is a dependency.

## Changes in the core

- The `BufferChunk` fix (refer to its section), with a Changelog entry.
- Roadmap: phase 14 row and status. The order of the next toolkits is
  flow, then python (skipped for now).
- `CLAUDE.md`: a short note about the plotters fork, its branch, its
  local clone and its dependencies (Qwt, comms at run time).

## Decisions of the designer (the user must check them)

- The core fix: constrain `operator ElementType` to pointer types, not a
  workaround in plotters (for example a wrapper type for the Qt slots).
  PothosFlow will need the same fix.
- Minimum Qwt 6.3.0: CI and Arch test only 6.3.0. Qwt 6.2 has the new
  axis API too, but no job tests it.
- Qwt in CI: the release archive from SourceForge, built with qmake (the
  build system of upstream), not with a CMake script of plotters.
- `PothosPlotterUtils` stays a shared library, as upstream.
- The smoke test and the data test need PothosComms and PothosBlocks in
  the prefix. The plotters CI builds both.
- `POTHOS_BASE_WITH_PLOTTERS` is ON by default, as the other toolkits.
  Thus Qwt becomes a dependency of PothosBase.
- `debian/` and the `qwt6` submodule are deleted. Changelog entry
  "Release 0.5.0 (pending)".
- Order: core fix PR → plotters → PothosBase → core docs.

## Recommended order of work

1. The core fix of `BufferChunk` with its test, a PR into `pothos2`.
2. Fork and branch. New CMake with `FindQwt.cmake`, the window code with
   its test, the Qwt 6.3 fixes. The build is clean locally with Arch Qt
   6.12 and Qwt 6.3.0, against a core with the fix.
3. The test runner, the smoke test and the data test, locally with
   `offscreen`.
4. README, Changelog, delete `debian/` and `qwt6`.
5. CI (4 jobs with Conan, `install-qt-action` and Qwt from source, and
   `linux-system`). PR into `pothos2` of the fork.
6. PothosBase: the plotters submodule, checks, CI.
7. Core docs: roadmap and `CLAUDE.md`.
