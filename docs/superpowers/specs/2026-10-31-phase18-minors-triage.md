# Phase 18: triage of the deferred minor findings (appendix)

This file is the read-only triage of 2026-10-31 that the phase 18 design uses (`2026-10-31-phase18-minors-design.md`). Part A covers the core ledgers of phases 1, 2, 5a, 6a, 6b, 7 and the phase 8 items; part B covers the core ledgers of phases 3, 4, 5 and the flow and audio items. The design document gives the decisions; this file gives the evidence (file:line), the fix and the test for each item.

## Triage A — deferred minors of phases 1, 2, 5a, 6a, 6b, 7, 8

Scope: every "minor (deferred" line of the ledgers of phases 1, 5a, 6a, 6b, 7, 2
(docs/superpowers/ledgers/) and the 4 phase 8 items of other-items.txt.
State checked: PothosCore working tree (branch phase17-audio = pothos2 + 2 doc
commits, version 2.0), PothosBlocks branch pothos2 (d07cda4), PothosBase main.
Read-only: nothing was edited, built or committed.

Ids: P1-n = phase 1 ledger line order (lines 37..48), P5a-n = phase 5a (M-3..M-6),
P6a-n = phase 6a (M-7..M-12), P6b-n = phase 6b (lines 26..30), P7-n = phase 7
(lines 19..21), P2-n = phase 2 (M-1, M-2, M-4..M-8), P8-n = phase 8 (other-items.txt order).

Counts: OBSOLETE 10, VALID 22, DECISION 9 (total 41).

---------------------------------------------------------------------------

### OBSOLETE (10)

| Id | Item | Evidence |
|----|------|----------|
| P1-3 | poco forces CMAKE_BUILD_TYPE=RelWithDebInfo | Poco removed (phase 5; no `poco/` dir). CMakeLists.txt:32-36 defaults to Release at top level only. |
| P1-4 | Pothos.pc says -lPothos, Debug installs libPothosd | The debug postfix came from Poco. No DEBUG_POSTFIX on Pothos today (only PothosUtil.cmake:103 clears it for modules). build/conan-dbg/stage/lib has libPothos.so (Debug). lib/Pothos.in.pc:14 matches. |
| P1-6 | `<ciso646>` in Config.hpp warns (STL4036) | Removed in 162c468f; include/Pothos/Config.hpp:101-102 is only a comment; `git grep ciso646` finds no include. |
| P1-11 | plan says Poco builds as C++23 | Poco is gone; text-only item. |
| P1-12 | PocoEncodings built/installed | Poco is gone. |
| P6a-3 (M-9) | find_package(Pothos) after add_subdirectory(core) does not find the config | Fixed in 9cfbf8da: package redirect in cmake/Modules/CMakeLists.txt:19-26 (CMAKE_FIND_PACKAGE_REDIRECTS_DIR). Covered by tests/subproject (sibling, plain_a/b, parent find_package) in pothos.subproject. |
| P6b-1 | POTHOS_TEST_* compare C strings by content (pointer identity lost) | Accepted design, documented in Changelog.txt:112. Not a defect. |
| P6b-2 | InputPort/OutputPort layout change without ABI bump | ABI is now 2.0-0 (78606a7b, include/Pothos/System/Version.hpp:41). |
| P6b-5 | PluginRegistry / ModulePaths std::map::operator[] under shared lock | Fixed in 574a4299: Registry.cpp readers use findEntry/find under shared_lock (lines 100-108, 203-204, 250-301); ModulePaths.cpp:24-31 uses find(). operator[] stays only under lock_guard (add/remove, updatePluginAssociation). |
| P7-3 | pothos.subproject only configures, never builds the sibling | Still configure-only (cmake/PothosSubprojectTest.cmake:39-48), but the link path is now built and tested by PothosBase CI (../PothosBase/.github/workflows/ci.yml:153-156: core + toolkits via add_subdirectory, redirect find_package, ctest). Accepted ruling; no action. |

---------------------------------------------------------------------------

### VALID (22)

### P1-1 — developer POTHOS_* environment leaks into the self-tests
- Repo/file: PothosCore cmake/PothosTestEnv.cmake:12-35 (also the copies in the toolkits, e.g. ../PothosBlocks/cmake/PothosTestEnv.cmake).
- Defect: pothos_test_env() sets POTHOS_ROOT, XDG_*, APPDATA, but does not unset
  POTHOS_PLUGIN_PATH (read at lib/System/Paths.in.cpp:162), POTHOS_CONF_PATH
  (lib/ConfLoader/ConfLoader.cpp:116), POTHOS_LOG_LEVEL/CHANNEL/FILE/FORMAT/TIMEZONE/ASYNC
  and POTHOS_SYSLOG_ADDR (lib/System/Logger.cpp:474-480). A developer shell with these
  set makes `pothos.selftests`, consumer and api_compat load foreign modules, foreign
  .conf files, or send the log elsewhere (POTHOS_LOG_CHANNEL=null hides failures;
  POTHOS_LOG_LEVEL=trace floods). Side note: with -DPOTHOS_APPDATA_ENV=SNAP_USER_DATA the
  isolation also misses that variable.
- Fix: in pothos_test_env() add `foreach(v POTHOS_PLUGIN_PATH POTHOS_CONF_PATH POTHOS_LOG_LEVEL POTHOS_LOG_CHANNEL POTHOS_LOG_FILE POTHOS_LOG_FORMAT POTHOS_LOG_TIMEZONE POTHOS_LOG_ASYNC POTHOS_SYSLOG_ADDR) unset(ENV{${v}}) endforeach()`. Same change in the toolkit copies (blocks sets POTHOS_PLUGIN_PATH itself after the call, so keep that order).
- Test: extend tests/test_env_guard.cmake (pothos.test_env_guard): a child `cmake -E env POTHOS_PLUGIN_PATH=/bogus POTHOS_LOG_CHANNEL=null cmake -P wrapper` where the wrapper calls pothos_test_env() and then fails if `DEFINED ENV{POTHOS_PLUGIN_PATH}` or `ENV{POTHOS_LOG_CHANNEL}`.

### P1-2 — pothos.stage does not clean the stage (stale files are loaded)
- Repo/file: PothosCore CMakeLists.txt:135-137 (`cmake --install ... --prefix stage`).
- Defect: the stage prefix keeps every file of earlier installs. A removed header
  (example P6b-4: SpinLockRW.hpp) stays visible to consumer/api_compat; a renamed or removed
  module in stage/lib/Pothos/modules<abi>/ is still loaded by the self-tests; old library
  names stay (build/tsan/stage/lib still has libPothos.so.0.7-4). A full `rm -rf stage` is
  not safe: developers install toolkits (PothosBlocks for comms tests) into the core prefix.
- Fix: run the stage through a small script (cmake/PothosStage.cmake): read the previous
  `${PROJECT_BINARY_DIR}/install_manifest.txt` if it exists, file(REMOVE) each listed path that
  is under the stage prefix, then `cmake --install`. Only the core's own old files go; toolkit
  files in the prefix stay.
- Test: a CMake -P test (pothos.stage_clean or inside test_env_guard style): write a fake
  manifest that lists `<work>/stage/include/Pothos/Stale.hpp`, create that file and a foreign
  file not in the manifest, run the removal part, check Stale.hpp is gone and the foreign file stays.

### P1-5 — dead variables Pothos_LINKER_FLAGS / Pothos_COMPILE_OPTIONS / Pothos_COMPILE_DEFINITIONS
- Repo/file: PothosCore lib/CMakeLists.txt:283, 286, 287.
- Defect: nothing in the core sets these variables (`git grep` finds only these uses). They are
  added PUBLIC to the exported Pothos target. In a subproject build a parent variable with the
  same name would leak into Pothos::Pothos (and into the installed export) — a subproject hazard,
  not only dead code.
- Fix: delete the three lines.
- Test: no practical automatic test of dead code; P1-10's extended consumer check would catch a
  leak through INTERFACE_COMPILE_OPTIONS. Optional: tests/subproject sets
  `Pothos_COMPILE_DEFINITIONS=PARENT_LEAK` before add_subdirectory and fails if
  `get_target_property(Pothos INTERFACE_COMPILE_DEFINITIONS)` contains it.

### P1-8 — the CI "no network" grep does not cover all build scripts
- Repo/file: PothosCore .github/workflows/ci.yml:47-52.
- Defect: the original gap (poco/**, libsimdpp/cmake) is gone with those dirs, but the grep
  scans only `CMakeLists.txt lib apps cmake`. Build scripts in docs/CMakeLists.txt,
  include/CMakeLists.txt and tests/** (consumer, api_compat, subproject, test_env_guard.cmake)
  are not scanned. Note: tests/subproject/CMakeLists.txt legitimately uses `FetchContent_Declare(... SOURCE_DIR ...)` (local source, no download), so a plain grep over tests/ would fail.
- Fix: grep `$(git ls-files 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake' '*.cmake.in')` with an
  exclusion for tests/subproject (or match only `file(DOWNLOAD` / `URL ` / `GIT_REPOSITORY`).
- Test: the CI step is the test; no further automatic test.

### P1-9 — CI has no permissions block, no concurrency; push+PR runs twice
- Repo/file: PothosCore .github/workflows/ci.yml:3-5 (`on: push:` for every branch, `pull_request:`), no `permissions:` and no `concurrency:` keys.
- Defect: the GITHUB_TOKEN gets the repository default scope; a push to a PR branch runs the full
  matrix twice (push + pull_request); superseded runs are not cancelled (macOS/Windows minutes).
- Fix: add `permissions: { contents: read }`; `concurrency: { group: ${{ github.workflow }}-${{ github.ref }}, cancel-in-progress: true }`; limit `push` to `branches: [master, pothos2]`. Same for the toolkit and PothosBase workflows.
- Test: no practical automatic test (actionlint can check syntax only).

### P1-10 — consumer leak check ignores INTERFACE_COMPILE_OPTIONS / LINK_OPTIONS
- Repo/file: PothosCore tests/consumer/CMakeLists.txt:11.
- Defect: the loop checks only INTERFACE_INCLUDE_DIRECTORIES, INTERFACE_LINK_LIBRARIES,
  INTERFACE_COMPILE_DEFINITIONS. A private dependency that leaks through
  INTERFACE_COMPILE_OPTIONS, INTERFACE_LINK_OPTIONS or INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
  (e.g. an `-I.../xsimd` option, see P1-5) is not detected.
- Fix: add INTERFACE_COMPILE_OPTIONS INTERFACE_LINK_OPTIONS INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INTERFACE_LINK_DIRECTORIES to the foreach list.
- Test: pothos.consumer itself. RED proof by hand once: add `target_compile_options(Pothos PUBLIC -DXSIMD_LEAK)` (matches "xsimd") and see the consumer fail.

### P5a-1 (M-3) — never-dispatched and duplicate SIMD copies are built
- Repo/file: PothosCore cmake/Modules/PothosConfigSIMD.cmake:1222-1227 (X86_XOP and X86_AVX,X86_XOP copies); :72-77, :113-121, :157-165, :201-209 (`POTHOS_WIN32` is never set anywhere, so on MSVC the sse2/sse3/ssse3/sse4_1 copies have no flag); lib/System/SIMD.cpp:56-72 never reports `x86_xop`.
- Defect: on GCC/Clang x86 the core builds 11 copies of SIMDConvert.cpp
  (build/conan-dbg/lib/SIMDConvertArches.hpp): `x86_xop` and `x86_avx__x86_xop` can never be
  chosen (no feature name `x86_xop` is detected); `x86_sse2` equals `fallback` on x86-64. On MSVC
  sse2/sse3/ssse3/sse4_1 are 4 more copies of the fallback code. The same set is built for every
  toolkit SIMD source (PothosGenerateSIMDSources) — wasted build time and binary size, no wrong result.
- Fix: remove the XOP block from pothos_get_arch_perm (lines 1222-1227); skip X86_SSE2 on 64-bit
  x86 (fallback already has SSE2); skip sse3/ssse3/sse4_1 copies when the arch has no flag
  (empty `POTHOS_<ARCH>_CXX_FLAGS` on MSVC).
- Test: new self-test in lib/SIMD/Builtin/TestSIMDConvert.cpp, `test_simd_copy_keys_reachable`:
  for each key of POTHOS_SIMD_CONVERT_ARCHES, every `__`-separated feature must be in a list of all
  names detectSIMDFeatures can report (new `Detail::knownSIMDFeatureNames()` next to
  detectSIMDFeatures). RED today on x86 (`x86_xop`). Plus: no two copies with the same xsimd arch
  name (`xsimdArch()` from test_simd_convert_copy_flags) except where documented.

### P5a-2 (M-4) — ARM copy keys never match the detected feature names
- Repo/file: PothosCore cmake/Modules/PothosConfigSIMD.cmake:714, 759, 768 (suffixes `-arm_neon`, `-arm_neon_flt_sp`, `-arm64_neon`) vs lib/System/SIMD.cpp:74-77 (detects `neonfltsp`, `neon`); matching in lib/System/SIMD.cpp:117-167.
- Defect: getOptimalSIMDFeatureSetKey never selects an ARM copy: on armhf the NEON copy
  (`-mfpu=neon`) is never used, the fallback (often VFP only) runs instead; on aarch64 the
  `arm64_neon` copy is never used (little loss: baseline is NEON). The same mismatch is in every
  toolkit dispatcher (PothosUtil --generate-simd-dispatchers uses the same keys).
- Fix: in detectSIMDFeatures also report the key names: `add(cpu.neon(), "arm_neon")`,
  `add(cpu.neon(), "arm_neon_flt_sp")`, `add(cpu.neon64(), "arm64_neon")`; keep `neon`/`neonfltsp`
  for the old `--simd-features` output. (Visible change: two or three more names in
  getSupportedSIMDFeatureSet on ARM.)
- Test: the same `test_simd_copy_keys_reachable` as P5a-1 (fails on ARM builds today). No ARM CI job
  exists (macOS runner is Intel now); the check of the name list runs on any host if it compares the
  key list of PothosConfigSIMD suffixes (all archs) to the known names, e.g. a CTest that greps the
  `_SUFFIX` values in PothosConfigSIMD.cmake against the names in SIMD.cpp.

### P5a-3 (M-5) — test_simd_convert_unaligned covers only int32→float on the dispatched copy
- Repo/file: PothosCore lib/SIMD/Builtin/TestSIMDConvert.cpp:138-157.
- Defect: alignment faults (aligned loads on unaligned buffers) in other type pairs or other copies
  would not be found. Test coverage gap only; the reviewer's harness found 0 faults.
- Fix: loop the same body over POTHOS_SIMD_CONVERT_PAIRS and over every copy this CPU can run (as
  test_simd_convert_matches_scalar already does, line 119-136), offsets 1..(sizeof(T)-1) and odd n.
- Test: the extended self-test itself (`/framework/tests/test_simd_convert_unaligned`).

### P6a-1 (M-7) — version hash from a parent git repo when the core is vendored without .git
- Repo/file: PothosCore CMakeLists.txt:39-48; cmake/Modules/GetGitRevisionDescription.cmake:42-55 (walks up parent dirs until it finds `.git`).
- Defect: a source export (tarball, `git archive`, a vendored copy committed into another repo)
  inside another git work tree gets `POTHOS_EXTVER=g<parent hash>`, and CMake re-configures on every
  commit of the parent (HEAD dependency). Submodules and worktrees (a `.git` file) work.
- Fix: `if (NOT POTHOS_EXTVER AND EXISTS "${PROJECT_SOURCE_DIR}/.git")` around the git lookup; else `unknown`.
- Test: a CTest script: `git archive HEAD` into `<work>/outer/core`, `git init` + one commit in
  `<work>/outer`, configure only, check the "Pothos version:" line ends with `-unknown`. Costs one
  configure (needs the dependency prefix like pothos.subproject).

### P6a-2 (M-8) — installed Pothos found by the parent + add_subdirectory(core) fails
- Repo/file: PothosCore lib/CMakeLists.txt:200 (`add_library(Pothos::Pothos ALIAS Pothos)`).
- Defect: when the parent called find_package(Pothos) for an installed core before
  add_subdirectory(core), the imported Pothos::Pothos exists and the ALIAS fails with a generic
  "another target with the same name already exists" error.
- Fix: in the top CMakeLists.txt before add_subdirectory(lib): `if(TARGET Pothos::Pothos) message(FATAL_ERROR "Pothos::Pothos already exists (find_package(Pothos) before add_subdirectory of the core?): use one of them")`.
- Test: a pothos.subproject variant (-DPOTHOS_SUBPROJECT_FIND_FIRST=ON: the parent does
  find_package(Pothos) with the staged prefix first) that expects a failed configure with this text;
  PothosSubprojectTest.cmake needs an "expect failure" helper.

### P6a-4 (M-10) — INSTALL_RPATH_USE_LINK_PATH is hard-wired
- Repo/file: PothosCore CMakeLists.txt:93-96 (pothos_target_settings).
- Defect: before phase 6a, `option(CMAKE_INSTALL_RPATH_USE_LINK_PATH ... TRUE)` let a packager turn
  it off; now it is always TRUE, so install RPATHs get the link dirs of non-system dependencies
  (Debian/Fedora policy forbids RPATHs to system or build paths).
- Fix: `if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)` set the property TRUE, else leave the
  property to its default (CMake initializes it from the variable). Reads a CMAKE_* variable, does
  not set it, so the subproject rule holds.
- Test: tests/subproject: the parent sets `CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF` and checks
  `get_target_property(Pothos INSTALL_RPATH_USE_LINK_PATH)` is false.

### P6a-5 (M-11) — pothos.subproject does not forward all toolchain settings (residual)
- Repo/file: PothosCore CMakeLists.txt:182-192, cmake/PothosSubprojectTest.cmake:28-45.
- Status: mostly fixed in 2144ae8f (TOOLCHAIN, PREFIX_PATH, BUILD_TYPE are forwarded). Still not
  forwarded: CMAKE_MAKE_PROGRAM (Ninja not on PATH → configure of the parent fails),
  CMAKE_GENERATOR_PLATFORM/TOOLSET (-A/-T with Visual Studio generators).
- Fix: pass `-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}` and the platform/toolset, forward as
  `-DCMAKE_MAKE_PROGRAM=...`, `-A`, `-T` when not empty.
- Test: no practical automatic test in CI (Ninja is on PATH everywhere); low priority.

### P6a-6 (M-12) — CMP0144 policy warning (residual in the test projects)
- Repo/file: PothosCore tests/consumer/CMakeLists.txt:9, tests/api_compat/CMakeLists.txt:9.
- Status: the in-tree find_package is gone (d3dd3b19, CMakeLists.txt:113-115 includes the config
  directly), so the core configure is clean. But pothos.consumer and pothos.api_compat print the
  CMP0144 warning on every run (build/conan-dbg/Testing/Temporary/LastTest.log:404, 557):
  pothos_test_env sets ENV{POTHOS_ROOT}, and the test projects use cmake_minimum_required(3.25)
  without NO_PACKAGE_ROOT_PATH. The toolkits already use NO_PACKAGE_ROOT_PATH (CLAUDE.md).
- Fix: `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)` in both test projects (shows the
  recommended toolkit form too).
- Test: in cmake/PothosConsumerTest.cmake pass `-Werror=dev` to the configure step, so any developer
  warning (this one included) fails pothos.consumer / pothos.api_compat.

### P6b-4 — stale build/stage/include/Pothos/Util/SpinLockRW.hpp
- Same root cause and fix as P1-2 (stage keeps removed files). No stale copy in the current build
  dirs (checked build/*/stage), but any old build dir has it. Fixed by P1-2.

### P7-2 — POTHOS_BUILD_APPS=OFF + POTHOS_MODULE_UTIL(ENABLE_DOCS) fails at build time with an empty command
- Repo/file: PothosCore cmake/Modules/PothosUtil.cmake:129-151; cmake/Modules/PothosConfig.cmake:80-87 (POTHOS_UTIL_EXE is not set without apps); same problem in PothosConfigSIMD.cmake:1354-1358 (PothosGenerateSIMDSources runs `${POTHOS_UTIL_EXE} --generate-simd-dispatchers`).
- Defect: a toolkit next to a core built with POTHOS_BUILD_APPS=OFF configures fine, then the build
  fails on a custom command whose program is empty.
- Fix: `if(NOT POTHOS_UTIL_EXE) message(FATAL_ERROR "POTHOS_MODULE_UTIL(${POTHOS_MODULE_UTIL_TARGET}): docs need PothosUtil; the core is built with POTHOS_BUILD_APPS=OFF")` inside the ENABLE_DOCS branch; same check at the start of PothosGenerateSIMDSources.
- Test: pothos.subproject no-apps variant with `-DSIBLING_DOCS=ON` (sibling calls POTHOS_MODULE_UTIL
  with ENABLE_DOCS even without apps) expecting a configure failure with the text.

### P2-1 (M-1) — float out-of-range direction taken from the exponent sign
- Repo/file: PothosCore lib/Util/Expr/Lexer.cpp:316-322 (parseDecimalFloat, from_chars path).
- Defect: on result_out_of_range the result is `inf` unless the written exponent starts with `-`.
  `0.<400 zeros>1` (no exponent) gives `inf` (correct: 0.0); `0.<400 zeros>1e5` gives `inf` too.
  The libc++ stream path (parseFloatStream, :329-344) decides from the value and is correct, so
  results differ by platform. Wrong numeric result.
- Fix: decide by the decimal magnitude: effective exponent = exp + (digits of intPart without leading
  zeros − 1), or exp − (index of the first non-zero fraction digit + 1) when intPart is all zeros;
  negative → 0.0, else inf.
- Test: new `test_expr_float_out_of_range` in lib/Util/Builtin/TestExpr.cpp: eval of
  `"0." + std::string(400,'0') + "1"` == 0.0, the same with `e5` == 0.0, `"1" + 400 zeros + "e-100"`
  stays finite (not out of range), `1e400` == inf, `1e-400` == 0.0.

### P2-2 (M-2) — Value nesting depth is unbounded for values built through constants
- Repo/file: PothosCore lib/Util/Expr/Convert.cpp:13-29, 43-87 (toObject/fromObject recurse); lib/Util/Expr/Value.cpp:99-131 (repr recurses); lib/Util/EvalEnvironment.cpp:64-68 (define) and :104-116 (registerConstantObj). Parser limits (MaxNesting, MaxTreeDepth, StackGuard) do not apply to stored values.
- Defect: chains like `registerConstantExpr("a<i>", "[a<i-1>]")`, or a deep ProxyVector given to
  registerConstantObj, build values of any depth; toObject/fromObject/repr/== then recurse without
  a limit and can overflow the stack (crash instead of an exception). Needs a hostile or very odd
  caller; low priority.
- Fix: a depth count in fromObject and in define (helper `valueDepth()`), throw ExprError
  "value is nested too deeply" past MaxTreeDepth (256).
- Test: `test_eval_constant_depth` in lib/Util/Builtin/TestEvalExpression.cpp: register 300 nested
  constants in a loop, POTHOS_TEST_THROWS on the registration past the limit (today: no throw → RED).

### P2-4 (M-5) — round(int, -19) returns 0 instead of an overflow error
- Repo/file: PothosCore lib/Util/Expr/Builtins.cpp:181 (`if (n < -18) return Value(0);`).
- Defect: for n == -19 and |x| > 5·10^18, the correct result is ±10^19, which does not fit int64 (other
  int overflows are errors). Example: `round(9000000000000000000, -19)` gives 0.
- Fix: `if (n < -19) return Value(0); if (n == -19) { if (x > 5000000000000000000 or x < -5000000000000000000) fail(... "integer overflow" ...); return Value(0); }` (same error text as binaryOp overflow).
- Test: TestExpr.cpp: `round(9000000000000000000, -19)` throws, `round(5000000000000000000, -19) == 0`, `round(-9000000000000000000, -19)` throws.

### P2-7 (M-8) — only ExprError becomes Pothos::Exception
- Repo/file: PothosCore lib/Util/EvalEnvironment.cpp:46-61 (Impl::evaluate catches only Expr::ExprError); :98-101 and :112-115 catch only Pothos::Exception.
- Defect: std::bad_alloc / std::length_error (e.g. repeated `s+s` constants) leave eval() and
  registerConstant* as std exceptions, without the "EvalEnvironment::eval ... in: <expr>" context; a
  caller that catches Pothos::Exception (PothosFlow property evaluation) does not catch them.
- Fix: after the ExprError catch add `catch (const Pothos::Exception &) {throw;} catch (const std::exception &ex) {throw Pothos::Exception("EvalEnvironment::eval", std::string(ex.what()) + " in: " + expr);}`; same in registerConstantObj around fromObject.
- Test: no practical automatic test: a bad_alloc/length_error needs huge allocations (doubling a string
  to max_size()); a unit test would need a fault-injection hook.

### P8-1 — network endpoint constructor leaks Impl on errors
- Repo/file: PothosBlocks network/SocketEndpoint.cpp:268-289.
- Defect: `_impl(new Impl())` is a raw pointer; the constructor deletes it only on
  `std::system_error` (:284-288). parseUri errors (:275, InvalidArgumentException), the scheme/opt
  check (:276-280) and name resolution errors (resolveHost via hostAddress :76-82 throws
  Pothos::IOException / TimeoutException) leave the constructor with `_impl` leaked (the destructor
  does not run). Small leak per failed network_source/sink creation.
- Fix: guard: `std::unique_ptr<Impl> guard(_impl);` at the start of the body, `guard.release();` at
  the end; drop the manual delete (keep the system_error → RuntimeException translation).
- Test: `POTHOS_TEST_THROWS(PothosPacketSocketEndpoint("udp://127.0.0.1:1", "CONNECT"), Pothos::InvalidArgumentException)` in network/TestNetworkBlocks.cpp; the leak itself is only visible under LeakSanitizer (no ASan CI job) — otherwise no practical automatic test.

### P8-2 — CONNECT tries only the first resolved address
- Repo/file: PothosBlocks network/SocketEndpoint.cpp:53, 67-72, 76-82 (hostAddress → Pothos::Util::resolveHost → core lib/Net/Address.cpp:125-128 returns `.front()`, IPv4 first).
- Defect: a host name with several addresses gets one attempt. Example: `tcp://localhost:<port>`
  when the bound side listens on `[::1]` only: 127.0.0.1 is tried, refused, no IPv6 attempt. The core
  TcpStream tries all addresses (lib/Net/TcpStream.cpp:128), so remote/proxy and network blocks behave differently.
- Fix (blocks only, no new core API): in the CONNECT branch use `asio::ip::tcp::resolver::async_resolve`
  + `asio::async_connect(socket, results, ...)` under the existing runFor(ConnectTimeout) (blocks
  already uses Asio directly). Alternative: add public `Pothos::Util::resolveHostAll` (API addition).
- Test: blocks self-test: network_source bound on `tcp://[::1]:0`, network_sink connects to
  `tcp://localhost:<port>`; skip when `localhost` does not resolve to both 127.0.0.1 and ::1 (environment dependent).

---------------------------------------------------------------------------

### DECISION (9)

### P1-7 — CMAKE_OSX_DEPLOYMENT_TARGET=13.3 only in CI
- Where: .github/workflows/ci.yml:35; spec docs/superpowers/specs/2026-10-26-core-modernization-design.md:263-266 (minimum macOS 13.3 for floating-point std::format/to_chars).
- Choice: (a) default the target in CMakeLists (must be set before project() and is a global CMAKE_*
  variable: breaks the subproject rule); (b) keep the compiler default (host version, always ≥ 13.3
  on a supported Mac) and add a configure check: `if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 13.3) message(FATAL_ERROR ...)`, plus one README line.
- Recommend (b): no global setting, a clear error in place of the libc++ "unavailable" errors.

### P5a-4 (M-6) — no AVX-512F-only copy (Knights Landing/Mill use the AVX2 copy)
- Where: cmake/Modules/PothosConfigSIMD.cmake:1205-1220 (F-only copy only when BW is not compilable).
- Recommend: no change. Xeon Phi is discontinued; one more copy per SIMD source in every toolkit costs
  build time for no current hardware. Close the item.

### P6b-3 — SpinLock::unlock always calls notify_one
- Where: include/Pothos/Util/SpinLock.hpp:70-74; the only core user is ActorInterface::_extCallLock (lib/Framework/ActorInterface.hpp:110, try_lock/unlock only).
- Choice: a waiter flag/count would skip notify_one, but changes the layout of a public inline class
  (ABI 2.0-0 just released) and adds a race-prone path. libstdc++/libc++ notify_one checks a waiter
  count in user space (no syscall without waiters).
- Recommend: no change; close the item.

### P7-1 — remote host older than doesBlockExist (0.7.1) fails the existence check
- Where: lib/Framework/TopologyToolkitBlocks.hpp:31 (calls "doesBlockExist" through the remote registry proxy).
- Choice: support a 0.7.x remote host (catch the missing method, fall back to the old get) or declare
  remote interoperation between 2.0 and 0.7.x unsupported. There is no version handshake in lib/Remote.
- Recommend: unsupported (2.0 has a new ABI/API and module dir modules2.0-0); one sentence in the
  Changelog/README "remote hosts must run the same major version". Optional later: a version check
  in the remote handshake with a clear error.

### P2-3 (M-4) — user constants can shadow true/false/null/True/False/None
- Where: lib/Util/EvalEnvironment.cpp:21-29 (isValidName allows them); lib/Util/Expr/Eval.cpp:103-104 (user lookup before builtinConstant).
- Choice: (a) reject these names in isValidName (registration throws "invalid constant name");
  (b) builtins win in lookup (registration silently has no effect); (c) keep.
- Recommend (a): a clear error, no silent change of `true`. Visible change only for a caller that
  registered such a name (unlikely). Test: TestEvalExpression `POTHOS_TEST_THROWS(env.registerConstantExpr("true","0"), Pothos::Exception)`.

### P2-5 (M-6) — strings are byte strings ("\xe9" is 1 byte, len("é") == 2)
- Where: lib/Util/Expr/Lexer.cpp:272-278 (`\xHH` appends one byte), :280-293 (`\u` appends UTF-8); doc include/Pothos/Util/EvalEnvironment.hpp:31.
- Choice: Python semantics (`\xHH` = code point U+00HH as UTF-8, len counts code points) or byte strings.
- Recommend: keep byte strings (the values go to std::string block parameters; `\xHH` is the only way
  to write a raw byte, e.g. packet delimiters) and document it in the Doxygen comment: "str is a byte
  string: \xHH is one byte, \uXXXX is UTF-8, len() counts bytes".

### P2-6 (M-7) — str()/repr() of floats: '1e+05' where Python gives '100000.0'
- Where: lib/Util/Expr/Value.cpp:50-57 (floatRepr: std::format("{}") shortest form).
- Choice: Python repr rule (shortest round-trip digits; fixed notation when 1e-4 <= |x| < 1e16;
  `.0` for integral values) or keep.
- Recommend: Python rule (the language is documented as Python-like; str() output reaches labels and
  file names). About 20 lines: std::to_chars(..., chars_format::scientific) shortest, then lay out the
  digits. Test in TestExpr.cpp: str(100000.0) == '100000.0', str(1e16) == '1e+16', str(0.0001) == '0.0001', str(0.00001) == '1e-05'.

### P8-3 — SIMD copies: inline std:: functions depend on the link order
- Where: PothosCore cmake/Modules/PothosConfigSIMD.cmake:1067-1127 (pothos_multiarch; copies in
  arch order, fallback first), include/Pothos/Util/XSIMDCopy.hpp:26-31 (only xsimd is renamed per copy).
- Facts: the core copies (built -O3, lib/CMakeLists.txt:488-491) emit no weak symbols outside their
  namespaces (`nm` on build/conan-dbg SIMDConvert-*.o: 0). PothosBlocks Debug copies do: e.g.
  `std::max<float>`, `std::numeric_limits<int>::min()`, `std::ceil(float)` as W symbols in the AVX
  copies (../PothosBlocks/build/dbg/stream/.../Clamp-316b42.cpp.o). The linker keeps the first
  definition; the fallback copy is first and (checked for Clamp/IsX/MinMax/Round) defines a superset
  of these symbols, so today the SSE/fallback version wins. A change in order, a symbol only in an AVX
  copy, or LTO could make a fallback path call AVX code (SIGILL on older CPUs).
- Choice: (a) accept link order, document it in pothos_multiarch and add a guard test; (b) remove the
  dependency (no out-of-line std:: calls in copies: -O2 minimum and always_inline wrappers) — not
  fully enforceable.
- Recommend (a) + guard: a Linux CTest (core and toolkit template) that runs `nm -C --defined-only`
  on each copy object and fails when a non-fallback copy defines a weak symbol outside its own
  namespace and `xsimd_*` that the fallback copy of the same source does not define. Also make
  pothos_multiarch always emit the fallback copy first (it does today: NONE_NULL is first in
  pothos_get_arch_perm) and say so in a comment.

### P8-4 — Windows non-ASCII paths (file blocks and core)
- Where: PothosBlocks file/FileUtils.hpp:28-36 (`open(const char*)`), file/MemoryMappedBufferContainer.cpp:148,179 (CreateFileA/CreateFileMappingA), file/TextFileSink.cpp:80 (`ofstream::open(const char*)`), `std::filesystem::exists(std::string)` in BinaryFileSource.cpp:134. Core: lib/Util/FileLockWindows.cpp:26 (CreateFile = A without UNICODE), lib/Framework/MemoryMappedBufferContainer.cpp:143,174, std::filesystem::path(std::string) in lib/System/Paths.in.cpp:30-141, while getEnv returns UTF-8 (lib/Platform/EnvironmentWindows.cpp:18-20). DynamicLibrary and Process already use W APIs.
- Defect: on Windows a std::string path is UTF-8 in some places and the ANSI code page in others; a
  path with non-ASCII characters (user name in the install or data dir, file block paths) fails or
  opens another file.
- Choice: (a) policy "std::string paths are UTF-8 everywhere" with helpers
  (`toPath(const std::string&)` via std::u8string, `toUtf8(path)`), W APIs / `_wopen` / path
  overloads at each boundary; (b) a process manifest with activeCodePage=UTF-8 for PothosUtil and
  PothosFlow (Windows 10 1903+), which does not help other hosts of libPothos.
- Recommend (a), with (b) as an optional extra later. Test (runs on every OS, meaningful on the
  Windows CI job): a core self-test that creates a FileLock and a memory-mapped buffer on
  temp_directory_path() / u8"тест_é.bin"; a blocks self-test that runs binary_file_sink →
  binary_file_source through such a path.

---------------------------------------------------------------------------

### Extra: other phase 8 deferred items (PR #10 "Deferred (minor)"), not in other-items.txt
Listed for completeness, only briefly checked:
- DatagramIO keeps some upstream quirks — too vague to triage.
- The SIMD test exercises only the best architecture of the host — core: fixed for SIMDConvert
  (test_simd_convert_matches_scalar runs every runnable copy); blocks stream SIMD test: not checked.
- The copier test does not assert that a copier was inserted — ../PothosBlocks/stream/TestCopier.cpp:71-86:
  still no explicit assert, but both blocks throw PortDomainError for the other domain, so a direct
  connection cannot work; low value.
- Windows: the NUL handle is closed outside spawnMutex — still true (lib/Platform/ProcessWindows.cpp:186-201):
  after the lock is released the inheritable NUL handle can leak into a child that another thread
  starts. Harmless (a handle to NUL); fix: CloseHandle inside the locked block. Belongs to triage B (runtime/app).

## Phase 18 triage B: deferred minor findings (core phases 3-5, flow, audio)

Read-only triage, 2026-10-31. Code that was read:

- PothosCore: branch phase17-audio (pothos2 content), head c323c226.
- PothosFlow: branch pothos2, head 0369d13.
- PothosAudio: branch phase17-audio, head **f5e4982** (one commit after
  8a12cdf: "Sink: start to play with a margin of 2 periods"). Origin has
  the same head.

Ids: P3-n = phase 3 ledger line n of the "minor (deferred)" list (in
order), P4-Mn / P5-Mn = the review ids of the ledgers, FLOW-n, AUDIO-n =
the lines of the item files. A letter (a, b, c) splits a line with two or
more findings.

Counts: VALID 38, DECISION 9, OBSOLETE 3 (50 entries from 38 source
lines: 15 phase 3, 8 phase 4, 7 phase 5, 2 flow, 8 audio).

## Most important VALID items

1. P4-M5: TcpStream::connect can hang forever after a timeout.
2. P3-8b: Process::wait() holds the mutex while it blocks: kill() from
   another thread waits for the exit that it must cause.
3. P5-M5b: --daemon exits 0 when the pid file cannot be written.
4. P3-13a: JIT: std::filesystem errors escape the error cache and the log.
5. P3-1: logger dispatch can start a new writer thread after shutdown.

---

## OBSOLETE

### P3-5 Bad level warning at load starts the writer thread; no pthread_atfork (phase 5 --daemon)
The thread still starts (lib/System/Logger.cpp:501 -> dispatch ->
Logger.cpp:150). But the consequence was a plain fork() for --daemon, and
phase 5 replaced it with fork+exec (commit d6a10f49,
lib/App/ServerUnix.cpp:57-82). The exec'd daemon has fresh threads. The
only fork() in lib/ and apps/ is that one. test_app_daemon_with_threads
(lib/App/Builtin/TestServer.cpp:292) sets POTHOS_LOG_LEVEL=bogus and
POTHOS_SYSLOG_ADDR and checks that SIGTERM ends the daemon.

### P5-M6 CMAKE_DEBUG_POSTFIX 'd' came from Poco
No CMake file of the core sets DEBUG_POSTFIX now (only
cmake/Modules/PothosUtil.cmake:103 clears it for modules). Debug and
Release both make libPothos, which agrees with "-lPothos" (this also
closes the phase 1 minor "Pothos.pc says -lPothos but Debug installs
libPothosd"). Version 2.0 renamed the library (libPothos.so.2.0-0,
Changelog.txt:12-15) and requires a rebuild, so stale 'd' build trees are
a user action, not a code defect.

### AUDIO-4a No pre-fill of the sink at start (a few aU possible)
Fixed by PothosAudio f5e4982: AudioBlock.cpp:177-178 (_prefillFrames = 2
periods) and AudioBlock.cpp:284-295 (silence until the ring buffer has
the margin, or 2 callbacks for a short stream). The second half of that
ledger line is AUDIO-4b (still valid).

---

## DECISION

### P3-3 POTHOS_LOG_LEVEL=none no longer disables logging
Logger::parseLevel (lib/System/Logger.cpp:203-215) accepts only
fatal..trace and 1..8. "none" gives the warning and the level
information (Logger.cpp:482-483, 498-501). Poco accepted "none" (priority
0: nothing is logged).
Choices: (a) keep; (b) accept "none" (and "off") as "log nothing".
Recommendation: (b). Scripts and old docs use it, and the cost is small.
Do it with an internal "disabled" root threshold below Fatal, not with a
new public LogLevel value. Test: test_logger_bad_level_env: set
POTHOS_LOG_LEVEL=none, configureFromEnvironment(capture sink), log at
Fatal, flush, expect 0 records and no warning.
Side note: include/Pothos/System/Logger.hpp:187-189 says "default notice"
and "all non-informational logs", but the default is "information"
(Logger.cpp:474). Correct the comment.

### P3-4 Error level is asynchronous (last errors before a crash may be lost)
Still true: only level <= Critical is synchronous (Logger.cpp:142). The
public doc says so (include/Pothos/System/Logger.hpp:81-82).
Choices: (a) keep; (b) make Error synchronous.
Recommendation: (a). Error messages come from work() threads (for
example the audio xrun reports in LOGGER mode). A synchronous write
would add a flush and a console write to the data path during an
overload. POTHOS_LOG_ASYNC=0 already gives synchronous logs for crash
analysis. Close the item.

### P3-12 Windows expandPath turns an undefined %VAR% into an empty string
lib/Util/FileSystem.hpp:69-80 replaces every %NAME% with the value or
with "". The doc comment (FileSystem.hpp:63) and the test
(lib/Util/Builtin/TestFileSystem.cpp:27) make this the intended
behavior. Poco used ExpandEnvironmentStringsW, which keeps an undefined
%VAR% as it is.
Choices: (a) keep (the same rule as $VAR on Unix); (b) keep an undefined
%VAR% unchanged (the Windows rule).
Recommendation: (b). A Windows file name can contain '%' (for example
"50%a%b.json"). With (a) such a path changes silently. Change
FileSystem.hpp:78 to append path.substr(i, end-i+1) when the variable is
not set, and change the expected value at TestFileSystem.cpp:27 to
"%POTHOS_SURELY_UNSET_VARIABLE%a".
Side note: envOrEmpty (FileSystem.hpp:53-57) uses std::getenv, which
gives the ANSI code page on Windows (non-ASCII values break). This is the
phase 8 "Windows non-ASCII paths" item.

### P4-M9 SyslogSender resolves the address once at construction (Poco retried lazily)
lib/Net/Syslog.cpp:345-354: resolveHost once, 5 s timeout, an exception
on failure. With POTHOS_SYSLOG_ADDR set to a name that does not resolve:
at library load the static block logs an error (safeInit,
lib/Plugin/Static.cpp:21-24). In PothosUtil, setupDefaultLogging()
(apps/PothosUtil.cpp:214) throws, and every command exits 64.
Choices: (a) keep resolve-once; (b) resolve lazily and again after
errors, as Poco did.
Recommendation: (a), but make a failure not fatal. All internal users
pass IP literals (RemoteServer, Flow). In __startSyslogForwarding
(Logger.cpp:367), catch the exception in configureFromEnvironment, keep
the console sink and log one warning. Test: test_logger_bad_syslog_addr:
set POTHOS_SYSLOG_ADDR=no.such.host.invalid:514, call
configureFromEnvironment(capture), expect no exception and one warning.

### P4-M10 Read errors set eof/fail, not badbit (plan text says badbit)
lib/Net/TcpStream.cpp:34-35: underflow returns eof for an error and for
the end of the stream. The header documents "like file streams"
(lib/Net/TcpStream.hpp:18), and std::filebuf does the same. No caller
makes a difference between an error and the end of the stream.
Recommendation: close. Only the old plan text is different.

### P5-M8 --help together with an invalid option now exits 64
apps/PothosUtil.cpp:157-164: parse first, then help.
Choices: (a) keep; (b) show the help when --help is given, before the
check of the other options.
Recommendation: (a). An invalid command line is an error. The
behavior is simple and does not depend on the option order (GNU tools
exit 0 or 2 depending on the order). Close the item.

### FLOW-4 A second PothosFlow instance fails on the locator port
MainWindow::setupServer (PothosFlow MainWindow/MainWindow.cpp:281-305)
always starts a RemoteServer on the locator port 16415. The second
instance gets RemoteServerError (bind fails), and PothosFlow.cpp:49 shows
"PothosFlow Application Error". The variable spawnNewServer
(MainWindow.cpp:284-298) has no effect on the decision: the test
connection to an existing server only sets it. Upstream has the same code
(upstream/master MainWindow.cpp:282-306).
Choices: (a) keep (one instance for each host); (b) use the server of the
first instance when it answers (the intent of spawnNewServer); (c) start
the second server on port 0.
Recommendation: (a), with a clear message. With (b) and (c) the blocks of
the second instance run in the server process of the first instance (the
"localhost" node is the locator port), and they stop when the first
instance exits. Catch RemoteServerError in setupServer. Give the text
"Another PothosFlow or PothosUtil --proxy-server uses port 16415". Remove
the dead spawnNewServer test connection, or keep it only for the
reload path. Test: no practical automatic test in the smoke test (it has
the pothos_locator_port lock). A manual check: start two instances.

### AUDIO-2 ma_uint32(sampRate) truncates; rate 0 selects the native rate
PothosAudio AudioBlock.cpp:145: config.sampleRate = ma_uint32(sampRate).
44100.9 becomes 44100. 0 makes miniaudio use the native rate. A negative
or NaN value is undefined behavior (double to unsigned conversion).
Choices: (a) document 0 as "native rate"; (b) reject rates <= 0.
Recommendation: reject NaN and values <= 0 with
Pothos::InvalidArgumentException, and round with std::lround. The block
doc has no "native rate" option. The rxRate label gives the real rate
(_sampleRate = _device.sampleRate, AudioBlock.cpp:156), so a user can see
the result. Test: test_audio_bad_rate: setupStream(0.0) and
setupStream(-1.0) throw "Invalid argument". A source with 44100.6 posts
rxRate 44101.

### AUDIO-6 "Default Sample Rate" in /devices/audio/info is the first native rate
PothosAudio AudioInfo.cpp:13-27 takes nativeDataFormats[0].sampleRate.
miniaudio has no "default rate" field, so the label is not correct.
Choices: (a) rename the key to "Native Sample Rates" with all different
native rates (an array); (b) open the device to read the rate that
miniaudio selects (slow, can fail when the device is busy).
Recommendation: (a). The key is only shown in the Flow host explorer.
Test: test_audio_info: on the null backend, expect the key "Native
Sample Rates" with an array that is not empty.

---

## VALID

### P3-1 Logger dispatch checks `shutdown` outside queueMutex
Repo PothosCore, lib/System/Logger.cpp:143-153. dispatch() reads the
atomic s.shutdown without the queue lock. shutdownLogging() then sets it,
joins the writer and sets stopWorker = false (Logger.cpp:290-320). The
thread in dispatch() then takes the lock, sees no joinable writer and
starts a new writer thread (Logger.cpp:150) after the shutdown. At
process exit this thread is never joined, and its records can be lost.
Fix: after the lock at Logger.cpp:149, check s.shutdown again. If it is
set, release the lock, call writeToSink(record) and return.
Test: no practical automatic test. The window is a few instructions; a
stress loop of shutdownLogging/restartLogging against log threads cannot
see the new thread without an internal accessor.

### P3-2 Windows restart race after detach (tests only)
Logger.cpp:310-320: on Windows the writer is detached, then stopWorker is
set to false again. If the detached writer did not see stopWorker = true
yet, its wait predicate is false again and it continues as a second
writer. After restartLogging() (only test_logger_after_shutdown,
lib/System/Builtin/TestLogger.cpp:155-163) two writers take records from
one queue, and the order can change. Production calls shutdownLogging()
only at exit.
Fix: a generation counter. workerLoop(gen) stops when s.generation != gen.
shutdownLogging() increments it under queueMutex instead of the reset of
stopWorker.
Test: no practical automatic test (Windows only, timing dependent).

### P3-6 fdMutex/spawnMutex duplicated in libPothos and PothosUtil
pothos_platform is a static library linked into both binaries, so each
has its own lock (lib/Platform/ProcessUnix.cpp:22-26,
lib/Platform/ProcessWindows.cpp:30-34). On Linux (pipe2) and macOS
(POSIX_SPAWN_CLOEXEC_DEFAULT) and glibc >= 2.34 (closefrom) the lock is
not necessary (ProcessUnix.cpp:56-61, 175-181). On Windows it is
necessary: a launch makes the child's pipe ends inheritable and calls
CreateProcessW with bInheritHandles = TRUE (ProcessWindows.cpp:186-195).
A launch from the other binary at the same time gets these handles, and
a pipe reader never sees the end of the stream. Today no code launches
from both copies at the same time (PothosUtil launches only in
--self-tests, after the module load).
Fix: on Windows use STARTUPINFOEXW with PROC_THREAD_ATTRIBUTE_HANDLE_LIST
(only the 3 standard handles). Then the global lock is not necessary.
Test: no practical automatic test (two binaries must launch at the same
time; Windows only).

### P3-7 macOS dylib uses `extern char **environ`
lib/Platform/ProcessUnix.cpp:15 and :149. Apple documents that a shared
library must use _NSGetEnviron() (<crt_externs.h>). It links and works
today (macOS CI is green), so the risk is low.
Fix: `#ifdef __APPLE__` include <crt_externs.h> and use
`*_NSGetEnviron()`.
Test: the existing environment tests on the macOS CI
(test_process_* that pass env) cover it. No new test.

### P3-8a Process is default-constructible with a null impl
lib/Platform/Process.hpp:140-168 has no declared constructor, so
`Process p;` compiles and p.id()/wait() dereference a null _impl. Only
launch() (ProcessUnix.cpp:202, ProcessWindows.cpp:216) uses it now, and
RemoteServer uses std::optional (lib/Remote/Server.cpp:81).
Fix: a private default constructor (launch is a member, so it can use it).
Test: no automatic test needed (a compile-time property; a static_assert
`!std::is_default_constructible_v<Process>` in TestProcess.cpp shows it).

### P3-8b Unix Process::wait() holds the mutex while it blocks
lib/Platform/ProcessUnix.cpp:212-223: the lock is held during a blocking
waitpid(). tryWait(), requestTermination() and kill()
(ProcessUnix.cpp:225-245) take the same lock first. If one thread waits
for a child and another thread calls kill() or requestTermination(),
that call blocks until the child exits, which it never does: a deadlock.
No current caller does this (RemoteServer calls both from one thread,
lib/Remote/Server.cpp:30-40), but the class allows copies across threads
("Copies refer to the same process").
Fix: in wait(), call waitid(P_PID, pid, WEXITED | WNOWAIT) without the
lock, then take the lock and reap with waitpid(WNOHANG) if exitCode is
not set.
Test: test_process_kill_while_waiting: launch PothosUtil with
--self-test1=/app/tests/helper_sleep, call wait() in a std::async, call
kill() from the test thread, expect the future ready within 5 s (RED:
it hangs).

### P3-9 Windows console sink writes UTF-8 through fprintf
lib/System/LoggerSinks.cpp:127-134. On a Windows console with the OEM
or ANSI code page, non-ASCII UTF-8 text (block names, paths) shows as
wrong characters. Poco used WriteConsoleW.
Fix: when stderr is a console (_isatty and GetConsoleMode succeed),
convert the line with utf8ToWide and write it with WriteConsoleW;
otherwise keep fprintf (bytes to files and pipes). Do not call
SetConsoleOutputCP (it changes the shared console of the parent).
Test: no practical automatic test (needs a real console; CI has none).

### P3-10 File sink silently drops records if the file cannot be opened
lib/System/LoggerSinks.cpp:136-143: a std::ofstream that did not open
only sets failbit on each write. With POTHOS_LOG_CHANNEL=file and a bad
POTHOS_LOG_FILE, all logs are lost without a message.
Fix: in makeChannelSink, if not file->is_open(), throw
Pothos::FileException. In configureFromEnvironment, catch it, use the
console sink and log one warning with the path.
Test: test_logger_file_sink_open_error: makeChannelSink("file",
"<temp dir>/no/such/dir/x.log", ...) throws Pothos::FileException.

### P3-11 RemoteServer::Impl default-constructs two unused Pipes
lib/Remote/Server.cpp:48: `Pothos::Platform::Pipe outPipe, errPipe;`.
Pipe() creates a real OS pipe (ProcessUnix.cpp:52-63), so each
RemoteServer makes 4 extra descriptors. Server.cpp:126-127 replaces them,
or they stay unused when closePipes is true.
Fix: std::optional<Pothos::Platform::Pipe> members (set only at
Server.cpp:126-127; ~Impl closes them only when set).
Test: no practical automatic test (a descriptor count is Linux only and
other threads change it).

### P3-13a JIT: std::filesystem exceptions escape catch(Pothos::Exception)
lib/ConfLoader/JITCompilerLoader.cpp:112-121 catches only
Pothos::Exception. compilationHelper (JITCompilerLoader.cpp:57-84) calls
std::filesystem::last_write_time and file_size, and moveFile can throw
from copy_file (lib/Util/FileSystem.hpp:49). create_directories
(JITCompilerLoader.cpp:104) is outside the try. A missing source or a
full disk gives std::filesystem::filesystem_error: no log, no saved
handle->exception, a retry on each call, and callers that catch only
Pothos::Exception do not catch it.
Fix: move create_directories into the try. Add
`catch (const std::exception &ex)` that saves the exception as a
Pothos::FileException (with ex.what()), logs it and throws it.
Test: test_jit_missing_source (in lib/ConfLoader/Builtin): register a
JIT conf whose source does not exist, put a non-empty file at the output
path, call the factory, expect a Pothos::Exception (RED:
filesystem_error).

### P3-13b JIT: stray indentation on the tokSep line
lib/ConfLoader/JITCompilerLoader.cpp:146 has 8 spaces instead of 4.
Fix: re-indent. Test: none needed.

### P3-14 No end-to-end test that argv with spaces/quotes/trailing backslash reaches a child unchanged
Only the quoting unit test exists (test_windows_command_line_quoting,
lib/Platform/Builtin/TestProcess.cpp:23). No test launches a child with
such an argument.
Fix: add the test.
Test: test_process_argv_roundtrip: launch PothosUtil with
`--self-test1=a b "c" d\` (spaces, quotes, trailing backslash).
PluginPath rejects it and echoes it in the error
(lib/Plugin/Path.cpp:16, "Pothos::PluginPath(<path>)"). Expect exit 64
and stderr that contains the exact argument.

### P3-15 test_logger_bad_level_env does not cover the real startup path
lib/System/Builtin/TestLogger.cpp:166-206 calls configureFromEnvironment
directly. test_app_daemon_with_threads sets POTHOS_LOG_LEVEL=bogus but
the daemon output goes to /dev/null. No test checks "one warning" for
load + setupDefaultLogging.
Fix: add the test.
Test: test_app_bad_log_level_once (lib/App/Builtin/TestServer.cpp):
launch PothosUtil --system-info with POTHOS_LOG_LEVEL=bogus and
POTHOS_LOG_CHANNEL=console, read stderr, expect exactly one line with
"invalid POTHOS_LOG_LEVEL".

### P4-M3 Finished connections are reaped only on the next accept
lib/Net/TcpServer.cpp:46-58 and 123-131: reapFinished() runs only in the
accept callback. After a burst of connections, the finished threads stay
joinable (stack and TcpStream kept) until a new client connects or the
server stops. A proxy server can keep many dead threads for a long time.
Fix: at the end of run() (TcpServer.cpp:100-104), post a reap to the
io_context: `asio::post(ioContext(), [self]{lock; self->reapFinished();})`.
The join waits only for a thread that has released the lock.
Test: test_net_tcp_reap_idle (Linux only): connect and close 20 clients,
wait 500 ms without a new connection, expect the count of
/proc/self/task back to the value before the clients (+-1).

### P4-M4 TcpServer with an empty host always binds :: (fails without IPv6)
lib/Net/TcpServer.cpp:149-150. Latent: parseUri rejects an empty host
(lib/Net/Authority.hpp:52), the proxy server uses getWildcardAddr
(apps/PothosUtilProxyServer.cpp:43), and no test passes "".
Fix: an empty host selects ipv6Supported()? address_v6::any() :
address_v4::any() (as SyslogListener, lib/Net/Syslog.cpp:408-419).
Test: test_net_tcp_empty_host: TcpServer("", 0, ...) binds, host() is
"::" when ipv6Supported() else "0.0.0.0", and a client to 127.0.0.1
connects.

### P4-M5 Connect timeout path waits for the io_context thread without a bound
lib/Net/TcpStream.cpp:139-145: after the timeout, the close is posted to
the io_context and result.wait() has no bound. If the io_context thread
is in a long callback, connect() waits for it. If the io_context is
stopped (process exit), the post never runs and connect() hangs forever.
Fix: keep the socket in a std::shared_ptr that the async_connect handler
and the posted close capture. Then the function can throw the
TimeoutException at once, without result.wait(). If
ioContext().stopped(), close the socket directly.
Test: test_net_connect_timeout_busy_io: post a task that blocks the
io_context thread for 3 s, call TcpStream::connect("127.0.0.1", port of
a listening server, 200 ms), expect TimeoutException within 1 s (RED: it
returns only after 3 s).

### P4-M7 SyslogListener::port() reads the socket from the caller thread
lib/Net/Syslog.cpp:450-454 calls local_endpoint() while the io_context
thread uses the socket in async_receive_from (formally a data race on the
Asio socket; practically harmless, getsockname only).
Fix: save the port in the constructor after bind (Syslog.cpp:420) in a
const member, and return it.
Test: no practical automatic test (only TSan can show it).

### P4-M8 Whole datagram size not capped (long source; macOS maxdgram 9216)
lib/Net/Syslog.cpp:306-315: only the text is cut (MaxSyslogMessage =
60000, lib/Net/Syslog.hpp:20). The source is escaped without a limit. A
datagram larger than 65507 bytes, or larger than 9216 bytes on macOS
(net.inet.udp.maxdgram), fails in send_to, and SyslogSender::send drops
it without a message (Syslog.cpp:366-367). On macOS each record with
more than about 9 KB of text is lost.
Fix: cut the source to 255 bytes, and cut the text so that the whole
line is at most MaxSyslogDatagram (8192 on all platforms is the
simplest safe value; a policy choice on the size).
Test: test_net_syslog_long_source: formatSyslog with a 70000-byte source
and text gives at most MaxSyslogDatagram bytes. Extend
test_logger_syslog_forwarding with a 20 KB text that must arrive (cut).

### P4-M11a RFC 3164 TAG may contain spaces
lib/Net/Syslog.cpp:268-271: the TAG ends at the first '[' or ':'. A
message without a TAG, such as "host some text: more", gives the source
"some text". Pothos itself sends only RFC 5424, so this affects only
foreign senders.
Fix: if a space comes before the '[' or ':', use an empty source and
keep all of the rest as the text.
Test: add to the existing syslog parse tests: parseSyslog("<14>Oct  1
12:00:00 host some text: more") gives source "" and text "some text:
more".

### P4-M11b December messages received in January get the new year
lib/Net/Syslog.cpp:283-298 always uses the current year. A message of
Dec 31 23:59 received on Jan 1 gets a time about one year in the future.
Fix: if the result is more than one day after now, use tm_year - 1.
Give parse3164 a `now` parameter (internal) for the test.
Test: test_net_syslog_3164_year: with now = Jan 1 00:00:10 of a year,
"Dec 31 23:59:59" gives Dec 31 of the year before.

### P5-M1 CTRL_CLOSE returns at once: the process ends before a clean shutdown
lib/App/ServerWindows.cpp:17-30: for CTRL_CLOSE_EVENT the handler sets
the event and returns TRUE. Windows then ends the process at once, so
the main thread cannot remove the pid file, flush the logger or stop the
topology. Poco waited in the handler (up to 10 s).
Fix: for CTRL_CLOSE_EVENT only, wait in the handler on a second
"shutdown done" event for at most 5 s (the Windows limit for close).
Set that event when main() returns (a static guard after runGuarded).
Test: no practical automatic test (needs a console window that closes;
Windows only).

### P5-M3a Service: exceptions in serviceMain are swallowed without a log
lib/App/ServerWindows.cpp:123-125: `catch (...) {}`. The service stops
with exit code 1 and no text anywhere.
Fix: catch Pothos::Exception and std::exception, log with
POTHOS_LOG_ERROR and call Logger::flush(), set the exit code 64 (as
runGuarded, apps/PothosUtil.cpp:130-152). A Windows event log entry
(ReportEventW) is better but larger.
Test: extend test_app_windows_service (elevated Windows CI): register a
service with a bad --run-topology path and POTHOS_LOG_CHANNEL=file, start
it, expect the service-specific exit code 64 and the error in the log
file.

### P5-M3b --register-service without a command is accepted
apps/PothosUtil.cpp:170-181 registers any remaining options. A service
without a command runs runCommands() with nothing to do and stops at
once.
Fix: before registerService, require at least one command option in
opts.args (a server mode, isServerMode(), or a command of runCommands);
otherwise throw InvalidArgumentException.
Test: test_app_register_service_no_command (Windows CI, no elevation
needed because the check comes before the service manager): PothosUtil
--register-service=X exits 64 with a message.

### P5-M3c Access denied message could suggest elevation
ServerWindows.cpp:71-75, 133-134, 169-174 give only the system text
("Access is denied.").
Fix: in throwLastError, if the error is ERROR_ACCESS_DENIED, add "run
PothosUtil as an administrator".
Test: no practical automatic test (the CI runner is elevated).

### P5-M4a unregisterService does not stop a running service first
ServerWindows.cpp:167-175: DeleteService only marks the service. It
stays and runs until the next stop or reboot.
Fix: open with SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE, send
SERVICE_CONTROL_STOP, wait up to 10 s for SERVICE_STOPPED, then
DeleteService.
Test: extend test_app_windows_service: start the service, call
--unregister-service, expect OpenServiceW to fail with
ERROR_SERVICE_DOES_NOT_EXIST within 15 s.

### P5-M4b STOP_PENDING/STOPPED race when the commands end by themselves
onServiceControl (ServerWindows.cpp:104-116) reports STOP_PENDING from
the dispatcher thread. serviceMain (ServerWindows.cpp:118-127) can report
STOPPED first. A later STOP_PENDING after STOPPED is not allowed by the
service rules.
Fix: a mutex and a "stopped" flag in reportState: no report after
SERVICE_STOPPED.
Test: no practical automatic test (timing dependent).

### P5-M5b PidFile errors after daemonizing go to /dev/null
lib/App/ServerUnix.cpp:57-82: the original process returns 0 right after
fork. The daemon writes the pid file (apps/PothosUtil.cpp:211-212). An
error goes to runGuarded, which prints to stderr (now /dev/null), and the
daemon exits 64. The user sees exit 0 and has no daemon and no message.
Fix (smallest): before spawnDaemon, the original process checks that the
pid file can be created (open the .tmp name for writing, then remove
it), and throws a FileException on error. Better: a status pipe from the
daemon to the original process.
Test: test_app_daemon_pidfile_error (Unix): PothosUtil --daemon
--pidfile=/no/such/dir/x.pid --proxy-server=tcp://127.0.0.1:0 exits 64
and stderr contains the path (RED: exit 0).

### P5-M7 --num-trials=0 runs nothing (silent pass for --self-test1)
apps/PothosUtilSelfTests.cpp:134-157: the loop runs 0 times, and
--self-test1 returns the success code. For --self-tests, every test
fails (numOk == 0, line 101). Poco had an IntValidator range.
Fix: in selfTestOne and selfTests, throw InvalidArgumentException when
num-trials < 1.
Test: extend test_app_pothosutil_cli: --self-test1=/platform/tests/test_env_missing
--num-trials=0 exits 64.
Side note (upstream behavior, not in the finding): the parent runs N
child processes and passes --num-trials N to each, so a test runs N*N
times (PothosUtilSelfTests.cpp:71-76), and one good trial is enough to
pass (line 101).

### FLOW-1 ConnectionPropertiesPanel: itemSelectionChanged of child trees can call a slot during destruction
PothosFlow PropertiesPanel/ConnectionPropertiesPanel.cpp:45, 50, 60
connect the child QTreeWidget signals to this. The class has no
destructor. If a child emits itemSelectionChanged while ~QWidget deletes
the children, handleItemSelectionChanged (line ~127) runs on a partly
destroyed object and uses _outputListWidget, which can be deleted
already. Not reproduced: the panel is deleted on each selection change
(PropertiesPanelDock.cpp:67, 115) without failure, and Qt does not send
ChildRemoved during deleteChildren. A Qt debug build would assert
("Called object is not of the correct type",
/usr/include/qt6/QtCore/qobjectdefs_impl.h:107).
Fix: add `~ConnectionPropertiesPanel() override` that calls
`_outputListWidget->disconnect(this)` (and the same for the input and
connections lists).
Test: no practical automatic test with a release Qt. With a debug Qt or
ASan: a test that makes the panel, selects items in both lists and
deletes it.

### FLOW-2 AffinityZonesDock: currentChanged of _editorsTabs during destruction
PothosFlow AffinitySupport/AffinityZonesDock.cpp:189 and 201-205. If
QTabWidget emits currentChanged while the dock is destroyed, the slot
writes "AffinityZones/currentZone" to the settings with the text of the
wrong tab (or ""), so the saved zone changes at exit. Not reproduced
(the dock lives as long as the main window).
Fix: a destructor that calls `_editorsTabs->disconnect(this)`.
Test: no practical automatic test with a release Qt. The flow smoke test
could check that "AffinityZones/currentZone" has the same value before
and after the main window closes.

### FLOW-3 BlockPropertiesPanel: currentChanged of _infoTabs during destruction
PothosFlow PropertiesPanel/BlockPropertiesPanel.cpp:144 and 346-366: the
slot uses _jsonBlockDesc, _evalTypesDesc and _block. Same hazard and same
"not reproduced" evidence as FLOW-1 (this panel is deleted on each
selection change).
Fix: a destructor that calls `_infoTabs->disconnect(this)`.
Test: as FLOW-1.

### AUDIO-1a readFrames/writeFrames return no count; produce/consume use numFrames
PothosAudio AudioBlock.cpp:337-373 and 375-407 can stop early (line 346,
384), but AudioSource.cpp:135 produces numFrames and AudioSink.cpp:123
consumes numFrames. The int8 conversion (AudioBlock.cpp:363-372) also
uses numFrames. Latent: work() is the only reader (source) or writer
(sink) of the ring buffer, so the available count only grows, and a short
read cannot occur today.
Fix: return `done`, and produce/consume that count.
Test: no practical automatic test (a short transfer cannot occur with a
single consumer ring buffer).

### AUDIO-1b A std::vector for each chunk in PORTPERCHAN mode
AudioBlock.cpp:354 and 392 allocate in work() for each chunk (up to 2
for each call).
Fix: member vectors of size _numChans, sized in the constructor.
Test: no practical automatic test (performance only).

### AUDIO-3 deactivate() without an initialized device throws
AudioBlock.cpp:226-234 calls ma_device_stop on a device that is not
initialized and throws. Latent through the framework: WorkerActor does
not call deactivate() when activate() failed
(PothosCore lib/Framework/WorkerActor.cpp:189-213). It can occur when a
direct call to setupStream() on an active block fails after closeStream().
Fix: `if (not _deviceInit) return;` at the start of deactivate() (and of
stopDeviceForTest()).
Test: no practical automatic test through a topology. A direct C++ test
that makes the block without setupStream and calls deactivate() shows
it.

### AUDIO-4b sleep_for(1 ms) is about 15.6 ms on Windows
AudioBlock.cpp:325-335: waitAvailable polls with sleep_for(1 ms) up to
MAX_WORK_WAIT (10 ms). On Windows with the default timer resolution one
sleep is about 15.6 ms. The sink can then be late by about 2 periods,
which is the size of the new pre-fill margin (AudioBlock.cpp:178).
Fix: on Windows, timeBeginPeriod(1) in activate() and timeEndPeriod(1)
in deactivate() (winmm).
Test: the Windows CI run of test_audio_loop_graph (it counts sink
underflows) shows it; no separate test.

### AUDIO-5 _xrun and _xrunFrames are two exchanges
AudioSource.cpp:113-115 and AudioSink.cpp:108-110 exchange _xrun, then
_xrunFrames. The callback (AudioBlock.cpp:272-279, 311-319) can add the
frames of a new event between the two, so the count goes to the wrong
message, and the next message shows 0 or too few frames. Only the text
is wrong.
Fix: one std::atomic<std::uint64_t>: bit 63 = new event, low bits =
frames. The callback does one fetch_add/fetch_or. work() does one
exchange(0).
Test: no practical automatic test (a window of a few instructions; the
null backend cannot force it).

### AUDIO-7 PothosDoc text of the source/sink says "default output/input device" swapped
AudioSource.cpp:27 "default output device" (must be input);
AudioSink.cpp:22 "default input device" (must be output). From upstream.
Fix: swap the two words.
Test: none needed (doc text).

### AUDIO-8 test_audio_stop_without_data does not test a device without data
TestAudioBlocks.cpp:249-260: the null device always gives frames, so
the test only checks that a running source stops fast. The original risk
(a blocking read with no data) is gone by design: waitAvailable waits at
most 10 ms (AudioBlock.cpp:17, 325-335).
Fix: a test-only flag (as stopDeviceForTest) that makes captureCallback
discard the input, set before commit. Or rename the test to
test_audio_source_stop_fast and state the reason.
Test: with the flag: commit, wait 200 ms, destroy the topology, expect
less than 1 s, and the collector got 0 frames.
