# Core modernization — roadmap

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`

The work has phases. Each phase has its own plan. After each phase the core
builds, and `PothosUtil --self-tests` is green in CI on Linux/macOS/Windows.
Poco goes away step by step: the intermediate phases keep it as a
**private** in-tree dependency (the spec permits this).

| # | Phase | Result | Plan |
|---|-------|--------|------|
| 1 | Build foundation | CMake ≥ 3.25 / works on 4.x, C++23, json in the tree, Poco and muparserx private, self-tests through CTest, export of `Pothos::Pothos` without Poco, CI on GitHub Actions | `2026-10-26-phase1-build-foundation.md` |
| 2 | Own expression evaluator | `EvalEnvironment` without muparserx, the muparserx submodule removed | `2026-10-27-phase2-expression-evaluator.md` |
| 3 | Replace Poco Foundation | `std::format`/`filesystem`/…, an internal platform layer (DynamicLibrary, Process+Pipe, Environment), a public log API; Poco stays only in Net/Util | `2026-10-26-phase3-replace-poco-foundation.md` |
| 4 | Network on Asio | Asio in the tree, Remote (client/server/proxy), syslog-compatible forward/listener, `Util/Network`; the network layer is ready for a future HTTP/WebSocket server (web UI) | `2026-10-27-phase4-network-asio.md` |
| 5 | PothosUtil without Poco::Util, Poco removed | own argument parser, stop on signals, `--daemon`/`--pidfile`/`--umask`, Windows service; the poco submodule removed | `2026-10-27-phase5-remove-poco.md` |
| 5a | libsimdpp → xsimd | xsimd as a copy in `third_party/xsimd`: CPU detection and `SIMDConvert` on xsimd, `PothosConfigSIMD.cmake` without `SimdppMultiarch`, the libsimdpp submodule removed; the architecture names (`sse2`, `avx2`…) stay, or change together with the toolkits | `2026-10-27-phase5a-xsimd.md` |
| 6 | Public API modernization | examine `include/Pothos/**` (std::span/optional/standard primitives), remove the warnings; CMake hygiene for the core as a subproject (user decision, 2026-10-27, before phase 7): options with the prefix `POTHOS_` in place of `ENABLE_LIBRARY*` (and the future `ENABLE_PYTHON`/`ENABLE_BLOCKS*`), tests/docs/`feature_summary`/global `CMAKE_*` only with `PROJECT_IS_TOP_LEVEL`, `PROJECT_BINARY_DIR` in place of `CMAKE_BINARY_DIR` (now `pothos.stage` in a subproject installs all of the parent project), the standard and the rpath as target properties, a CI check "the core through `add_subdirectory`" with parent options of the same names | `2026-10-27-phase6a-cmake-subproject.md`, `2026-10-27-phase6b-public-api.md` |
| 7 | Repository cleanup (user decisions, 2026-10-27) | PothosCore keeps only the core (`lib`, `include`, `apps`); all submodules and their directories are removed (the core is self-contained); `blocks`, `python`, `flow`, `widgets`, `soapy`, `plotters`, `comms`, `audio` move to separate repositories (`blocks` and `python` too, user clarifications 2026-10-27; before, the plan put them into the core tree); README.md and Changelog.txt are corrected. For a build "core + toolkits" as subprojects: after `add_subdirectory(core)`, `find_package(Pothos)` in the parent or in a sibling does not find the configuration now (`Pothos_DIR` is a normal variable in the core scope; review note of 6a) | `2026-10-27-phase7-repo-cleanup.md` |
| 8 | blocks in the core tree (user decision, 2026-10-27) | the PothosBlocks history in `blocks/`, built with the core (`POTHOS_BUILD_BLOCKS`) on the `third_party` of the core (json, xsimd, Asio through `lib/Net`), without Poco; the copier and the network blocks come with the core | `2026-10-27-phase8-blocks-in-core.md` |
| 9 | Dependencies through find_package, blocks separate again (user decision, 2026-10-28; cancels phase 8) | `third_party/` removed: nlohmann_json, Asio, xsimd through `find_package` (system packages or Conan 2, `conanfile.txt`; CMake does not know about Conan); blocks is the fork `markvin/PothosBlocks` (branch `pothos2`, its own `conanfile.txt`), only the public API of the core (`Pothos::Util::parseUri/resolveHost`, the public `XSIMDCopy.hpp`); CI: 4 jobs with Conan + `linux-system` (Arch) | `2026-10-28-phase9-conan-deps.md` |
| 10 | comms on the pothos2 core (user decisions, 2026-10-28) | the fork `markvin/PothosComms` (branch `pothos2`): built against the installed core (`find_package`, nlohmann_json, xsimd; system packages or Conan 2), without Poco; `math/SIMD` on `XSIMDCopy.hpp` and the xsimd 14 API; its own internal library `comms_filter_design` replaces spuce (`filter/design/`, a port of spuce `ad08df7`, reference data from spuce); the self-tests require PothosBlocks in the prefix of the core; CI: 4 jobs with Conan + `linux-system` (Arch) | `2026-10-28-phase10-comms.md` |
| 11 | PothosBase — the aggregate build (user decisions, 2026-10-28) | the repository `markvin/PothosBase` (branch `main`): core, blocks, comms are submodules on `pothos2`, one CMake tree (`add_subdirectory`), `POTHOS_BASE_<NAME>_DIR` puts a local clone in place of a submodule; the tests `base.stage`/`base.modules`/`base.selftests` from one prefix; `build/pothos-env.sh` for debug; in the toolkits `CMP0144` NEW + `NO_PACKAGE_ROOT_PATH`; CI: Linux (Conan) + `linux-system` (Arch) | `2026-10-28-phase11-pothosbase.md` |
| 12 | widgets on the pothos2 core (user decisions, 2026-10-29) | the fork `markvin/PothosWidgets` (branch `pothos2`): built against the installed core (CMake 3.25, `CMP0144` NEW), only Qt 6 (≥ 6.5) from the system or from the Qt installer, not from Conan; without Qt 6 the configure step stops with a clear error; the install RPATH has the Qt directory when it is not a system directory; a smoke test of 11 widgets on the `offscreen` platform (no display); CI: 4 jobs (Conan for the core + `install-qt-action`) + `linux-system` (Arch, `qt6-base`); widgets is a submodule of PothosBase | `2026-10-29-phase12-widgets.md` |
| 13 | soapy on the pothos2 core (user decisions, 2026-10-28 and 2026-10-29) | the fork `markvin/PothosSoapy` (branch `pothos2`): built against the installed core (CMake 3.25, `CMP0144` NEW), without Poco (the SoapySDR log goes to the Pothos logger); SoapySDR ≥ 0.8 from the system or from the upstream source, not from Conan; without SoapySDR the configure step stops with a clear error; with SoapySDR < 0.9 the soapy targets ignore the `#warning` about `<ciso646>` (GCC ≥ 15); the install RPATH has the SoapySDR directory when it is not a system directory; the self-tests need no hardware and use an own SoapySDR test driver (`pothos_soapy_test`, registered only during a test); a manual test `soapy.device` with a real device; CI: 4 jobs (Conan + SoapySDR `soapy-sdr-0.9.0` from the source, `actions/cache`) + `linux-system` (Arch, `soapysdr` 0.8.1); soapy is a submodule of PothosBase | `2026-10-29-phase13-soapy.md` |
| 14 | plotters on the pothos2 core (user decisions, 2026-10-28 and 2026-10-30; python is skipped for now) | the fork `markvin/PothosPlotters` (branch `pothos2`): built against the installed core (CMake 3.25, `CMP0144` NEW), only Qt 6 (≥ 6.5) and Qwt ≥ 6.3 for Qt 6 from `find_package` (`cmake/FindQwt.cmake`; a Qwt for Qt 5 is refused at the configure step), not from Conan; no `qwt6` submodule, no built-in Qwt, no spuce (an own copy of the window code of comms); the self-tests (windows, a smoke test of 9 blocks, data through the 5 plotters, `offscreen`) need PothosComms and PothosBlocks in the prefix; a core fix: `BufferChunk` converts implicitly only to pointer types (Qt 6 moc); CI: 4 jobs (Conan + `install-qt-action` + Qwt 6.3.0 built with qmake from the SourceForge archive, `actions/cache`) + `linux-system` (Arch, `qwt`); plotters is a submodule of PothosBase | `2026-10-30-phase14-plotters.md` |
| 15 | flow on the pothos2 core (user decisions, 2026-10-31) | a PR into the core first: a public log listener API (`Logger::addListener`/`removeListener`, public `LogRecord`) and the fix of the known data races (wait mode flags, reads of the plugin registry); the fork `markvin/PothosFlow` (branch `pothos2`): built against the installed core (CMake 3.25, `CMP0144` NEW), without Poco (the message window uses the listener API), only Qt 6 (≥ 6.5, Widgets and Concurrent) from the system or the Qt installer; the application sources are an internal static library; a smoke test without a display (the main window, the block cache, Help → About, the message window, a selected block, the exit) needs PothosBlocks in the prefix; CI: 4 jobs (Conan for the core + `install-qt-action`) + `linux-system` (Arch, `qt6-base`); flow is a submodule of PothosBase with the test `base.flow` | `2026-10-31-phase15-flow.md` |
| 16 | Version 2.0 (user decisions, 2026-10-31) | library 2.0.0, API `0x02000000`, ABI `2.0-0` (before: 0.8.0, `0x00070000`, `0.7-4` of the upstream development branch): `libPothos.so.2.0-0` and `lib/Pothos/modules2.0-0`, thus modules of upstream 0.7 and 0.8 builds are not loaded; a self-test checks the numbers; the toolkits take the ABI from `POTHOS_ABI_VERSION` and only need a rebuild | (a small change, described in the chat; no plan file) |
| 17 | audio on the pothos2 core (user decisions, 2026-10-31) | the fork `markvin/PothosAudio` (branch `pothos2`): miniaudio 0.11.25 in place of PortAudio, as a copy of `miniaudio.h` in `third_party/miniaudio` (only device input and output; no audio packages for the build); the block API stays; a ring buffer between the device callback and `work()`; the device converts format, channels and rate; `POTHOS_AUDIO_BACKEND` selects the backend (`null` for the tests); 16 self-tests on the null backend; CI: 4 jobs (Conan) + `linux-system` (Arch); audio is a submodule of PothosBase | `2026-10-31-phase17-audio.md` |
| 18 | the deferred minor findings (user decision, 2026-10-31: "triage them yourself and fix all of them") | about 90 minor findings of the reviews of phases 1–17: a triage (VALID 60, DECISION 18 decided by the designer, OBSOLETE 13) and the fixes in the core, PothosBlocks, PothosFlow and PothosAudio; the test environment of the core and of all toolkits unsets the POTHOS_* variables of a developer shell; UTF-8 paths on Windows everywhere (wide API, `utf8Path`/`pathUtf8`); no new features and no ABI change | `2026-10-31-phase18-minors.md` |

The order of phases 2 and 3 can change. **User decision (2026-10-26):
phase 3 occurs before phase 2.**

## Status

- Phase 1 — done (branch `pothos2`, CI green on Linux GCC/Clang,
  Windows MSVC, macOS (Intel runner)).
- Phase 3 — done and merged into `pothos2` (PR #1, merge `e749d7c5`): Poco
  stays in 10 files (only Net and Util), `cmake/poco-allowlist.txt`.
- Execution ledgers (all "Ruling:" decisions and the deferred minor notes)
  of phases 1–7: `docs/superpowers/ledgers/`.
- Phase 2 — done and merged into `pothos2` (PR #2, merge `bd369e9b`): an
  own evaluator `lib/Util/Expr/`, muparserx removed.
- Phase 4 — done and merged into `pothos2` (PR #3, merge `34ea844e`): the
  network layer `lib/Net/` on standalone Asio (`third_party/asio`), Remote
  and syslog without Poco, Poco Net is not built.
- Phase 5 — done and merged into `pothos2` (PR #4, merge `d3ecdda5`): Poco
  is fully removed; PothosUtil is on its own `pothos_app` (`lib/App/`).
- Phase 5a — done on the branch `phase5a-xsimd` (plan
  `2026-10-27-phase5a-xsimd.md`): xsimd 14.3.0 in `third_party/xsimd`, CPU
  detection and `SIMDConvert` on xsimd, an own dispatcher, the libsimdpp
  submodule removed — the core has no submodules. The PR into `pothos2`
  comes after the review.
- Phase 5a — merged into `pothos2` (PR #6, merge `f3db2df8`).
- Phase 6 (user decisions, 2026-10-27): it has two parts. **6a** is the
  CMake hygiene for the core as a subproject (plan
  `2026-10-27-phase6a-cmake-subproject.md`). **6b** is a "moderate" public
  API change: home-made constructs become standard ones, the abstractions
  stay. `std::span` comes as additional methods. The signatures that the
  blocks use do not break. The namespace `Pothos::` stays.
- Phase 6a — done on the branch `phase6-api-cmake`: the core works as a
  CMake subproject (options `POTHOS_*`, global settings only at the top
  level, the test `pothos.subproject`). Merged into `pothos2` (PR #7, merge
  `e9ebc24d`).
- Phase 6b — done on the branch `phase6b-public-api` (plan
  `2026-10-27-phase6b-public-api.md`): standard locks (`SpinLockRW`
  removed, `SpinLock` waits), `std::` aliases in `Util/Templates.hpp`,
  `using` in place of `typedef`, `BufferChunk::asSpan`, strings in
  `POTHOS_TEST_*`, the core has no warnings (`POTHOS_WARNINGS_AS_ERRORS` in
  the Linux CI), the test `pothos.api_compat`. Phase 6 is done; the next
  one is phase 7 (repository cleanup). Known out of the phase: TSan races
  in `ThreadEnvironment::_waitModeEnabled` (a plain `bool`) and in the
  tests in `TestTopology.cpp`.
- Phase 6b — merged into `pothos2` (PR #8, merge `0d8e51e1`).
- Phase 7 (user decisions, 2026-10-27: only remove the submodules; the
  copier and the network blocks stay in PothosBlocks — a clear error and
  documentation; remove `debian/`) — done on the branch
  `phase7-repo-cleanup` (plan `2026-10-27-phase7-repo-cleanup.md`): the
  repository has only the core and no submodules. `find_package(Pothos)`
  finds the core as a subproject (a redirect in
  `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`). All roadmap phases are done; the
  next work is the port of the toolkits to the pothos2 API in their
  repositories.
- Phase 8 (user decision, 2026-10-27: do not copy `third_party` into the
  toolkits, put blocks into the core tree) — done on the branch
  `phase8-blocks-in-core` (plan `2026-10-27-phase8-blocks-in-core.md`).
  Also fixed: the safe load of modules (SIGPIPE), a read/write race of
  `TcpStream` in remote calls, `XSIMDTraits` for xsimd 14, the generator
  of the SIMD dispatchers. Of the other toolkits, only Comms needs xsimd.
- Phase 5a (user decision, 2026-10-27): xsimd replaces libsimdpp (a copy
  in `third_party/xsimd`, not Highway — the toolkits already use xsimd).

- Phase 9 (user decisions, 2026-10-28) — done on the branch
  `phase9-conan-deps` (spec `2026-10-28-phase9-conan-deps-design.md`, plan
  `2026-10-28-phase9-conan-deps.md`): phase 8 is cancelled. blocks is the
  fork `markvin/PothosBlocks` (branch `pothos2`, the upstream history
  `4aef23f` and the commits of phases 8–9). `third_party/` is removed; the
  dependencies come from `find_package` (the system or Conan 2). CI is
  green: core 5/5, blocks 4/4. Next: comms with the same scheme (phase 10).

- Phase 10 (user decisions, 2026-10-28) — done in the fork
  `markvin/PothosComms` (spec `2026-10-28-phase10-comms-design.md`, plan
  `2026-10-28-phase10-comms.md`, PR #1 `phase10-comms` → `pothos2`): comms
  builds against the installed core without Poco and without spuce. Its
  own filter design library `filter/design/` gives the same taps as spuce
  `ad08df7` (reference data, the generator in `filter/design/tools/`).
  Behavior changes: `IIRFilter.setTaps` with 1 tap and `IIRDesigner` with
  an unknown type or band throw an exception. The Remez limits where spuce
  had UB give an error. CI is green: 5/5, 60 self-tests in each job. The
  core did not change.

- Phase 11 (user decisions, 2026-10-28) — done in the repository
  `markvin/PothosBase` (spec `2026-10-28-phase11-pothosbase-design.md`,
  plan `2026-10-28-phase11-pothosbase.md`, PR #1 `add-comms` → `main`):
  core, blocks and comms are submodules on the heads of `pothos2`, in one
  CMake tree. blocks (PR #1) and comms (PR #2) use `CMP0144` NEW and
  `find_package(Pothos ... NO_PACKAGE_ROOT_PATH)`: a toolkit does not find
  the core through `POTHOS_ROOT`. CI is green: 2/2 (Conan and Arch), 316
  self-tests from one prefix, 0 warnings. The core did not change. Next:
  the other toolkits one by one (widgets → soapy → python → plotters →
  flow), each goes into PothosBase after its port.

- Phase 12 (user decisions, 2026-10-29) — done in the fork
  `markvin/PothosWidgets` (spec `2026-10-29-phase12-widgets-design.md`,
  plan `2026-10-29-phase12-widgets.md`, PR #1 `phase12-widgets` →
  `pothos2`) and in PothosBase (PR #2 `add-widgets` → `main`). Only Qt 6
  (≥ 6.5); a Qt 6 fix in `indicator.cpp` (the digits are in a list, not
  found with `findChildren`). The smoke test of 11 widgets on `offscreen`
  fails on the Qt warnings about a failed call
  (`QMetaObject::invokeMethod`, `Cannot queue arguments`). The widgets CI
  is green: 5/5, 13 self-tests in each job. Qt 6.5.3 on Linux and macOS,
  Qt 6.8.3 on Windows (the Qt 6.5.3 headers do not compile with the
  current MSVC; user decision). PothosBase: 329 self-tests from one
  prefix. The core did not change. Next: soapy.

- Phase 13 (user decisions, 2026-10-28 and 2026-10-29) — done in the fork
  `markvin/PothosSoapy` (spec `2026-10-29-phase13-soapy-design.md`, plan
  `2026-10-29-phase13-soapy.md`, PR #1 `phase13-soapy` → `pothos2`, merge
  `94e1d7f`) and in PothosBase (PR #3 `add-soapy` → `main`). Poco is
  removed. CI builds SoapySDR `soapy-sdr-0.9.0` from upstream (0.9 has no
  `<ciso646>`). `linux-system` uses the Arch SoapySDR 0.8.1 with the
  `#warning` workaround. The self-tests need no hardware: an own driver
  `pothos_soapy_test` in the module `SoapySupport` (registered only for
  the time of a test; Find replies only to its own `driver=`). There are
  11 self-tests (10 in `/soapy/tests` and
  `/soapy/device_tests/test_source`). The manual check `soapy.device` on
  an Ettus USRP B210 (SoapyUHD): 950 640 samples in 1 s at 1 MSps (receive
  only). Fixed during the port: missing `#include` lines (`<exception>` in
  `SoapyBlock.hpp`, the build with libc++ on macOS). The soapy CI is
  green: 5/5. PothosBase: 340 self-tests from one prefix. The core did not
  change. Next: python.

- Phase 14 (user decisions, 2026-10-28 and 2026-10-30) — done in the core
  (PR #18 `phase14-plotters`: spec `2026-10-30-phase14-plotters-design.md`,
  plan `2026-10-30-phase14-plotters.md` and a fix of the public API, merge
  `9c8df224`), in the fork `markvin/PothosPlotters` (PR #1
  `phase14-plotters` → `pothos2`, merge `6186aa4`) and in
  PothosBase (PR #4 `add-plotters` → `main`). python is skipped for now
  (user decision, 2026-10-30). The core fix: `BufferChunk` converts
  implicitly only to pointer types. Before, Qt 6 moc found an order of
  `BufferChunk` through `int`, and each slot with a `BufferChunk` did not
  compile. Qwt comes only from `find_package`; `FindQwt.cmake` examines
  the libraries that the found Qwt uses and refuses a Qwt for Qt 5. The
  window code is a copy of the spuce port of comms, with its reference
  data. Fixed during the port: `~PothosPlotter` called its slot from
  `~QwtPlot` (an abort in a debug build). There are 17 self-tests: the
  windows, a smoke test of the 9 blocks, and data through the 5 plotters.
  A test owns its `QApplication` only for the time of the test: when the
  core unloaded the test module, `~QCoreApplication` waited for the Qt
  thread pool (the spectrogram renders with `QtConcurrent`). That wait
  hung on the loader lock on Windows and crashed on macOS. The plotters
  CI is green: 5/5. PothosBase: 358 self-tests from one prefix. Next:
  flow.

- Phase 15 (user decisions, 2026-10-30 and 2026-10-31) — done in the core
  (PR #20 `phase15-flow`: spec `2026-10-31-phase15-flow-design.md`, plan
  `2026-10-31-phase15-flow.md`, the log listener API and the race fixes,
  merge `102c15fc`), in the fork `markvin/PothosFlow` (PR #1
  `phase15-flow` → `pothos2`, merge `0369d137`) and in PothosBase (PR #5
  `add-flow` → `main`). The goal of the user: the application starts on the new core.
  The message window gets the log records from `Logger::addListener()`. A
  message that a listener logs goes only to the sink, `flush()` in a
  listener does not wait, and a change of the sink in a listener throws.
  The races: the wait mode flags are atomic, and the reads of
  `PluginRegistry` and of the module paths use `find` (before,
  `operator[]` added entries under a shared lock). A local TSan run of 59
  self-tests went from 21 reports to 0. The smoke test found two crashes
  that upstream has too: a null clipboard on the `offscreen` platform,
  and a slot of the destroyed graph view at exit with a selected block
  (the manual check of the user found it first). Fixed in CI:
  `std::shared_ptr::unique()` is removed in C++20 (libc++ and MSVC), and
  a message box on macOS has no window title (the smoke test finds the
  boxes by their text). The flow CI is green: 5/5. PothosBase: 370
  self-tests from one prefix, and `base.flow`. All toolkits of the
  roadmap are ported, except python (skipped for now).

- Phase 16 (user decisions, 2026-10-31) — the version of pothos2 is 2.0.
  Upstream has no 0.8 release (its last release is 0.7.1 with ABI `0.7`),
  but its development branch is "0.8.0 (pending)" with ABI `0.7-4`, the
  same numbers as pothos2 before. Thus an upstream build and a pothos2
  build put their modules into the same directory. The 0.x numbers belong
  to upstream; 2.0 matches the name pothos2 and cannot collide with a
  future upstream 1.0. This closes the deferred ABI note of phase 6b.

- Phase 17 (user decisions, 2026-10-31) — done in the fork
  `markvin/PothosAudio` (PR #1 `phase17-audio` → `pothos2`, merge
  `2f323611`) and in PothosBase (PR #7 `add-audio` → `main`); the core
  did not change. The use case of the user: a microphone or a line input
  → the speakers in PothosFlow (the manual check passed on Linux with
  PipeWire). miniaudio replaces PortAudio; `miniaudio.h` is a copy in the
  toolkit (a user decision: an exception to the `find_package` rule). The
  final review found two defects, fixed with tests: the sink stopped for
  good after an underflow with a backoff time, and a device that stops
  gave no error. CI found two more on the slow macOS and Windows runners:
  the sink needed a pre-fill of 2 periods at the start, and a test of a
  source → sink graph must allow xrun reports (the two devices have
  independent clocks). PothosBase: the audio self-tests run with
  `POTHOS_AUDIO_BACKEND=null`.

- Phase 18 (user decision, 2026-10-31) — done: core PR #24 (merge
  `77f4af67`), PothosBlocks #2, PothosFlow #2, PothosAudio #2,
  PothosComms #4, PothosWidgets #2, PothosSoapy #2, PothosPlotters #2 and
  PothosBase #8. The spec has a triage appendix
  (`2026-10-31-phase18-minors-triage.md`) with the evidence for each
  finding. Found on the way: a host name in `POTHOS_SYSLOG_ADDR` crashed
  the library load (static init order of the lookup cache). The Windows
  CI found two UTF-8 defects that Linux cannot show (a conf file path
  through the ANSI code page) and two test defects. Deferred: six minor
  findings of the final review (the next small PR before the release
  2.0.0).

- Release 2.0.0 (2026-10-31, user decision): the six deferred findings
  are fixed (core PR #26). The Changelogs get the date, and the merge
  commits get the tags `pothos-2.0.0`, `pothos-blocks-0.6.0`,
  `pothos-comms-0.4.0`, `pothos-widgets-0.6.0`, `pothos-soapy-0.6.0`,
  `pothos-plotters-0.5.0`, `pothos-flow-0.8.0`, `pothos-audio-0.4.0` and
  `pothos-base-0.1.0` (PothosBase points to the tagged commits). Next:
  python as its own phase.

## Decisions for the next phases

User decisions (2026-10-28), made before these phases started:

- plotters (spuce `design_window` in Periodogram and Spectrogram): a small
  own copy of the window code in plotters. The spuce replacement in comms
  stays an internal library of comms and is not installed. Done in phase
  14.
- soapy: ConanCenter does not have SoapySDR. CI builds SoapySDR from the
  upstream source `pothosware/SoapySDR` (a pinned tag or commit) in a
  separate step before the toolkit build. It installs SoapySDR into the
  dependency prefix (`actions/cache` keyed on the version). The other
  dependencies come from Conan. The job `linux-system` uses
  `extra/soapysdr`. The CMake code of the toolkit uses only
  `find_package(SoapySDR CONFIG)`. A recipe for ConanCenter can come
  later; it is optional. Done in phase 13 (the tag `soapy-sdr-0.9.0`, user
  decision 2026-10-29).
- PothosBase — done as phase 11 (refer to the table and the status).
- PothosBase, the next toolkits: add them after their port, one at a time
  (widgets is phase 12, soapy is phase 13, plotters is phase 14; python
  is skipped for now (user decision 2026-10-30); flow is phase 15). The
  checks `base.modules` and `base.selftests` expect the module directory
  `/<toolkit>/` and the tests in `/<toolkit>/tests/`. flow has no
  self-tests: `base.selftests` does not get it, and `base.flow` runs its
  smoke test (phase 15). python (`proxy/environment`) still needs a
  setting for the toolkit (a deferred review note of phase 11).

## Notes for the phase 2 plan (collected during the analysis)

- `Topology::make()` (`lib/Framework/TopologyMakeJSON.cpp:16-36`) and
  `ProxyBlockEval` (`lib/Util/Builtin/BlockEval.cpp:85`) calculate
  `arg.dump()` for all JSON values that are not strings. Thus the syntax
  must be a superset of JSON: `true`/`false`/`null` in lower case, the
  numbers `-3`, `1e-05`, arrays, objects with string keys.
- A string argument that does not evaluate becomes a literal string
  (`TopologyMakeJSON.cpp:21-31`, documented in `Topology.hpp:~95`). Thus
  an unknown identifier (`int32`, `hello`) **must** throw an exception, and
  must not give a value.
- An example from the `Topology.hpp` documentation: globals
  `{"name":"baz","value":"-fiz"}` — a unary minus on a constant.
- The current hacks that go away: `splitExpr` for lists/dictionaries
  (`EvalEnvironmentListParsers.cpp`), the encoding of a dictionary into a
  matrix with the label `__map__`, the temporary constants `__tmp__N`,
  `MupOctalValReader`.
