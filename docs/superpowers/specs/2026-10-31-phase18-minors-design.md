# Phase 18: the deferred minor findings

Date: 2026-10-31. Status: design. The work happens in the core, in
PothosBlocks, PothosFlow and PothosAudio, and (for one item) in the
test-environment copies of all toolkits.

## Intent

The reviews of phases 1–17 gave about 90 minor findings that the phases
did not fix ("minor (deferred)" in the ledgers, the phase 8 PR, the
phase 15 and 17 ledgers). The user decided (2026-10-31): "triage them
yourself and fix all of them". The user does not make the individual
decisions; the designer makes them, and this document lists them for a
check.

A read-only triage checked each finding against the code of 2026-10-31.
The appendix `2026-10-31-phase18-minors-triage.md` gives the evidence
(file:line), the smallest fix and a test idea for each item. Its ids are
used below (P1-n, P2-n, P3-n, P4-Mn, P5-Mn, P5a-n, P6a-n, P6b-n, P7-n,
P8-n, FLOW-n, AUDIO-n).

When this phase is complete, every VALID item is fixed (with a test when
a practical test exists), every DECISION item is done as decided below,
and the OBSOLETE items are closed. The release 2.0.0 can then come
without known open findings.

## Decisions of the user

1. Triage all deferred findings and fix all of them; the designer decides
   (2026-10-31).

## Result of the triage

- OBSOLETE (13, closed, no work): P1-3, P1-4, P1-6, P1-11, P1-12 (Poco,
  `<ciso646>`), P6a-3, P6b-1, P6b-2, P6b-5, P7-3, P3-5, P5-M6, AUDIO-4a.
- VALID (60): fix. The list is in the appendix; the plan groups them.
- DECISION (18): the decisions follow.

## Decisions of the designer (the user must check them)

Core, build:

- P1-7 macOS deployment target: no default in CMake. A configure error
  when `CMAKE_OSX_DEPLOYMENT_TARGET` is set and lower than 13.3 (the
  core uses `std::format` with chrono, which needs macOS 13.3).
- P5a-4 AVX-512F-only copy: close. The AVX2 copy is correct on those
  processors.
- P6b-3 `SpinLock::unlock` calls `notify_one`: close. A fix changes the
  layout of a public class; the cost is small in libstdc++ and libc++.
- P8-3 SIMD copies and the link order of inline `std::` functions: accept
  the link order (the fallback copy is linked first). Document the rule
  in `XSIMDCopy.hpp`, and add a CTest guard on Linux (GCC and Clang) that
  checks with `nm` that the copies of the core define no weak `std::`
  symbols other than in the fallback copy.

Core, runtime:

- P3-3 `POTHOS_LOG_LEVEL=none`: accept `none` again. It turns logging off
  (no record reaches the sink or the listeners). Document it.
- P3-4 Error level is asynchronous: keep it (the header documents that
  only Fatal and Critical are synchronous). Close.
- P3-12 Windows `expandPath` with an undefined `%VAR%`: keep the empty
  text and document it. Close.
- P4-M9 syslog address resolved once: keep, but a failure to resolve
  `POTHOS_SYSLOG_ADDR` is a warning on stderr, not an exit code 64 of
  every PothosUtil command.
- P4-M10 read errors set eof/fail, not badbit: keep (file streams do the
  same; the header documents it). Close.
- P5-M8 `--help` with an invalid option exits 64: keep. Close.
- P7-1 a remote host with a 0.7 core: not supported. Write this in the
  README and the Changelog of the core (the remote protocol of pothos2 is
  not compatible with 0.7).
- P8-4 Windows paths that are not ASCII: "UTF-8 everywhere". On Windows,
  the core converts each `std::string` path to `std::filesystem::path`
  from UTF-8 (not from the ANSI code page), and the file blocks of
  PothosBlocks open files with the wide API. A self-test on Windows
  uses a temporary directory with a non-ASCII name.

Expressions:

- P2-3 user constants can shadow `true`, `false`, `null`, `True`,
  `False`, `None`: reject these names in `registerConstant*` with an
  error.
- P2-5 strings are byte strings: keep and document (UTF-8 bytes; `len`
  counts bytes).
- P2-6 `str()`/`repr()` of floats: the Python repr rule (the shortest
  round-trip digits; fixed notation for an exponent from −4 to 15 with a
  `.0` for a whole number; else scientific with at least 2 exponent
  digits, for example `1e-05`, `1e+16`).

Toolkits:

- FLOW-4 a second PothosFlow when one runs: keep the failure (the same
  as upstream), but show a clear message ("Another PothosFlow or Pothos
  server uses port 16415 …") in place of the raw `RemoteServer` error,
  and remove the dead variable `spawnNewServer`.
- AUDIO-2 the sample rate: reject a rate of 0 or lower (or not finite)
  with an exception; round the others (`std::lround`).
- AUDIO-6 "Default Sample Rate" in `/devices/audio/info`: replace it with
  "Native Sample Rates" (the list of the rates of the native formats).

Side findings of the triage (VALID, added):

- The Doxygen of `Logger.hpp` says that the default level is "notice";
  the code uses "information". Fix the comment.
- `--num-trials=N` runs each test N×N times (the parent loops and passes
  the value to the child). Run N times.
- Windows: the NUL handle is closed outside `spawnMutex`
  (`ProcessWindows.cpp`). Close it inside the lock.

## Goals

- Each VALID item of the appendix is fixed in its repository.
- Each fix that changes behavior has a test that failed before the fix,
  when a practical automatic test exists. For the others, the plan says
  why there is no test, and the review checks the code.
- The test environment (`PothosTestEnv.cmake`) unsets the developer
  variables in the core and in all toolkit copies (P1-1).
- CI is green for every changed repository; PothosBase moves its pointers
  and stays green.
- The ledgers of the old phases are not changed; the Changelog of each
  repository records the user-visible fixes.

## Non-goals

- No new features. No change of the public API of the core except where
  a decision above says so (P2-3, P3-3).
- No change of the ABI version (2.0-0): no public class layout changes
  (this is why P6b-3 is closed).
- python is not in this phase.

## Anti-goals

- A fix that changes a public behavior that the decisions above do not
  name.
- A test that is weakened to pass.
- A Windows-only or macOS-only fix that has no CI job that builds it.

## Constraints

- The rules of `CLAUDE.md` of the core and of each toolkit (C++23,
  CMake 3.25, no warnings, ASD-STE100, no download).
- Each repository gets its own PR. The order: core first (the toolkits
  copy the test environment from it), then blocks, flow, audio and the
  other toolkits, then PothosBase, then the core docs.
- The user approves each push, PR and merge (the user said "merge when
  green" for the open PRs of phase 17 only).

## Left to the builder

- The grouping into tasks and commits, and the exact tests (the appendix
  gives a test idea for each item).
- When a VALID item turns out to be a larger change than the appendix
  says, or a fix needs a public API change that is not decided above:
  stop and ask the user.
