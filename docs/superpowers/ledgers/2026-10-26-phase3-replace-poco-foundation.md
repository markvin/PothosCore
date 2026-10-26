# SDD ledger — plan: docs/superpowers/plans/2026-10-26-phase3-replace-poco-foundation.md

Spec: docs/superpowers/specs/2026-10-26-core-modernization-design.md
Branch: phase3-replace-poco (from pothos2 @ 06d1ad01)

## Pre-flight
- T1→T2..T9: every task shrinks cmake/poco-allowlist.txt produced by T1; the test fails if an allowlisted file stops using Poco. OK.
- T2→T3..: StringUtils (Pothos::Util::Detail) consumed by later tasks replacing StringTokenizer leftovers. OK.
- T4 uses home dir lookup before T5 Platform::getEnv exists — plan says temporary std::getenv. OK.
- T5→T6: Process uses Platform env merge; currentProcessId from T5. OK.
- T5→T7: test_logger_bad_level_env uses Platform::setEnv. OK.
- T6→T7: Remote/Server.cpp pipeToSyslogWorker reads Platform Pipe (T6) and logs via Logger::log (T7). T6 must keep the syslog worker compiling with Poco channel until T7 — order OK.
- T7→T9: syslog adapter stays in allowlist (Net). OK.
- T8: drops PocoXML link; must ensure no other file uses Poco/XML or DOM (only TopologyDotMarkup). OK.
Task 1: RED 90 violations (89 lib/apps + include/Pothos/Exception.hpp) = grep count; allowlist populated with 89 files.
Task 1: Ruling: platform-only files (SharedBufferWindows.cpp, NumaInfoOSX.cpp, WindowsGetLogicalProcessorInfo.cpp, MsvcCompilerSupport.in.cpp, ...) are scanned on every OS and must be converted too, but they are not compiled on Linux — verification for them is CI (windows/macos jobs) — cost if wrong: a CI round-trip per mistake.
Task 1: complete (commits 06d1ad0..7c98af0, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  16.83 sec)
Task 2: RED = build failure "Util/StringUtils.hpp: No such file" → GREEN 4/4 string tests.
Task 2: Ruling: StringTokenizer semantics preserved per call site — no flags = split(s, sep, false, false); TOK_TRIM = split(..., true, false); TOK_TRIM|TOK_IGNORE_EMPTY = split defaults — cost if wrong: path/conf parsing edge cases (empty segments).
Task 2: Ruling: POCO_LONG_IS_64_BIT → `#if LONG_MAX == INT_MAX` (32-bit long, e.g. Windows) — cost if wrong: DType alias "long" maps to wrong width on some platform.
Task 2: Ruling: poco_bugcheck_msg in ServerHandler → throw Pothos::AssertionViolationException("ServerHandler","unknown action "+action) — cost if wrong: none (unreachable branch).
Task 2: Ruling: Poco formatHex is upper case → std::format "{:X}"/"{:02X}" — cost if wrong: hex case in docs/device info strings.
Task 2: Ruling: PothosUtil gets PRIVATE include dir ${PROJECT_SOURCE_DIR}/lib to share lib/Util/StringUtils.hpp — cost if wrong: none (internal, not installed).
Task 2: Ruling: PothosCheckPoco.cmake makes SRC absolute (relative SRC silently matched nothing) — cost if wrong: none.
Task 2: WindowsGetLogicalProcessorInfo.cpp edited but not compiled on Linux — CI.
Task 2: complete (commits 7c98af0..484a39d, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  16.80 sec)
Task 3: RED baseline: 81 Poco::format calls in 33 files (+7 files with an unused <Poco/Format.h> include). Converted by script (literal format strings; %s/%d/%z -> {}, %f -> {:f}, braces escaped); compile-time checked by std::format. 0 -Wformat warnings. 17 files freed from allowlist.
Task 3: Ruling: SIMDDispatcherUtils formatStr: `static const std::string` -> `static constexpr char[]` so std::format checks it at compile time (instead of std::vformat) — cost if wrong: none.
Task 3: Ruling: plan Step 1 "RED" is a baseline count, not a failing test (pure refactor; self-tests are the regression guard) — cost if wrong: none.
Task 3: complete (commits 484a39d..9bd44e5, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  17.14 sec)
Task 4: RED = build failure "Util/TempFile.hpp: No such file"; later RED for new helpers (globMatch/uniqueTempPath undeclared, FileSystem.hpp missing) → GREEN: test_temp_file, test_unique_temp_path, test_glob_match, test_expand_path.
Task 4: Ruling (plan gap): Poco::Path::expand had no plan entry — added Util/FileSystem.hpp expandPath (~, $VAR, ${VAR}; %VAR% on Windows) + test_expand_path — cost if wrong: conf/CLI paths with env vars.
Task 4: Ruling (plan gap): Poco::Glob in PothosUtilSelfTests matches plugin path strings, not files — added StringUtils globMatch (*, ?, [set], [!set]) + test, instead of directory iteration — cost if wrong: --self-tests glob filters.
Task 4: Ruling: TempFile gets `create` flag and uniqueTempPath(ext) — SharedBufferUnix opens with O_EXCL (needs a non-existing path), Compiler::createTempFile needs a name only (as Poco tempName) — cost if wrong: none.
Task 4: Ruling: json/conf paths now expandPath() BEFORE makeAbsolute (Poco order was makeAbsolute then expand, which breaks "$HOME/x") — cost if wrong: a conf relying on the old order (unlikely).
Task 4: Ruling: canAccess(): POSIX access(), Windows exists + owner_write perm — same as Poco semantics per platform — cost if wrong: misreported writability on Windows ACLs.
Task 4: Ruling: ModuleLoader.cache mtime string now from file_clock epoch (differs from Poco epochMicroseconds) — existing caches re-validate once — cost if wrong: one extra safe-load per module.
Task 4: Ruling: moveFile = rename, fallback copy+remove (Poco moveTo copied+removed) — cost if wrong: none.
Task 4: Ruling: FileRealPath (only a Poco comment) rewritten to std::filesystem::canonical (realpath with fixed FILENAME_MAX buffer before) — cost if wrong: none.
CI: macOS (Intel) build of 06d1ad01 failed only at JITCompilerLoader.cpp: std::exception_ptr incomplete (missing <exception> under libc++); fixed 07e8a0e on phase3, cherry-picked to pothos2 as a1ae6a3 and pushed.
Task 4: complete (commits 9bd44e5..07e8a0e, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  17.48 sec)
Task 5: RED = "Platform/Environment.hpp: No such file"; nodeId RED = link error → GREEN test_env_roundtrip, test_env_missing, test_host_info, test_node_id. 7 files freed; self-tests 100/100.
Task 5: Ruling (plan gap): added Platform::nodeId() (Poco::Environment::nodeId, MAC of first non-loopback adapter; used by HostInfo.nodeId, UID, TopologyNetworkFlows same-host check) — first adapter in NAME order (Poco used enumeration order) so all processes agree; fallback nodeName() when no adapter; Windows links iphlpapi — cost if wrong: same-host detection between processes (Remote/network flows).
Task 5: Ruling: UID string built with std::format "pothos://node/nodeId/pid/n" instead of Poco::URI (equivalent text) — cost if wrong: UID format differs (opaque id, no parser).
Task 5: Ruling: CompilerStdFlags POCO_COMPILER_* → _MSC_VER / __GNUC__||__clang__ — cost if wrong: none.
Task 5: Windows-only code (EnvironmentWindows.cpp, Paths.in.cpp WIN32 branch, Module.cpp, Logger.cpp WIN32) not compiled locally — CI.
Task 5: complete (commits 2e046bc..7749b23, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  17.93 sec)
Task 6: RED = configure error (CommandLine.cpp/headers missing) → GREEN 8 platform tests incl. test_process_large_output (4096 lines/256 KiB, no deadlock) and test_windows_command_line_quoting; self-tests 109/109.
Task 6: Ruling: dlopen flags RTLD_LAZY|RTLD_GLOBAL (Poco SharedLibrary default) instead of plan's RTLD_NOW — module interdependency behavior unchanged — cost if wrong: none.
Task 6: Ruling: DynamicLibrary::suffix() added (plan said keep out of class) — JIT and compiler temp outputs need it (was Poco::SharedLibrary::suffix) — cost if wrong: none.
Task 6: Ruling: platform sources compiled into BOTH libPothos and PothosUtil (POTHOS_PLATFORM_SOURCES/LIBRARIES cache vars) — libPothos hides internal symbols; a static lib would break POTHOS_API dllexport/dllimport on Windows — cost if wrong: duplicated code in PothosUtil.
Task 6: Ruling: Windows child-handle inheritance: pipes created non-inheritable, child ends made inheritable only inside a spawn mutex then cleared — no PROC_THREAD_ATTRIBUTE_HANDLE_LIST — cost if wrong: handle leak to concurrently spawned non-Pothos children.
Task 6: Ruling: Windows requestTermination signals named event POCOTRM<pid> (PothosUtil is a Poco ServerApplication until phase 5), fallback TerminateProcess — cost if wrong: remote server killed hard on Windows.
Task 6: Ruling: removed manual "\"...\"" quoting of args in ModuleSafeLoad and MSVC compiler support (Process quotes; PothosUtil --load-module still unquotes) — cost if wrong: MSVC JIT args with spaces (Windows CI only).
Task 6: Ruling: RemoteServer: launch failure now throws RemoteServerError immediately (Poco left an empty process handle); destructor order requestTermination → join forwarders → close pipes → wait (closing an fd under a blocked reader is unsafe) — cost if wrong: shutdown hang if server ignores SIGTERM.
Task 6: Ruling: SelfTests closes outPipe after the verbose reader finished (was before) — same reason — cost if wrong: none.
Task 6: Ruling: GccClang test() catches launch failure → false (Poco threw) and drains output — cost if wrong: none.
Task 6: Note: Pipe paths from std::string on Windows go through ANSI code page in std::filesystem::path — non-ASCII paths may break (pre-existing class of issue).
CI: pothos2 a1ae6a3: ALL FOUR JOBS GREEN (windows, linux-gcc, linux-clang, macos-15-intel) — phase 1 verified on all platforms.
Task 6: complete (commits 7749b23..4105ed4, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.49 sec)
Task 7: Tests written first, but their RED run was not observed in isolation (first build already contained the implementation) — honest gap; GREEN: 8 logger tests (levels, format pattern, braces, concurrent 8x1000, async order, fatal sync with slow sink, after shutdown, bad level env). Manual: default format output verified; bad level warned twice at first (static init + setupDefaultLogging) → RED observed via output, fixed to warn once per invalid value → 1 line. Self-tests 117/117. 28 files freed (24 + 3 unused <Poco/Exception.h> + TestRemote unused <Poco/URI.h>).
Task 7: Ruling: console sink writes with C stdio to stderr (Poco wrote std::clog) — forwardStdIoToLogging intercepts clog/cout/cerr, writing there would recurse — cost if wrong: logs on stderr instead of clog (same fd).
Task 7: Ruling: color only when stderr is a terminal (Windows: VT mode enabled) — Poco colored unconditionally — cost if wrong: no color in pipes (intended).
Task 7: Ruling: RemoteServer::startSyslogForwarding keeps sending the child's stdout/stderr lines straight to the given syslog address (plan said via Logger::log, which would change where they go); guessPrio now uses Detail::guessLevel — cost if wrong: none.
Task 7: Ruling: syslog listener delivers received records to the sink WITHOUT level filtering (as Poco listener fed the root channel directly) — cost if wrong: remote debug messages shown regardless of local level (as before).
Task 7: Ruling: stopSyslogForwarding sets an empty sink (Poco left the closed forwarder as root channel: messages dropped) — same effect — cost if wrong: none.
Task 7: Ruling: shutdownLogging (static guard at unload/exit) does not wait for the writer thread: it drains the queue itself, joins on POSIX, detaches on Windows (threads are already killed when DLL static destructors run at exit) — cost if wrong: a record written concurrently by the writer and the drain could be reordered at exit.
Task 7: Ruling: LoggerState is a leaked singleton (never destroyed) so logging from static destructors cannot touch a destroyed object — cost if wrong: none (one allocation at exit).
Task 7: Ruling: Logger::flush registered in the managed class too; threadId is a hash of std::thread::id (not an OS tid) — cost if wrong: %T shows a hash.
Task 7: Ruling: per-source levels are exact-name only (Poco had hierarchical "a.b" inherits "a") — per plan — cost if wrong: setLevel("Pothos") no longer affects "Pothos.X".
Task 7: complete (commits 4105ed46c..2ab3814, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.83 sec)
Task 8: Reference + escaping tests captured from the original Poco XML output (both PASS on old code, as plan Expected) → still PASS after the rewrite; self-tests 119/119.
Task 8: Ruling: reference test checks each node label/edge verbatim after normalization (whitespace removed, node-id hashes → N) instead of a byte-for-byte golden — node order follows random block uids — cost if wrong: indentation changes undetected (cosmetic).
Task 8: Ruling: PocoXML stays installed and in IN_TREE_LIBRARIES (PocoUtil depends on it at runtime); libPothos no longer links it directly, but it is still in libPothos NEEDED through PocoUtil's link interface — plan's `ldd ... | grep -c PocoXML → 0` not achievable until Poco::Util goes (phase 5) — cost if wrong: none.
Task 8: complete (commits 2ab3814..5cd420d, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.79 sec)
Task 9: allowlist 89 → 10 files, every one uses only Poco/Net/* or Poco/Util/* (PothosUtil.cpp/.hpp, PothosUtilProxyServer, ConfLoader & ModuleSafeLoad PropertyFileConfiguration, TopologyNetworkFlows, Remote Client/Server, System/Logger syslog, Util/Network). Docs updated. Step 5 (push + PR) awaits user consent.
Task 9: complete (commits 5cd420d..b193167, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.64 sec)
CI run 40354927955 (b193167, first push of the branch): all 4 failed — linux/windows: pothos.poco_allowlist (cmake -P without cmake_minimum_required → CMake 3.x OLD policy CMP0057, IN_LIST not an operator; local CMake 4.4 hid it); windows build: TestSharedBuffer passes fs::path to std::string param (wchar_t paths); macos: test_unique_temp_path/test_temp_file (temp_directory_path has a trailing slash). Fixed 3f7ffd7 (cmake_minimum_required in all -P scripts, .string(), fs::equivalent).
Final review: opus reviewer, verdict "With fixes" (0 Critical, 6 Important, 16 Minor).
Final: fixed Important 5 flush starvation — test_logger_flush_under_load RED (flush not returned in 5s with a never-empty queue) → GREEN with enqueued/written sequence counters, suite 120/120 (6d05765).
Final: fixed Important 2 Windows termination event name — test_termination_event_name RED (missing fn) → GREEN, name = POCOTRM + %08X (bc68e12).
Final: fixed Important 1 fd inheritance — test_process_no_fd_inheritance RED (child saw fd-open) → GREEN via posix_spawn_file_actions_addclosefrom_np(3) (glibc>=2.34) / POSIX_SPAWN_CLOEXEC_DEFAULT + addinherit_np (macOS) (c806a92).
Final: fixed Important 3 (partial), 4, 6 — Windows-only, no local RED possible; verification = Windows CI (ab846dd): exe.wstring() in command line, _wdupenv_s/_wputenv_s with UTF-8 conversion, try_lock drain at exit on Windows, lexically_normal+make_preferred for LoadLibraryExW. Suite 123/123 on Linux.
Final: Ruling: Important 3 remainder — std::filesystem::path built from std::string (ANSI code page on MSVC) at call sites across the code base is NOT converted now; policy "std::string = UTF-8, convert via u8string" to be applied in phase 6 (public API switches to std::filesystem::path) — cost if wrong: non-ASCII install/module paths break on Windows until then.
Final: Ruling: declined-to-judge items (RemoteServer stderr >64KiB while reading Port:, SIGPIPE on closed child pipes, inherited signal mask, BSD nodeId, exact-name levels/%T hash/cache epoch, PocoXML via PocoUtil, nodeId enumerate per call, globMatch worst case, unescaped DOT edge ports) — pre-existing or already ruled, stand — cost if wrong: as noted by reviewer.
Final: minor (deferred): logger dispatch checks `shutdown` outside queueMutex (race may start a writer after shutdown)
Final: minor (deferred): Windows restart race after detach (tests only)
Final: minor (deferred): POTHOS_LOG_LEVEL=none no longer disables logging
Final: minor (deferred): Error level is asynchronous (last errors before a crash may be lost)
Final: minor (deferred): a bad level warning at load starts the writer thread; no pthread_atfork handling (phase 5 --daemon)
Final: minor (deferred): fdMutex/spawnMutex duplicated in libPothos and PothosUtil
Final: minor (deferred): macOS dylib uses `extern char **environ` (Apple suggests _NSGetEnviron)
Final: minor (deferred): Process default-constructible with null impl; Unix wait() holds the mutex while blocking
Final: minor (deferred): Windows console sink writes UTF-8 via fprintf (Poco used WriteConsoleW)
Final: minor (deferred): file sink silently drops records if the file cannot be opened
Final: minor (deferred): RemoteServer::Impl default-constructs two unused Pipes
Final: minor (deferred): Windows expandPath turns undefined %VAR% into empty (Poco left it)
Final: minor (deferred): JIT: std::filesystem exceptions escape catch(Pothos::Exception); stray indentation on tokSep line
Final: minor (deferred): no end-to-end test that argv with spaces/quotes/trailing backslash reaches a child unchanged
Final: minor (deferred): test_logger_bad_level_env does not cover the real startup path (load + setupDefaultLogging)
