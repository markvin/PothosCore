# Phase 18: the deferred minor findings — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the 60 VALID deferred findings and do the 18 DECISION items
of the triage, in the core and in the toolkits, each with a test that
failed first when a practical test exists.

**Architecture:** One task for each area. Each item follows its entry in
the appendix (the evidence, the fix, the test idea). The core goes first
(one PR), then each toolkit (one PR each), then PothosBase and the core
docs.

**Tech Stack:** C++23, CMake ≥ 3.25, Asio, nlohmann_json, xsimd, Qt 6
(flow), miniaudio (audio), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-31-phase18-minors-design.md`
(decisions) and `docs/superpowers/specs/2026-10-31-phase18-minors-triage.md`
(appendix: evidence, fix and test for each id).

## Global Constraints

- The decisions of the spec are binding. A VALID item that needs a public
  API change or a layout change not in the spec: stop and ask the user.
- No ABI change (`2.0-0`); no change of a public class layout.
- Each behavior fix gets a test that failed first (TDD), when the
  appendix gives a practical test. Else write "no automatic test: <why>"
  in the ledger.
- Core branch `phase18-minors` (spec `fa8c9f92`). Toolkit branches
  `phase18-minors` from their `pothos2`. Audio only after the phase 17 PR
  is merged.
- English text in ASD-STE100; no "Generated with Claude Code" or
  `Co-Authored-By`; no home directory paths in committed text.
- Approval points: each push, PR and merge.
- Ledger: `.superpowers/sdd/2026-10-31-phase18-minors/progress.md`.

## Review Focus

- A developer shell with `POTHOS_PLUGIN_PATH`, `POTHOS_LOG_LEVEL=none` or
  `POTHOS_SYSLOG_ADDR` set: every self-test runner of the core and the
  toolkits must give the same result as a clean shell. Test: Task 1
  extends `tests/test_env_guard.cmake`; Task 10 checks one toolkit copy.
- A stage directory with files of an older build: the stage tests must
  not load them. Test: Task 1 `pothos.stage` cleanup check.
- An ARM computer: the SIMD dispatch must pick the NEON copy. Test: Task 3
  `test_simd_copy_keys_reachable` (all keys of the copies are names that
  detection can report).
- A connect to a host that resolves to IPv6 and IPv4 where only one
  family listens (blocks network CONNECT, core TcpStream): it connects.
  Test: Task 10 (blocks) with a listener on `127.0.0.1` only and the
  host `localhost`.
- A Windows user with a non-ASCII user name (temporary directory and
  config paths): the core and the file blocks open their files. Test:
  Task 6 and Task 10 self-tests with a non-ASCII temporary directory
  (Windows CI).

---

### Task 1: Core test environment and stage (P1-1, P1-2, P6b-4)

**Files:** `cmake/PothosTestEnv.cmake`, `tests/test_env_guard.cmake`,
`CMakeLists.txt` (the `pothos.stage` test), a new
`cmake/PothosStageClean.cmake` if needed.

- [ ] Step 1: extend `test_env_guard.cmake`: with `POTHOS_PLUGIN_PATH`,
  `POTHOS_CONF_PATH`, `POTHOS_LOG_LEVEL`, `POTHOS_LOG_CHANNEL`,
  `POTHOS_LOG_FILE`, `POTHOS_LOG_FORMAT`, `POTHOS_LOG_TIMEZONE`,
  `POTHOS_LOG_ASYNC` and `POTHOS_SYSLOG_ADDR` set before
  `pothos_test_env()`, the variables are unset after it (except
  `POTHOS_PLUGIN_PATH` when the caller sets it again). Run: fails.
- [ ] Step 2: unset them in `pothos_test_env()`. Run: passes.
- [ ] Step 3: a check that a file in `build/stage` from an older install
  (a module not in the current `install_manifest.txt`) is removed by
  `pothos.stage`. Run: fails.
- [ ] Step 4: `pothos.stage` removes the files of the previous
  `install_manifest.txt` of the stage (not the whole directory: toolkits
  can be installed in it), then installs. Run: passes; full `ctest`.
- [ ] Step 5: commit.

### Task 2: Core CMake and CI hygiene (P1-5, P1-7, P1-8, P1-9, P1-10, P6a-1, P6a-2, P6a-4, P6a-5, P6a-6, P7-2)

Each item as in the appendix. Tests:
- P1-7: a configure check (CTest `pothos.config_checks` or the existing
  subproject test) with `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0` on APPLE
  only; on other systems the check is not registered.
- P1-10: `pothos.consumer` fails when `INTERFACE_COMPILE_OPTIONS` or
  `INTERFACE_LINK_OPTIONS` of `Pothos::Pothos` names a private
  dependency (check with a deliberate temporary bad value, then remove
  it).
- P6a-1, P6a-2, P6a-5, P6a-6: extend `pothos.subproject` (a vendored core
  without `.git` gives the version without a hash; a parent with an
  installed Pothos plus `add_subdirectory(core)` configures; the make
  program and `-A`/`-T` are forwarded; `-Werror=dev` in the consumer and
  api_compat test projects).
- P6a-4: `POTHOS_INSTALL_RPATH_USE_LINK_PATH` option (default ON) in
  place of the hard-wired value; a configure test with OFF.
- P7-2: a configure error when `POTHOS_BUILD_APPS=OFF` and a module uses
  `ENABLE_DOCS` (in `PothosUtil.cmake`); a configure test.
- P1-5, P1-8, P1-9: no automatic test (dead variables, CI grep scope,
  CI `permissions`/`concurrency`); the review checks them.
- [ ] Steps: test first where listed, fix, full `ctest`, commit.

### Task 3: SIMD (P5a-1, P5a-2, P5a-3, P8-3)

- [ ] `test_simd_copy_keys_reachable`: every key of the generated copies
  is a name that `detectSIMDFeatures` can report on its architecture.
  Fails (ARM keys, `x86_xop`). Fix: report the key names on ARM; do not
  generate never-dispatched or duplicate copies. Passes.
- [ ] P5a-3: extend `test_simd_convert_unaligned` to all conversion pairs
  and all runnable copies.
- [ ] P8-3: the `nm` guard (Linux, GCC and Clang) as in the spec; the
  rule in the `XSIMDCopy.hpp` comment.
- [ ] Full `ctest`, commit.

### Task 4: Expressions (P2-1, P2-2, P2-3, P2-4, P2-5, P2-6, P2-7)

- [ ] Tests in `TestExpr.cpp`/`TestEvalExpression.cpp` first:
  `0.<400 zeros>1` gives 0.0 and `1<400 zeros>.0` gives inf (P2-1); a
  deeply nested value through constants gives an error, not a crash
  (P2-2); `registerConstant("true", …)` and the 5 other names throw
  (P2-3); `round(1, -19)` is an overflow error (P2-4); `str(1e5) ==
  "100000.0"`, `str(1e16) == "1e+16"`, `str(1e-5) == "1e-05"`,
  `str(0.0001) == "0.0001"` (P2-6); a `std::exception` from a function
  becomes a `Pothos::Exception` (P2-7). Run: they fail.
- [ ] Fixes as in the appendix and the spec; P2-5: the Doxygen of
  `EvalEnvironment.hpp` says that strings are UTF-8 bytes. Run: pass.
- [ ] Commit; Changelog lines for P2-3 and P2-6.

### Task 5: Logger (P3-1, P3-2, P3-3, P3-9, P3-10, P3-15, P3-4 doc, P3-12 doc, `Logger.hpp` default level)

- [ ] Tests first: `POTHOS_LOG_LEVEL=none` gives no record to the sink
  and the listeners (P3-3); a file sink with a path that cannot be opened
  logs one warning on stderr and does not drop silently (P3-10, decide
  the check from the appendix); the startup path with a bad level (P3-15).
- [ ] Fixes as in the appendix: `shutdown` checked again under
  `queueMutex` (P3-1, no automatic test: a race at exit); the Windows
  restart race (P3-2); the Windows console sink writes UTF-8 with the
  wide API (P3-9); the docs (P3-4, P3-12, default level).
- [ ] Full `ctest`, commit; Changelog for P3-3.

### Task 6: Platform and process (P3-6, P3-7, P3-8a, P3-8b, P3-11, P3-14, NUL handle, P8-4 core)

- [ ] Tests first: `wait()` in a thread and `kill()` from another thread
  ends within 5 s (P3-8b); arguments with spaces, quotes and a trailing
  backslash reach a child unchanged (P3-14); a default-constructed
  `Process` throws a clear error (P3-8a); a non-ASCII temporary directory
  works for the paths of the core (P8-4, runs on all systems, needed on
  Windows).
- [ ] Fixes as in the appendix: one `fdMutex`/`spawnMutex` (P3-6),
  `_NSGetEnviron` on macOS (P3-7), no unused pipes in
  `RemoteServer::Impl` (P3-11), the NUL handle in the lock, UTF-8 paths
  on Windows (P8-4).
- [ ] Full `ctest`, commit.

### Task 7: Network (P4-M3, P4-M4, P4-M5, P4-M7, P4-M8, P4-M9, P4-M11a, P4-M11b)

- [ ] Tests first: a connect timeout with a blocked io thread throws
  within 1 s (P4-M5); an empty host binds IPv4 when IPv6 is off (P4-M4,
  where the test can force it, else review); a long source is cut so
  that the datagram fits 9216 bytes (P4-M8); an RFC 3164 TAG with a
  space (P4-M11a); a December message received in January keeps the old
  year (P4-M11b); an unresolvable `POTHOS_SYSLOG_ADDR` gives a warning
  and PothosUtil exits 0 (P4-M9).
- [ ] Fixes as in the appendix; P4-M3 (reap finished connections at
  once) and P4-M7 (`port()` stored at bind) with the review.
- [ ] Full `ctest`, commit.

### Task 8: PothosUtil, services, JIT and docs (P5-M1, P5-M3a/b/c, P5-M4a/b, P5-M5b, P5-M7, `--num-trials`, P3-13a/b, P7-1)

- [ ] Tests first: `--daemon --pidfile=/no/such/dir/x.pid` exits 64
  (P5-M5b, Unix); `--self-test1 X --num-trials=0` is an error (P5-M7);
  `--num-trials=2` runs a test 2 times (count the output); the JIT loader
  logs a `std::filesystem` error and saves it (P3-13a).
- [ ] Fixes as in the appendix; the Windows service items (P5-M1, P5-M3,
  P5-M4) with the Windows CI service test; P3-13b format; P7-1 in the
  README and Changelog.
- [ ] Full `ctest`, commit.

### Task 9: Core review and PR

- [ ] Fresh review of the branch (most capable model) with the spec, the
  appendix and the Review Focus; fix Critical/Important with tests.
- [ ] Ask the user; push `phase18-minors`, PR into `pothos2`; CI green;
  merge on the word of the user.

### Task 10: PothosBlocks (P8-1, P8-2, P8-4 file blocks, P1-1 copy)

- [ ] Tests first: a network endpoint with a bad URI does not leak (a
  `unique_ptr` guard; ASan locally if available, else review) (P8-1);
  `tcp://localhost:<port>` connects to a listener on `127.0.0.1` only
  (P8-2); the file blocks write and read a file in a non-ASCII directory
  (P8-4).
- [ ] Fixes; the `PothosTestEnv.cmake` copy from Task 1.
- [ ] Branch, commit, PR (ask), CI green, merge on the word of the user.

### Task 11: PothosFlow (FLOW-1, FLOW-2, FLOW-3, FLOW-4, P1-1 copy)

- [ ] FLOW-1..3: the destructors disconnect the child signals (the
  `GraphDraw` fix of phase 15); a smoke-test step that opens and closes
  the block properties panel and the affinity zones dock, if the smoke
  test can do it, else review.
- [ ] FLOW-4: the clear message and no `spawnNewServer`; test: the
  runner starts a `PothosUtil --proxy-server` on the locator port and
  the smoke test expects the message (or a unit check of the message
  function).
- [ ] The `PothosTestEnv.cmake` copy. PR as in Task 10.

### Task 12: PothosAudio (AUDIO-1a, 1b, 2, 3, 4b, 5, 6, 7, 8, P1-1 copy)

After the phase 17 PR is merged.
- [ ] Tests first: `setupStream(0)` and `setupStream(-1)` throw;
  44100.4 gives 44100 (AUDIO-2); `deactivate()` without `setupStream` does
  not throw (AUDIO-3); `/devices/audio/info` has "Native Sample Rates"
  (AUDIO-6).
- [ ] Fixes: counts from `readFrames`/`writeFrames` (1a), a member buffer
  (1b), a shorter wait on Windows (4b, with `timeBeginPeriod` or a
  condition), one atomic for the xrun count (5), the doc text (7), the
  test name (8). The `PothosTestEnv.cmake` copy. PR as in Task 10.

### Task 13: The other toolkit copies of the test environment (P1-1)

- [ ] PothosComms, PothosWidgets, PothosSoapy, PothosPlotters: copy the
  Task 1 `PothosTestEnv.cmake`; one PR each (ask); CI green.

### Task 14: PothosBase and core docs

- [ ] PothosBase: move the pointers (core and the changed toolkits) and
  the `PothosTestEnv` use of the base runner; `ctest -j4` and the
  configure checks; PR (ask).
- [ ] Core docs: roadmap row 18 and status; PR (ask).
