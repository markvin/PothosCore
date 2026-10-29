# Phase 10: PothosComms on the pothos2 core

Date: 2026-10-28. Base branch of the core: `pothos2` (after PR #11, merge
`cc87805a`). Work happens in the comms repository. The core changes only
in its docs (roadmap, ledger).

## Intent

PothosComms (DSP and communications blocks) must build and pass its
self-tests against the installed pothos2 core. Use the same method as
PothosBlocks in phase 9: a separate repository, an installed core,
dependencies through `find_package`, an optional Conan 2 install, CI on
4 platforms.

Comms needs the filter design library spuce. spuce is not maintained
(last commit 2018). It is not on ConanCenter or in Arch. Its CMake needs
CMake 2.8 and does not work on CMake 4. Thus comms gets its own filter
design code. This code replaces spuce and keeps the behavior of the blocks.

## Decisions of the user

1. Port comms with the method of blocks (phase 9).
2. Replace spuce with own code in comms. Do not fork spuce, do not use
   liquid-dsp, iir1 or KFR. Port only the parts of spuce that comms uses.
   spuce has the license BSL-1.0, the same as Pothos.
3. The playback of the design (fork, build, Poco, spuce replacement,
   reference data, CI, non-goals) is correct.

## Current state (recon on upstream `f8a5fd0`)

The recon built upstream comms against the installed pothos2 core
(GCC, Arch, system json and xsimd, spuce not found):

- Most of the code compiles without changes.
- 5 files fail on Poco:
  - `utility/WaveTrigger.cpp`, `filter/FIRDesigner.cpp`,
    `window/WindowDesigner.cpp`: `Poco::Logger`, `poco_warning_f*`;
  - `digital/ByteOrder.hpp`, `digital/ByteOrder.cpp`,
    `digital/TestByteOrder.cpp`: `Poco::ByteOrder`, `Poco::Platform`,
    `Poco::Int*`/`UInt*`;
  - `digital/Bitwise.cpp`: `Poco::format`;
  - `digital/TestBitwise.cpp`: `Poco::RandomBuf`;
  - `math/TestExp.cpp`, `math/TestModF.cpp`: `Poco::Random`.
- `math/SIMD/` (18 sources, one copy per architecture) fails:
  - the sources include `<xsimd/xsimd.hpp>` directly, not
    `<Pothos/Util/XSIMDCopy.hpp>`;
  - `CommsMathSIMD` links `Pothos`, not `Pothos::Pothos`, so the include
    path of the core is missing;
  - AVX-512 copies fail with "avx512dq requires avx512cd". `XSIMDCopy.hpp`
    has the workaround for this.
- `filter/` and `window/` are skipped because spuce is not found.
- 7 files include `<json.hpp>`. Change them to `<nlohmann/json.hpp>`.
- The top-level `CMakeLists.txt` needs CMake 3.1, uses `find_path` for
  `json.hpp`, the optional `PothosConfigSIMD`, `add_definitions` and
  `include_directories`. The option `POTHOS_XSIMD` selects the SIMD code.
- Self-tests: 53 `POTHOS_TEST_BLOCK` entries, all under `/comms/tests`.
- `debian/` contains the old Debian packaging.

### What comms uses from spuce

| Block | spuce API | Types |
|---|---|---|
| `filter/FIRDesigner.cpp` | `design_fir`, `design_complex_fir` | `SINC`, `RAISED_COSINE`, `ROOT_RAISED_COSINE`, `MAXFLAT`, `GAUSSIAN`, `REMEZ`; bands `LOW_PASS`, `HIGH_PASS`, `BAND_PASS`, `BAND_STOP`, `COMPLEX_BAND_PASS`, `COMPLEX_BAND_STOP` |
| `filter/FIRDesigner.cpp` | `remez_estimate_num_taps`, `remez_estimate_atten`, `remez_estimate_bw`, `remez_estimate_weight` | Remez parameter estimates and warnings |
| `filter/FIRDesigner.cpp`, `window/WindowDesigner.cpp` | `design_window` | `rectangular`, `hann`, `hamming`, `blackman`, `bartlett`, `flattop`, `kaiser`, `chebyshev` |
| `filter/IIRDesigner.cpp` | `design_iir`, `iir_coeff` (`get_a`, `get_b`) | `butterworth`, `chebyshev`, `chebyshev2`, `elliptic`; bands `LOW_PASS`, `HIGH_PASS`, `BAND_PASS`, `BAND_STOP` |
| `filter/IIRFilter.cpp` | `iir_df<Type, double>` (`set_taps`, `reset`, `clock`, `print`), which uses `fir<Numeric, Coeff>` from `fir.h` | run-time direct form IIR filter, not a design function |

The reference version of spuce is commit `ad08df7` of the fork
https://github.com/hayguen/spuce.git. This is the commit of the spuce
submodule that upstream Pothos used to build comms (refer to `CLAUDE.md`).
The fork `master` (`cbbc0fe`) has 26 newer commits (many `int` to
`size_t` changes). Do not use it as the reference. The sources are in
`spuce/filters/`:
`design_fir`, `design_window`, `window`, `design_iir`, `iir_coeff`,
`butterworth_iir`, `chebyshev_iir`, `chebyshev2_iir`, `elliptic_iir`,
`find_roots`, `gaussian_fir`, `butterworth_fir`, `raised_cosine_imp`,
`root_raised_cosine_imp`, `remez_estimate`, `remez_fir`,
`create_remez_lpfir`, `sinc_fir`, `sinc_helper`, `fir_inv_dft`,
`transform_fir`, `iir_df.h`, `fir.h`, and their headers. The design part
is approximately 2300 lines. The
builder uses only what the call graph from the API above needs.

## Goals

- The fork `markvin/PothosComms`, branch `pothos2`, builds against an
  installed pothos2 core with its own `conanfile.txt` or with system
  packages.
- All modules build, including `filter` and `window`. Nothing needs spuce.
- No Poco in comms.
- All comms self-tests pass in CI on linux-gcc, linux-clang, macos,
  windows and linux-system (Arch).
- The new filter design code gives the same taps as spuce, within a
  tolerance. Reference data made from spuce proves this.

## Non-goals

- Do not add blocks, and do not remove blocks.
- Do not change the parameters, the defaults, the options or the
  behavior of the blocks.
- Do not optimize the DSP code. Do not change the SIMD dispatch design.
- Do not port the other toolkits.
- Do not make a Conan package of comms or of the filter design code.
- Do not change the public API of the core. If comms needs a core change,
  stop and ask the user.

## Anti-goals

These results are failures, also when the build is correct:

- comms contains a copy of spuce as a third party directory, or a
  dependency on spuce in any form (package, submodule, download).
- A CMake file of comms refers to Conan.
- The configure step or the build step downloads files.
- comms uses internal headers or internal targets of the core. Only the
  installed public API is permitted.
- A designer block gives different taps from upstream comms with spuce
  (outside the tolerance), and no test catches it.
- The reference data is generated during the build or in CI. It is
  generated one time and committed.

## Repository and history

1. Fork `pothosware/PothosComms` to `markvin/PothosComms`. Create the fork
   only after the user agrees at that step of the plan.
2. Make the branch `pothos2` from upstream `master` (`f8a5fd0`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosComms` (already made from
   upstream, no fork remote yet).
4. The user approves these steps before they occur: the fork, the first
   push, and pushes to `master` (none are planned).

## Build (the same as PothosBlocks)

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosComms C CXX)`,
  C++23 (`CXX_EXTENSIONS OFF`). `fft/kiss_fft.c` is C.
- `find_package(Pothos CONFIG REQUIRED)` and `include(PothosConfigSIMD)`.
- `find_package(nlohmann_json 3.11 CONFIG)` and
  `find_package(xsimd 14.2 CONFIG)`. Both are required. If one is
  missing, stop with one clear message that lists all missing packages
  (as in blocks). Asio is not needed.
- Internal wrapper targets `comms_json` and `comms_xsimd` (names as in
  blocks: `blocks_json`, `blocks_xsimd`).
- xsimd is required. Remove the option `POTHOS_XSIMD` and the code paths
  without xsimd (`#ifdef POTHOS_XSIMD`). The SIMD path is always on. The
  run-time dispatcher still selects the scalar fallback when the CPU has
  no supported SIMD architecture.
- A helper `pothos_comms_module()` like `pothos_blocks_module()`:
  `POTHOS_MODULE_UTIL(... DESTINATION comms ENABLE_DOCS)`, the include
  path for `common/`, `-Werror` when `COMMS_WARNINGS_AS_ERRORS` is on, and
  a list of self-test names for CTest.
- Options: `COMMS_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `COMMS_WARNINGS_AS_ERRORS` (default OFF). Remove `ENABLE_COMMS*` and the
  `cmake_dependent_option` per module: all modules always build.
- Use target properties only. No `add_definitions`, no
  `include_directories`, no global `CMAKE_*` settings except the default
  install prefix (as in blocks: the prefix of the core when the user gives
  no `CMAKE_INSTALL_PREFIX`).
- `math/SIMD`: every SIMD source includes `<Pothos/Util/XSIMDCopy.hpp>`
  first, as in blocks `stream/SIMD/`. `CommsMathSIMD` links
  `Pothos::Pothos` and `comms_xsimd`. It stays a static PIC library if MSVC
  still needs this (the comment in its CMake file tells why).
- `math`: keep the `exp10`/`exp10f` checks with `CheckSymbolExists`, or
  replace them with `std::pow(10, x)` if this gives the same results in the
  tests (decision of the builder).
- `fft`: keep the `FIXED_POINT=16` and `alloca` settings as target
  definitions.
- `conanfile.txt`: `nlohmann_json/3.12.0`, `xsimd/14.2.0`;
  `CMakeDeps`, `CMakeToolchain`.
- Delete `debian/`. Update `README.md` (build with system packages, build
  with Conan, run the tests) and `Changelog.txt`.

## Poco replacements

| Poco | Replacement |
|---|---|
| `Poco::Logger`, `poco_warning_f*` | `POTHOS_LOG_WARNING("<source>", "fmt {}", ...)` from `<Pothos/System/Logger.hpp>`. Keep the logger names as sources (`FIRDesigner`, `FIRDesigner.Remez`, `WaveTrigger`). |
| `Poco::ByteOrder::flipBytes`, the manual `byteswap` | `std::byteswap` |
| `Poco::Int16` ... `Poco::UInt64` | `std::int16_t` ... `std::uint64_t` |
| `Poco/Platform.h` (endian checks) | `std::endian` |
| `Poco::format` | `std::format` |
| `Poco::RandomBuf`, `Poco::Random` | `<random>` (`std::mt19937` with a fixed or random seed, as the test needs) |

## Filter design code (replaces spuce)

- Location: an internal static library in comms, for example
  `filter/design/` with the target `comms_filter_design` (name and path:
  decision of the builder). PIC, not installed. The `filter` and `window`
  modules link it.
- Namespace: an own namespace of comms (for example
  `PothosComms::FilterDesign`), not `spuce`.
- API: only what the blocks need, with C++23 types (`std::vector`,
  `std::span`, `std::complex`, `std::string_view` where it is good).
  Replace `iir_coeff*` (raw pointer from `design_iir`) with a value type
  that contains the `a` and `b` taps. Errors are exceptions, as in spuce
  (the blocks catch `std::runtime_error`). The builder can change the
  exception type if the blocks still give the same error to the user.
- Formulas, defaults and the meaning of the parameters stay the same as
  in spuce. The builder can change the code structure.
- The file headers tell that the code comes from spuce (BSL-1.0, with the
  copyright of spuce).
- No warnings on GCC, Clang and MSVC.
- The run-time IIR filter (`iir_df`, and the part of `fir` that it uses)
  goes into the same library as a small direct form filter template. The
  output must agree with the reference data.
- `IIRFilter::setTaps` calls `IIR.print()`, which writes the taps to
  stdout. Replace it with `POTHOS_LOG_DEBUG` (decision of the designer).
- `set_taps` divides the taps into the `b` and `a` halves. With fewer than
  2 taps, an unsigned value wraps around (undefined behavior). The new
  code rejects fewer than 2 taps with `Pothos::InvalidArgumentException`.
  This is a bug fix (decision of the designer). Record it in the ledger.

### Reference data (proof of the same behavior)

1. Build spuce `ad08df7` locally, one time,
   outside the comms repository (for example with
   `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, without Qt).
2. Write a small generator program. For a set of parameters, it calls the
   spuce functions in the table above and writes the results.
3. The set covers every type and every band of the table. It also covers
   odd and even tap counts, the window arguments (`kaiser` beta,
   `chebyshev` attenuation), IIR orders 1 to 8, and the Remez estimates.
   It also covers the parameters that spuce rejects (the expected error).
   For `iir_df`, it records the output for an impulse and a step input with
   taps from several `design_iir` results, for `float` and
   `std::complex<float>` input.
4. Commit the results as test data in comms (for example a header with
   arrays or a JSON file). Commit the generator source too, for example in
   `filter/design/tools/`. The build does not compile the generator.
5. Self-tests of the new code compare every result with the reference
   data. The tolerance is relative, approximately `1e-9` for FIR and the
   windows. IIR and Remez use iterative or root-finding code, so their
   tolerance can be larger (decision of the builder, documented in the test).
6. The current block tests (`TestFIRDesigner.cpp`, `TestFIRFilter.cpp`,
   `TestIIRFilter.cpp`) must also pass without changes to their
   expectations.

## Tests

- Use the method of blocks:
  1. CTest test `comms.stage` installs comms into `build/stage`.
  2. CTest test `comms.selftests` runs each `/comms/tests/...` test with
     the `PothosUtil` of the installed core. It sets `POTHOS_ROOT` and
     `POTHOS_PLUGIN_PATH`. It uses a copy of `cmake/PothosRunSelfTests.cmake`.
- Every test that passes upstream must pass. If a test fails because of
  a bug in old comms code, fix the bug and record the decision in the
  ledger. Do not change the expectation of a test to hide a fault.
- The comms tests need PothosBlocks at run time. They use
  `/blocks/feeder_source` (48 uses), `/blocks/collector_sink` (58 uses),
  `/blocks/copier`, `/blocks/vector_source`, `/blocks/finite_release` and
  others. (comms registers its own blocks under `/blocks/` too.) Thus the
  tests need an installed PothosBlocks (`markvin/PothosBlocks@pothos2`)
  in the prefix of the core, or in `POTHOS_PLUGIN_PATH`. The build of comms
  does not need PothosBlocks.
- When PothosBlocks is not installed, `comms.selftests` fails with a
  clear message ("install PothosBlocks"). It must not skip the tests
  without a message.

## CI of comms

- The same structure as the CI of blocks: linux-gcc (Debug,
  `-Werror`), linux-clang (Release, `-Werror`), macos (Intel runner,
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC Release).
  1. Check out comms, `markvin/PothosCore@pothos2` and
     `markvin/PothosBlocks@pothos2`.
  2. Install the dependencies with Conan for each repository.
  3. Build and install the core, then build blocks and install it into
     the prefix of the core.
  4. Build comms and run `ctest -V`.
- Job `linux-system` in an `archlinux` container (as in the core, with
  `--init`): system packages, no Conan.
- Keep a guard against downloads in the CMake files, as in the core
  (FetchContent, ExternalProject, `file(DOWNLOAD)`).

## Changes in the core

- Roadmap: phase 10 row and status.
- `CLAUDE.md`: a short note about the comms fork, its branch and its
  local clone.
- No code changes are expected. If comms needs a change of the public API,
  stop and ask the user.

## Decisions of the designer (the user must check them)

- History: branch `pothos2` in the fork, based on upstream `master`.
- xsimd is required. The code paths without xsimd are removed.
- All modules always build. The `ENABLE_COMMS*` options are removed.
- `debian/` is deleted.
- The reference data is generated one time from spuce and committed with
  its generator.
- The reference version of spuce is `ad08df7`, not the newer fork
  `master`.
- `IIR.print()` (stdout) becomes a debug log message. `setTaps` rejects
  fewer than 2 taps.
- CI also gets the `linux-system` job (Arch).
- The comms changes go into `pothos2` of the fork with direct commits on a
  work branch and a PR in the fork (the same as blocks in phase 9). The
  core docs go into `pothos2` of the core with a PR.

## Recommended order of work

1. Fork and branch. New top-level CMake, `conanfile.txt`, the module
   helper and the test runner (with an installed PothosBlocks).
   Temporarily leave `filter` and `window` out.
2. Remove Poco. Change `<json.hpp>` to `<nlohmann/json.hpp>`.
3. `math/SIMD` on `XSIMDCopy.hpp`. Remove `POTHOS_XSIMD`.
4. Reference data from spuce (generator and data, outside the build).
5. The filter design library, with its tests against the reference data.
6. `filter` and `window` on the new library. All tests pass locally.
7. CI (4 jobs with Conan and `linux-system`). README, Changelog, delete
   `debian/`.
8. Core docs: roadmap and `CLAUDE.md`.
