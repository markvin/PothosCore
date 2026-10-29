# Phase 13: PothosSoapy on the pothos2 core

Date: 2026-10-29. Status: design. Work happens in a new fork
`markvin/PothosSoapy`, then in `markvin/PothosBase`. The core changes
only in its docs.

## Intent

PothosSoapy gives the SDR source and sink blocks of Pothos. They use
SoapySDR to control SDR hardware. The roadmap ports the toolkits one at
a time, in the order widgets → soapy → python → plotters → flow. Soapy
is the first toolkit with a dependency that is not on ConanCenter and
that CI must build from source.

When this phase is complete, soapy builds against an installed pothos2
core and SoapySDR, has no compiler warnings, has CI with self-tests
that move data through the source and sink blocks without hardware, and
is a submodule of PothosBase.

## Decisions of the user

1. SoapySDR is not on ConanCenter. CI builds SoapySDR from the upstream
   source `github.com/pothosware/SoapySDR` at a pinned tag, in a
   separate step before the toolkit. CI installs it into a prefix for
   the dependencies and keeps it in `actions/cache` with the version as
   the key. The other dependencies come from Conan. The job
   `linux-system` uses the Arch package `extra/soapysdr` (0.8.1). The
   CMake of the toolkit uses only `find_package(SoapySDR CONFIG)`. A
   ConanCenter recipe is optional and comes later. (Decision of
   2026-10-28.)
2. CI uses only upstream `pothosware/SoapySDR`.
3. CI tests the blocks with a test driver of our own. The driver is a
   part of the test code of soapy. It is not an external module.
   (SoapyLoopback of JuliaTelecom is a sample only: it gives no data and
   has no transmit side.)
4. A manual check on real hardware before the PR: an RTL-SDR receiver
   with `/soapy/source`. RTL-SDR has no transmitter, so the sink gets no
   hardware check. SoapySpyServer is an optional manual check with a
   real signal over the network. It is not in CI.
5. The `<ciso646>` warning (see "Current state"): CI pins SoapySDR
   `soapy-sdr-0.9.0`. The minimum version in CMake is 0.8. The job
   `linux-system` keeps the Arch package 0.8.1. Only when SoapySDR is
   older than 0.9, the toolkit adds `-Wno-cpp` (GCC) or `-Wno-#warnings`
   (Clang) to its own targets, with a comment that refers to SoapySDR
   PR #463. With SoapySDR 0.9 or newer, this workaround is off.

## Current state (recon on upstream `142c4a8`)

- 14 source files, approximately 3200 lines, two modules, both with
  `DESTINATION soapy`:
  - `SoapySupport` (repository root): `SoapyBlock.cpp/.hpp` (the
    common base of the source and the sink, approximately 1100 lines),
    `SoapySource.cpp`, `SoapySink.cpp`, `BlockThread.cpp`,
    `DemoController.cpp`, `EnumerateCache.cpp`, `SoapyInfo.cpp`,
    `Logger.cpp`. `SoapyBlockDesc.hpp` is a template:
    `configure_file` makes `SoapySourceDesc.hpp` and
    `SoapySinkDesc.hpp` (the `|PothosDoc` text of the two blocks).
  - `SoapyUtilityBlocks` (`utility/`): `ChannelAligner.cpp`,
    `Converter.cpp`, `RandomDropper.cpp`, `TxBurstTimer.cpp`.
- Registered paths: `/soapy/source`, `/soapy/sink` (aliases
  `/sdr/source`, `/sdr/sink`), `/soapy/demo_controller`,
  `/soapy/channel_aligner`, `/soapy/converter`, `/soapy/random_dropper`,
  `/soapy/tx_burst_timer` (each with an alias), and the plugin call
  `/devices/soapy/info`.
- No self-tests at all.
- Poco in 5 files: `Poco::Logger` and its macros (`SoapyBlock.hpp`,
  `SoapyBlock.cpp`, `BlockThread.cpp`, `DemoController.cpp`,
  `Logger.cpp`) and `Poco::format` (`SoapyBlock.cpp`,
  `utility/Converter.cpp`). `Logger.cpp` sends the SoapySDR log to the
  Poco logger; the SoapySDR levels have the same values as the Pothos
  `LogLevel` (Fatal = 1 ... Trace = 8). The level `SOAPY_SDR_SSI` goes
  directly to `std::cerr`.
- JSON: `#include <json.hpp>` with `find_path` (old style).
- CMake: `cmake_minimum_required(VERSION 3.1.0)` (CMake 4 refuses it),
  `cmake_dependent_option(ENABLE_SOAPY ...)` and `ENABLE_SOAPY_UTILITY`
  (a missing SoapySDR or json turns the modules off without an error),
  `include_directories`, `find_package(SoapySDR "0.6.0" CONFIG)`.
  `utility/` links `${SoapySDR_LIBRARIES}`, the root links `SoapySDR`.
- Spike (2026-10-29, scratchpad, the Poco headers replaced by stubs):
  a syntax check of all 12 `.cpp` files with GCC 16, `-std=c++23 -Wall
  -Wextra`, against the stage of PothosBase and the Arch SoapySDR 0.8.1.
  The pothos2 API gave no errors. Other errors: `DemoController.cpp`
  uses `std::memset` without `<cstring>`; `BlockThread.cpp` uses the
  Poco macro `poco_error_f2`.
- `SoapySDR/Config.hpp` of SoapySDR 0.8.1 includes `<ciso646>`. Since
  GCC 15, libstdc++ gives a `#warning` for this header in C++20 and
  newer. Thus each file that includes a SoapySDR header gives a
  warning, and an error with `-Werror` (GCC 16 and Clang with libstdc++;
  checked). SoapySDR fixed this in PR #463 and #473; the fix is in the
  tag `soapy-sdr-0.9.0` (2025-12). Arch has 0.8.1.
- `SoapySDR::Registry` has a destructor that removes its entry (checked
  in the source of 0.8.1). Thus a `Registry` object in a test function
  registers a driver only while the test runs.
- `debian/` exists (packages for Pothos 0.7).
- The Arch repositories have no SoapyRTLSDR module. It is in the AUR.

## Goals

- The fork `markvin/PothosSoapy`, branch `pothos2`, builds against an
  installed pothos2 core and SoapySDR 0.8 or newer, with CMake 3.25 or
  newer (also CMake 4).
- No Poco.
- No compiler warnings with `-Wall -Wextra` (GCC and Clang).
  `SOAPY_WARNINGS_AS_ERRORS=ON` turns them into errors.
- Self-tests move data from `/soapy/sink` through the test driver to
  `/soapy/source` and check it, check the settings of the blocks, and
  check the 4 utility blocks. They pass on Linux, macOS and Windows in
  CI.
- PothosBase builds soapy as a submodule. Its tests load the soapy
  modules and run the soapy self-tests from the stage prefix.
- The core docs record phase 13.

## Non-goals

- No new blocks and no changes of the block API (block paths, calls,
  signals, slots, `|PothosDoc` comments), except a fix of a bug.
- No SoapySDR hardware in CI. No SpyServer in CI.
- No packages (Debian, Conan package, ConanCenter recipe for SoapySDR,
  installer).
- No change of the public API of the core.
- No port of other SoapySDR modules (SoapyRTLSDR, SoapyRemote and so
  on).

## Anti-goals

These results are failures, also when the build is correct:

- The configure step continues without SoapySDR and gives a build
  without the soapy modules. A missing SoapySDR must stop the configure
  step with a clear message.
- A CMake file refers to Conan, or the configure or build step downloads
  files.
- CI uses a SoapySDR source other than upstream `pothosware/SoapySDR`.
- The test driver is visible to the user: it is in the device list of
  SoapySDR (`SoapySDR::Device::enumerate()`, `/devices/soapy/info`)
  outside of a running test, or it is installed as a SoapySDR module.
- A self-test passes because no data moved (for example the source got
  only timeouts) or because it skips a block without a message.
- The warning workaround hides warnings of our own code, or stays on
  with SoapySDR 0.9 or newer.
- soapy builds in PothosBase but not standalone against an installed
  core (or the opposite).
- The installed modules load only from the build tree. They must load
  from the install prefix (the SoapySDR library must be found at run
  time).

## Repository and history

1. Fork `pothosware/PothosSoapy` to `markvin/PothosSoapy`. Create the
   fork only after the user agrees at that step of the plan.
2. Make the branch `pothos2` from upstream `master` (`142c4a8`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosSoapy`. Its `origin` is
   upstream now; after the fork, `origin` is the fork and `upstream` is
   `pothosware/PothosSoapy` (as in the core).
4. The work goes into the work branch `phase13-soapy` and a PR into
   `pothos2` of the fork (the same as blocks, comms and widgets). The
   user approves the fork, the first push and each merge.

## Build (the same method as PothosComms and PothosWidgets)

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosSoapy CXX)`,
  C++23 (`CXX_EXTENSIONS OFF`).
- `cmake_policy(SET CMP0144 NEW)` and
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with the
  comment of comms that tells why.
- `find_package(SoapySDR 0.8 CONFIG)`. If SoapySDR is missing, stop with
  one clear message: install `soapysdr` (or the package of the system),
  or give `CMAKE_PREFIX_PATH` or `SoapySDR_DIR`. Link the imported
  target `SoapySDR` in both modules (not `${SoapySDR_LIBRARIES}`).
- `find_package(nlohmann_json 3.11 CONFIG)` and an interface target
  `soapy_json` (as `comms_json` in comms); `#include
  <nlohmann/json.hpp>`. soapy gets its own `conanfile.txt` with
  `nlohmann_json` only (as comms). SoapySDR is not in it (decision 1).
- One clear configure error lists all missing dependencies (SoapySDR,
  nlohmann_json), as the error of comms.
- Remove `ENABLE_SOAPY`, `ENABLE_SOAPY_UTILITY` and
  `cmake_dependent_option`: both modules always build.
- Keep the `configure_file` of `SoapyBlockDesc.hpp` for the source and
  the sink.
- The default install prefix is the prefix of the core when the user
  gives no `CMAKE_INSTALL_PREFIX` (as in comms).
- Options: `SOAPY_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`; as
  `COMMS_BUILD_TESTS`, it only registers the CTest tests),
  `SOAPY_WARNINGS_AS_ERRORS` (default OFF), `SOAPY_TEST_DEVICE`
  (default empty, see "Tests").
- Warnings: `-Wall -Wextra` for GCC and Clang, `-Werror` with
  `SOAPY_WARNINGS_AS_ERRORS` (a function as `comms_warnings`). SoapySDR
  headers come as system includes from the imported target. Fix the
  warnings in the soapy code; do not turn them off. The only exception
  is the `<ciso646>` workaround of decision 5.
- The installed modules must find the SoapySDR library at run time. On
  Linux and macOS use an install RPATH to the SoapySDR library directory
  when it is not a system directory (the same method as Qt in widgets).
  On Windows the SoapySDR `bin` directory is on `PATH` (CI and the
  user). Decision of the builder: the exact method.
- Keep the layout of the sources (root and `utility/`). Tests go into a
  new directory `tests/` (see "Tests").
- Delete `debian/`. Update `README.md` (dependencies: the pothos2 core,
  SoapySDR 0.8 or newer, nlohmann_json; build; run the tests; the
  manual check with a device) and `Changelog.txt` (a "Release 0.6.0
  (pending)" entry: the port to the pothos2 core, no Poco, self-tests
  with a test driver).

## Code changes

- Logging: replace `Poco::Logger` with `POTHOS_LOG_*` from
  `<Pothos/System/Logger.hpp>`. Keep the log sources of upstream
  (`"SoapySDR"`, `"SoapyBlock"`, `"DemoController"`, and the source of
  `BlockThread.cpp`). Remove the `_logger` member of `SoapyBlock`.
- `Logger.cpp`: the SoapySDR log handler sends each message to
  `Pothos::System::Logger::log` with the same level value. Replace the
  `static_assert` checks of the Poco values with checks of the Pothos
  `LogLevel` values (Fatal, Information, Trace). `SOAPY_SDR_SSI` stays
  on `std::cerr`.
- `setLogLevel` of the blocks: no change (it sets the SoapySDR level, with
  the SoapySDR level names).
- Replace `Poco::format` with `std::format`. Keep the text of each
  message.
- Add the missing standard includes (for example `<cstring>` in
  `DemoController.cpp`).
- Fix all warnings of GCC and Clang with `-Wall -Wextra` in the soapy
  code.
- Other changes only when the build, a warning or a test needs them.
  If a test finds a bug in a block, fix it and record it in the
  Changelog.

## Tests

### Runner (the method of comms)

1. CTest test `soapy.stage` installs soapy into `build/stage`.
2. CTest test `soapy.selftests` runs each `/soapy/tests/...` test with
   the `PothosUtil` of the installed core. It uses a copy of
   `cmake/PothosRunSelfTests.cmake` and `cmake/PothosTestEnv.cmake` of
   comms, with the check for PothosBlocks (the tests use
   `/blocks/feeder_source` and `/blocks/collector_sink`).

### Test driver

- New code in `tests/` (for example `tests/TestDevice.hpp/.cpp`): a
  `SoapySDR::Device` subclass. It is always compiled into the
  `SoapySupport` module, with the self-tests (as comms: the self-tests
  are always in the module, and PothosBase runs them with
  `SOAPY_BUILD_TESTS` OFF). The driver stays inactive until a test
  registers it. It is never a SoapySDR module and is never in the
  SoapySDR module directory.
- The driver name has a prefix that tells it is for tests, for example
  `pothos_soapy_test`.
- Registration: a `SoapySDR::Registry` object that the test makes (for
  example a helper object at the start of each test). When the test
  ends, the destructor removes the entry. No static registration.
- Find: return the device only when the args have
  `driver=<test driver name>`.
- Streams: RX and TX, 1 or 2 channels (for `ChannelAligner`), the
  formats `CF32`, `CS16` and `CS8` (*builder*: at least the formats the
  tests use). `writeStream` puts the elements into a FIFO for each
  channel; `readStream` takes them out. With an empty FIFO, `readStream`
  waits up to the timeout and then returns `SOAPY_SDR_TIMEOUT`. Time:
  a tick counter at the sample rate; `readStream` gives
  `SOAPY_SDR_HAS_TIME`. `activateStream` with burst flags and
  `writeStream` with `SOAPY_SDR_END_BURST` work, as far as the tests
  use them.
- Settings: the device keeps the values that the blocks set and gives
  them back: frequency, gain (one or more named elements), gain mode,
  sample rate, bandwidth, antenna, DC offset mode, frequency correction,
  clock source, time source, hardware time, sensors, settings
  (`writeSetting`/`readSetting`).
- Samples: SoapyLoopback (MIT) and SoapyRTLSDR show the shape of a
  driver. Write the driver new; do not copy their code.

### Self-tests

Each test is a `POTHOS_TEST_BLOCK("/soapy/tests", ...)`. A test that
needs PothosBlocks fails with a clear message when PothosBlocks is
missing (the runner checks it first, as in comms).

1. Data loopback: topology feeder → `/soapy/sink` → test driver →
   `/soapy/source` → collector. Known samples go in; the test checks
   that the same samples come out, in the same order, for each
   format that the test uses. The test fails when the source gives
   only timeouts.
2. Settings: make `/soapy/source` and `/soapy/sink` with the test
   driver, call their setters (frequency, gain, sample rate, antenna,
   bandwidth and so on), and check the values in the driver and from the
   getters of the blocks.
3. Registration: while the driver is registered, the "Factories" field
   of `/devices/soapy/info` (`Registry::listFindFunctions()`) has its
   name; after the test, it does not. `SoapySDR::Device::enumerate()`
   without args never gives the driver (Find answers only to
   `driver=<test driver name>`), so the background enumerator of the
   blocks (`EnumerateCache.cpp`) does not see it.
4. Utility blocks, with synthetic data and no device:
   `/soapy/converter`, `/soapy/channel_aligner`,
   `/soapy/random_dropper`, `/soapy/tx_burst_timer`. Each test checks
   the main behavior of the block (decision of the builder: the exact
   checks, from the code and the `|PothosDoc` text).
5. `/soapy/demo_controller`: only make it and delete it (it is a demo).

### Manual check with hardware

- `SOAPY_TEST_DEVICE` (CMake option, default empty). When it has device
  args (for example `driver=rtlsdr`), CTest gets the test
  `soapy.device`: it runs `/soapy/source` with these args for about one
  second and prints the number of samples and the rate. It fails when
  no samples come. Without the option, the test does not exist.
- Before the PR the user runs this test with an RTL-SDR (the module
  SoapyRTLSDR from the AUR). The result goes into the PR text.
- SpyServer: the same test with `driver=spyserver,...` is an optional
  check with a real signal. It is not a requirement.

## CI of soapy

- The same structure as the CI of comms: linux-gcc (Debug, `-Werror`),
  linux-clang (Release, `-Werror`), macos (Intel runner,
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC Release).
  1. Check out soapy, `markvin/PothosCore@pothos2` and
     `markvin/PothosBlocks@pothos2`.
  2. Install the dependencies of the core with Conan.
  3. SoapySDR: restore it from `actions/cache` (key: OS, compiler and the
     SoapySDR tag). On a cache miss, check out `pothosware/SoapySDR` at
     `soapy-sdr-0.9.0`, build it without the language bindings and the
     apps that the tests do not need, and install it into a prefix for
     the dependencies.
  4. Build and install the core, then PothosBlocks.
  5. Build soapy with `CMAKE_PREFIX_PATH` to the SoapySDR prefix and run
     `ctest -V`.
- Job `linux-system` in an `archlinux` container (`--init`): system
  packages with `soapysdr`, no Conan. This job tests the `<ciso646>`
  workaround with GCC 16 and `-Werror`.
- Keep the guard against downloads in the CMake files.
- The SoapySDR tag is in one place of the workflow (an `env` value).

## PothosBase

After `pothos2` of the fork has soapy:

- Add the submodule `soapy` (HTTPS URL, `branch = pothos2`), the
  options `POTHOS_BASE_WITH_SOAPY` and `POTHOS_BASE_SOAPY_DIR`, and
  `SOAPY_WARNINGS_AS_ERRORS` from `POTHOS_BASE_WARNINGS_AS_ERRORS`.
- The checks `base.modules` and `base.selftests` include soapy (module
  directory `/soapy/`, tests `/soapy/tests/`).
- CI of PothosBase: the SoapySDR step of the soapy CI (build from
  source, cache) in the job with Conan, `soapysdr` in `linux-system`.
  The `conanfile.txt` of PothosBase does not change.
- README of PothosBase: SoapySDR is a dependency.

## Changes in the core

- Branch `phase13-soapy` from `pothos2` (`e4591193`), PR into `pothos2`
  of `markvin/PothosCore`.
- Roadmap: phase 13 row and status.
- `CLAUDE.md`: a short note about the soapy fork, its branch, its local
  clone and the SoapySDR version in CI.
- No code changes are expected. If soapy needs a change of the public
  API, stop and ask the user.

## Decisions of the designer (the user must check them)

- Fork `markvin/PothosSoapy`, branch `pothos2` from upstream `142c4a8`,
  local clone `../PothosSoapy`, work branch
  `phase13-soapy`.
- CMake as in comms and widgets: CMake 3.25, `CMP0144` NEW,
  `NO_PACKAGE_ROOT_PATH`, `-Wall -Wextra`, the options `SOAPY_*`, a
  clear error without SoapySDR, both modules always build.
- The SoapySDR log goes to the Pothos logger with the same level values.
  The log sources and the message texts stay as in upstream.
- The test driver is always compiled into `SoapySupport` (as the
  self-tests of comms), is registered only inside a test, answers Find
  only to its own `driver=` value, and has a test name prefix.
  `SOAPY_BUILD_TESTS` only registers the CTest tests.
- The registration test checks the "Factories" field of
  `/devices/soapy/info`, not the device list.
- The self-tests need PothosBlocks in the prefix of the core (as comms).
- The hardware check is the CTest test `soapy.device`, only with
  `-DSOAPY_TEST_DEVICE=...`.
- `/soapy/demo_controller` gets only a make-and-delete test.
- `POTHOS_BASE_WITH_SOAPY` is ON by default, as the other toolkits. Thus
  SoapySDR becomes a dependency of PothosBase. A missing SoapySDR stops
  the configure step (`-DPOTHOS_BASE_WITH_SOAPY=OFF` builds without it).
- `debian/` is deleted. Changelog entry "Release 0.6.0 (pending)".
- After the merge in the fork: soapy into PothosBase, then the core
  docs.

## Recommended order of work

1. Fork and branch. New top-level CMake (with the `<ciso646>`
   workaround), the test runner, Poco removed, warnings fixed. The
   build is clean locally with Arch SoapySDR 0.8.1 and GCC 16 with
   `-Werror`.
2. The test driver and the self-tests, locally.
3. A local build against SoapySDR 0.9.0 from upstream source (check of
   the CI method and of the workaround that is off).
4. README, Changelog, delete `debian/`.
5. CI (4 jobs with Conan and SoapySDR from source, and `linux-system`).
   PR into `pothos2` of the fork. The manual RTL-SDR check before the
   PR.
6. PothosBase: the soapy submodule, checks, CI.
7. Core docs: roadmap and `CLAUDE.md`.
