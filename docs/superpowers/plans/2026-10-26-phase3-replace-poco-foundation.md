# Phase 3: replace Poco Foundation — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The core does not use Poco Foundation/XML anywhere, except in the code that still depends on Poco Net/Util (Remote, `Util/Network.cpp`, the syslog part of the logger, `apps/PothosUtil*`). A public logging API on `std::format` and an internal platform layer appear.

**Architecture:** The replacement works as a "ratchet". The CTest test `pothos.poco_allowlist` allows Poco only in the files from `cmake/poco-allowlist.txt`. It fails when a file from the list does not use Poco any more. Therefore each task must make the list shorter. The C++23 standard covers strings, numbers, format, files, random, regex and byte order. The OS-specific code moves to the internal layer `lib/Platform/` (namespace `Pothos::Platform`, the headers are not installed). This code is environment variables, system information, library loading, processes and pipes. The logging is an own core with the sinks console/color/file/null. The UDP syslog still goes through adapters to Poco Net (phase 4 replaces them).

**Tech Stack:** C++23 (`std::format`, `std::filesystem`, `<random>`, `<regex>`, `std::byteswap`), POSIX (`posix_spawnp`, `pipe`, `dlopen`, `uname`), Win32 (`CreateProcessW`, `CreatePipe`, `LoadLibraryW`), CMake/CTest, the existing self-test framework `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`; phase 3 runs before phase 2 by the decision of the user)

## Global Constraints

- C++23, no compiler extensions. Linux, macOS (≥ 13.3), Windows (MSVC): the CI must stay green on all three.
- No new external dependencies. The OS code uses only system APIs.
- The internal headers (`lib/Platform/*`, the helper `lib/**/*.hpp`) are not installed and `include/Pothos/**` does not include them.
- You can change the public API (compatibility is not necessary). The architecture of blocks, ports, topologies, Object, Callable and Proxy stays.
- The namespace is an open question (spec, section "Branches and version"). The new code lives in `Pothos`/`Pothos::System`/`Pothos::Platform`. Do not copy the name `Pothos` into string literals and paths without a need.
- The environment variables of the logger stay: `POTHOS_LOG_LEVEL` (default `information`), `POTHOS_LOG_CHANNEL` (`color`; also `console`, `file`, `null`), `POTHOS_LOG_FILE` (`pothos.log`), `POTHOS_LOG_FORMAT` (`%Y-%m-%d %H:%M:%S %s: %t`), `POTHOS_LOG_TIMEZONE` (`local` | `UTC`). The new variable is `POTHOS_LOG_ASYNC` (`1` by default, `0` — synchronous).
- Quill is not used (decision of the user). The logger is an own logger with an internal sink interface. This interface lets you connect Quill later with no change to the public API.
- The log forwarding over UDP stays compatible with syslog (in this phase through Poco Net).
- The check of each task: `cmake --build build && ctest --test-dir build --output-on-failure` → all tests PASS, N self-tests out of N.
- Push to `origin` only with the consent of the user.

## Review Focus

1. **Process arguments with spaces and quotes** (module paths, a path to `PothosUtil` like `C:\Program Files\...`) must reach the child process with no change on all OSes. The test `test_windows_command_line_quoting` (Task 6) checks the quoting algorithm as a pure function on all platforms.
2. **The child process writes more than the pipe buffer holds (> 64 KiB)** and the parent reads only stdout: there must be no deadlock. The test `test_process_large_output` (Task 6).
3. **Logging from many threads at the same time:** no loss and no merge of messages, no crashes. The test `test_logger_concurrent` (Task 7).
4. **A log message contains curly braces** (JSON, user text): it must not throw `std::format_error`. The test `test_logger_braces_in_message` (Task 7).
5. **A wrong value of `POTHOS_LOG_LEVEL`** (`bogus`): the start does not fail, `information` is used, and one warning is shown. The test `test_logger_bad_level_env` (Task 7).

---

### Task 1: The ratchet `pothos.poco_allowlist`

**Files:**
- Create: `cmake/PothosCheckPoco.cmake`, `cmake/poco-allowlist.txt`
- Modify: `CMakeLists.txt` (the test section)

**Interfaces:**
- Produces: the CTest test `pothos.poco_allowlist` = `cmake -DSRC=${PROJECT_SOURCE_DIR} -DALLOWLIST=${PROJECT_SOURCE_DIR}/cmake/poco-allowlist.txt -P cmake/PothosCheckPoco.cmake`.
- Produces: the format of `poco-allowlist.txt` is one path from the repository root on each line. `#` comments and empty lines are ignored. **Each next task removes from it the lines of the files that do not use Poco any more.**

The rules of the script: It scans `include/`, `lib/` and `apps/` (`*.cpp`, `*.hpp`, `*.in.cpp`). A file "uses Poco" if it contains `#include <Poco/` / `#include "Poco/` or `Poco::`/`POCO_`/`poco_`. The script gives an error in these cases: (a) a file uses Poco and is not in the list; (b) a file is in the list, but does not use Poco or does not exist; (c) any file under `include/` uses Poco (even from the list). The message lists all violations.

- [ ] **Step 1: Write the script and an empty list, register the test**
- [ ] **Step 2: Run — it must fail**

Run: `ctest --test-dir build -R poco_allowlist --output-on-failure`
Expected: FAIL, about 88 files are listed (exactly the files that `grep -rlE '#include\s*[<"]Poco/|Poco::|POCO_|poco_' include lib apps` gives), also `include/Pothos/Exception.hpp` as a violation of rule (c).

- [ ] **Step 3: Remove Poco from `include/Pothos/Exception.hpp`** — delete the line `catch(const Poco::Exception &ex)...` from `POTHOS_EXCEPTION_CATCH` (Poco::Exception inherits `std::exception`, and the next branch catches it). Fill the list with the current files of `lib/`/`apps/`.
- [ ] **Step 4: Run** `cmake --build build && ctest --test-dir build --output-on-failure` → Expected: all PASS (5 tests).
- [ ] **Step 5: Commit** `test: ratchet test that confines Poco to an allowlist`

---

### Task 2: Strings and numbers

**Files:**
- Create: `lib/Util/StringUtils.hpp`, `lib/Util/Builtin/TestStringUtils.cpp`
- Modify: all files with `Poco/StringTokenizer.h`, `Poco/String.h`, `Poco/NumberFormatter.h`, `Poco/NumberParser.h`, `Poco/Types.h`, `Poco/HashMap.h`, `Poco/Hash.h`, `Poco/Random.h`, `Poco/RandomStream.h`, `Poco/ByteOrder.h`, `Poco/RegularExpression.h`, `Poco/Bugcheck.h` (the list: `grep -rlE 'Poco/(StringTokenizer|String|NumberFormatter|NumberParser|Types|HashMap|Hash|Random|RandomStream|ByteOrder|RegularExpression|Bugcheck)\.h' lib apps`); `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (`lib/Util/StringUtils.hpp`, namespace `Pothos::Util::Detail`, header-only):
  - `std::string_view trim(std::string_view s);` — removes the whitespace characters (by `std::isspace`) on both sides.
  - `std::string toLower(std::string_view s);`, `std::string toUpper(std::string_view s);` — ASCII.
  - `bool iequals(std::string_view a, std::string_view b);` — ASCII, case-insensitive.
  - `std::vector<std::string> split(std::string_view s, std::string_view separators, bool trimTokens = true, bool skipEmpty = true);` — the equivalent of `Poco::StringTokenizer(s, separators, TOK_TRIM|TOK_IGNORE_EMPTY)`. A separator is any character from `separators`.
  - `std::string replaceAll(std::string s, std::string_view from, std::string_view to);`

Replacements without wrappers: `NumberFormatter` → `std::format`/`std::to_string`. `NumberParser` → `std::from_chars` (a parse error → `Pothos::SyntaxException`, if the calling code expected an exception). `Poco::Int64`… → `<cstdint>`. `HashMap` → `std::unordered_map`. `Poco::hash` → `std::hash`. `Random`/`RandomStream` → `std::mt19937_64` + `std::random_device`. `ByteOrder` → `std::byteswap` + `std::endian`. `RegularExpression` → `std::regex` (ECMAScript). `poco_assert`/`poco_bugcheck` → `assert`/`throw Pothos::AssertionViolationException`.

- [ ] **Step 1: The tests `TestStringUtils.cpp`** (`POTHOS_TEST_BLOCK("/util/tests", ...)`, add to `lib/CMakeLists.txt`):
  - `test_string_trim`: `trim("  a b \t\n") == "a b"`, `trim("") == ""`, `trim("   ") == ""`.
  - `test_string_case`: `toLower("AbC1") == "abc1"`, `toUpper("abc") == "ABC"`, `iequals("Port:", "port:")`, `!iequals("a", "ab")`.
  - `test_string_split`: `split("Port: 1234", " ")` == `{"Port:", "1234"}`; `split(" a , ,b ", ",")` == `{"a", "b"}`; `split("a,,b", ",", true, false)` == `{"a", "", "b"}`; `split("", ",")` is empty.
  - `test_string_replace_all`: `replaceAll("a.b.c", ".", "::") == "a::b::c"`, `replaceAll("aaa", "a", "aa") == "aaaaaa"` (it does not loop).
- [ ] **Step 2: Run** `ctest --test-dir build -R selftests` → Expected: FAIL at the build (`StringUtils.hpp` does not exist).
- [ ] **Step 3: Implement `StringUtils.hpp`. Replace the listed Poco headers and calls. Delete the freed files from `cmake/poco-allowlist.txt`.**
- [ ] **Step 4: Run** the full `ctest` → Expected: all PASS; `grep -rlE 'Poco/(StringTokenizer|String|NumberFormatter|NumberParser|Types|HashMap|Hash|Random|RandomStream|ByteOrder|RegularExpression|Bugcheck)\.h' lib apps` → empty.
- [ ] **Step 5: Commit** `refactor: replace Poco string/number utilities with the standard library`

---

### Task 3: `Poco::format` → `std::format`

**Files:**
- Modify: all files with `Poco/Format.h` (`grep -rl 'Poco/Format.h' lib apps`, 37 files); `cmake/poco-allowlist.txt`

**Interfaces:** none new.

The rules to translate the specifiers: `%s %d %u %z %?d %?i %b` → `{}`; `%x` → `{:x}`; `%0Nd` → `{:0N}`; `%.Nf` → `{:.Nf}`; `%f` → `{:f}`; `%e` → `{:e}`; `%%` → `%`; positional `%[n]s` → `{n}`. Escape the literal `{`/`}` in the source string as `{{`/`}}`. If the format string is not a literal (the code builds it at run time), use `std::vformat` + `std::make_format_args`, or a concatenation.

- [ ] **Step 1: RED** — `ctest -R poco_allowlist` passes, and `grep -rc 'Poco::format' lib apps | grep -v ':0' | wc -l` gives a number > 0. Write this number in the report of the task.
- [ ] **Step 2: Translate all calls. Delete the freed files from the list.**
- [ ] **Step 3: Run** the full `ctest` → Expected: all PASS; `grep -r 'Poco::format\|Poco/Format.h' lib apps` → empty. The build has no new `-Wformat` warnings.
- [ ] **Step 4: Commit** `refactor: Poco::format -> std::format`

---

### Task 4: File system

**Files:**
- Modify: the files with `Poco/Path.h`, `Poco/File.h`, `Poco/TemporaryFile.h`, `Poco/Glob.h` (`grep -rlE 'Poco/(Path|File|TemporaryFile|Glob)\.h' lib apps`), `lib/ConfLoader/FileRealPath.cpp`, `lib/System/Paths.in.cpp`, `lib/Init.cpp`, `lib/Framework/Builtin/TestSharedBuffer.cpp`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces: the public functions `Pothos::System::get*Path()` continue to return `std::string` in the format of `std::filesystem::path::string()` (on Windows with `\`). The change of the return type to `std::filesystem::path` happens in phase 6, not now.
- Produces (`lib/Util/TempFile.hpp`, namespace `Pothos::Util::Detail`): `class TempFile { public: TempFile(); ~TempFile(); const std::filesystem::path &path() const; }` — a unique name in `std::filesystem::temp_directory_path()`. The constructor creates the file. The destructor deletes it (errors are ignored). The class is not copyable.

Replacements: `Poco::Path`/`File` → `std::filesystem` (`exists`, `is_directory`, `create_directories`, `file_size`, `absolute`, `canonical`/`weakly_canonical`). The check "executable" in `Init.cpp` → `std::filesystem::status(p).permissions() & perms::owner_exec` on POSIX, and on Windows the existence of the file. `Glob` → a walk with `directory_iterator` + a comparison of the extension/pattern (make this exact at the place of use). `Poco::Path::home()` → `HOME`/`USERPROFILE` through Task 5 (before Task 5, use `std::getenv` for a short time).

- [ ] **Step 1: The test** `lib/Util/Builtin/TestTempFile.cpp`: `test_temp_file` — the path exists after the constructor and is in `temp_directory_path()`. Two objects give different paths. After the destructor the file does not exist.
- [ ] **Step 2: Run** → FAIL at the build.
- [ ] **Step 3: Implement `TempFile`, translate the files, delete the lines from the list.**
- [ ] **Step 4: Run** the full `ctest` → all PASS; `grep -rlE 'Poco/(Path|File|TemporaryFile|Glob)\.h' lib apps` → empty.
- [ ] **Step 5: Commit** `refactor: Poco::Path/File/TemporaryFile/Glob -> std::filesystem`

---

### Task 5: The platform layer — environment and system information

**Files:**
- Create: `lib/Platform/Environment.hpp`, `lib/Platform/EnvironmentUnix.cpp`, `lib/Platform/EnvironmentWindows.cpp`, `lib/Platform/Builtin/TestEnvironment.cpp`
- Modify: the files with `Poco/Environment.h`, `Poco/Platform.h`, `Poco/Foundation.h` (`grep -rlE 'Poco/(Environment|Platform|Foundation)\.h' lib apps`), `lib/System/HostInfo.cpp`, `lib/Util/UID.cpp`, `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (namespace `Pothos::Platform`):
  - `std::optional<std::string> getEnv(const std::string &name);`
  - `void setEnv(const std::string &name, const std::string &value);` — on Windows through `_putenv_s` (both `std::getenv` and the child processes see it).
  - `void unsetEnv(const std::string &name);`
  - `std::string osName();` (`Linux`/`Darwin`/`Windows NT` — as in `uname -s` and Poco), `std::string osVersion();`, `std::string osArchitecture();`, `std::string nodeName();`, `unsigned processorCount();` (`std::thread::hardware_concurrency()`, minimum 1), `int currentProcessId();`
- The platform macros: `POCO_OS_FAMILY_WINDOWS` → `_WIN32`, `POCO_OS_FAMILY_UNIX` → `!_WIN32`, `POCO_OS == POCO_OS_MAC_OS_X` → `__APPLE__`.

- [ ] **Step 1: The tests** (`/platform/tests`): `test_env_roundtrip` — the name is `POTHOS_TEST_ENV_` + `currentProcessId()`: `setEnv(n,"a b=c")` → `getEnv(n) == "a b=c"`, `unsetEnv(n)` → `getEnv(n) == std::nullopt`. `test_env_missing` — `getEnv("POTHOS_SURELY_UNSET_VARIABLE") == std::nullopt`. `test_host_info` — `osName()`, `nodeName()` and `osArchitecture()` are not empty, `processorCount() >= 1`, `currentProcessId() > 0`.
- [ ] **Step 2: Run** → FAIL at the build.
- [ ] **Step 3: Implement (POSIX: `getenv/setenv/unsetenv`, `uname`; Windows: `_dupenv_s`/`_putenv_s`, `RtlGetVersion` or `GetVersionEx`, `GetComputerNameW`, `GetNativeSystemInfo`), translate the files, update the list.**
- [ ] **Step 4: Run** the full `ctest` → all PASS; `grep -rlE 'Poco/(Environment|Platform|Foundation)\.h|Poco::Environment|POCO_OS' lib apps` → only the files that stay in the allowlist for other reasons.
- [ ] **Step 5: Commit** `feat: internal platform layer for environment and host info`

---

### Task 6: The platform layer — libraries, processes, pipes

**Files:**
- Create: `lib/Platform/DynamicLibrary.hpp`, `lib/Platform/DynamicLibraryUnix.cpp`, `lib/Platform/DynamicLibraryWindows.cpp`, `lib/Platform/Process.hpp`, `lib/Platform/ProcessUnix.cpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/CommandLine.cpp`, `lib/Platform/Builtin/TestDynamicLibrary.cpp`, `lib/Platform/Builtin/TestProcess.cpp`
- Modify: the files with `Poco/SharedLibrary.h`, `Poco/Process.h`, `Poco/Pipe.h`, `Poco/PipeStream.h` (`lib/Plugin/Module.cpp`, `lib/Plugin/Loader.in.cpp`, `lib/Plugin/ModuleSafeLoad.cpp`, `lib/ConfLoader/*`, `lib/Util/Builtin/GccClangCompilerSupport.in.cpp`, `lib/Util/Builtin/MsvcCompilerSupport.in.cpp`, `lib/Remote/Server.cpp`, `lib/Remote/Builtin/TestRemote.cpp`, `apps/PothosUtilSelfTests.cpp`); `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces `DynamicLibrary` (not copyable, movable):
  - `explicit DynamicLibrary(const std::filesystem::path &path);` — throws `Pothos::LibraryLoadException` with the path and the text of `dlerror()`/`FormatMessageW`. On Windows it uses `LoadLibraryW` (UTF-8 → UTF-16).
  - `void *symbol(const std::string &name) const;` — `nullptr` if the symbol does not exist. `~DynamicLibrary()` unloads the library.
  - The module suffix is not part of the class. The current `MODULE_EXT` from `lib/CMakeLists.txt` stays (`.so` on Unix, also macOS, `.dll` on Windows).
- Produces `Pipe` (a shared handle; a copy shares one channel, as in Poco):
  - `Pipe();` — an anonymous pipe. `std::size_t read(char *buf, std::size_t len);` (0 = EOF), `std::size_t write(const char *buf, std::size_t len);`, `void closeRead();`, `void closeWrite();`, `void close();`
  - `class PipeInputStream : public std::istream { explicit PipeInputStream(Pipe); };`, `class PipeOutputStream : public std::ostream { explicit PipeOutputStream(Pipe); };`
- Produces `Process`:
  - `struct ProcessOptions { std::vector<std::string> args; std::map<std::string, std::string> env; Pipe *in = nullptr; Pipe *out = nullptr; Pipe *err = nullptr; };` — `env` adds to the inherited environment. `nullptr` means that the stream is inherited.
  - `static Process launch(const std::filesystem::path &exe, const ProcessOptions &opts);` — throws `Pothos::SystemException` with the path and the OS error if the launch fails (not an "empty process", as in Poco). After the launch the parent closes its copies of the child ends of the pipes.
  - `int id() const;`, `int wait();` (the exit code; if a signal ends the process, `128 + signo`), `std::optional<int> tryWait();`, `void requestTermination();` (SIGTERM; on Windows `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT)` for a process that started with `CREATE_NEW_PROCESS_GROUP`, otherwise `TerminateProcess`), `void kill();`
  - The destructor does not kill the process (as the Poco `ProcessHandle`).
- Produces `std::wstring buildWindowsCommandLine(const std::filesystem::path &exe, const std::vector<std::string> &args);` in `CommandLine.cpp`. It compiles on all OSes (for the test). The quoting rules are the same as for `CommandLineToArgvW` (spaces/tabs → quotes; `"` → `\"`; the backslashes before `"` are doubled).
- The manual quoting in the calling code (`"\""+path+"\""` in `ModuleSafeLoad.cpp`) is deleted. `Process` does the quoting.

- [ ] **Step 1: The tests** (`/platform/tests`):
  - `test_dynamic_library_load`: a table for each OS — Linux `libc.so.6`/`getpid`, macOS `/usr/lib/libSystem.B.dylib`/`getpid`, Windows `kernel32.dll`/`GetCurrentProcessId`; `symbol(name) != nullptr`, `symbol("pothos_no_such_symbol") == nullptr`.
  - `test_dynamic_library_missing`: `POTHOS_TEST_THROWS(DynamicLibrary("/no/such/lib.so"), Pothos::LibraryLoadException)`; the text of the exception contains the path.
  - `test_windows_command_line_quoting` (on all OSes): `exe=C:\Program Files\P\PothosUtil.exe`, args `{"--load-module", "C:\my mods\a.dll", "say \"hi\"", "trail\\", ""}` → exactly `L"\"C:\\Program Files\\P\\PothosUtil.exe\" --load-module \"C:\\my mods\\a.dll\" \"say \\\"hi\\\"\" trail\\ \"\""`.
  - `test_pipe_roundtrip`: `Pipe p; PipeOutputStream(p) << "hello\nworld\n" << std::flush; p.closeWrite();` → `PipeInputStream(p)` reads the lines `hello`, `world`, then EOF.
  - `test_process_exit_code`: `launch(System::getPothosUtilExecutablePath(), {.args={"--success-code","7"}}).wait() == 7`.
  - `test_process_capture_stdout`: `PothosUtil --help` with an `out` pipe → the output contains `self-tests`.
  - `test_process_large_output` (Review Focus 2): No PothosUtil command gives > 64 KiB (the maximum, `--plugin-tree`, is about 28 KiB). Therefore the same file has a helper block `POTHOS_TEST_BLOCK("/platform/tests", helper_large_output)`. If `getEnv("POTHOS_TEST_LARGE_OUTPUT") == "1"`, it writes to `std::cout` 4096 lines of 63 `x` characters + `\n` (262144 bytes). Otherwise it does nothing. The test starts `PothosUtil --self-test1 /platform/tests/helper_large_output` with `env = {{"POTHOS_TEST_LARGE_OUTPUT","1"}}` and an `out` pipe. It reads stdout to EOF. It checks that there are exactly 4096 lines `x…x` and that `wait() == 0` (this also checks the pass of `env`).
  - `test_process_launch_failure`: `launch("/no/such/exe", {})` → `POTHOS_TEST_THROWS(..., Pothos::SystemException)`.
- [ ] **Step 2: Run** → FAIL at the build.
- [ ] **Step 3: Implement (POSIX: `posix_spawn` + `posix_spawn_file_actions`, `pipe2(O_CLOEXEC)`/`pipe`+`FD_CLOEXEC`, `waitpid`, `dlopen(RTLD_NOW|RTLD_GLOBAL)` — check the default flags of the Poco SharedLibrary in `poco/Foundation/src/SharedLibrary_UNIX.cpp` and keep them; Windows: `CreatePipe` with inheritable ends only for the child sides, `CreateProcessW` + `STARTF_USESTDHANDLES`, `WaitForSingleObject`/`GetExitCodeProcess`), translate the calling code, update the list.**
- [ ] **Step 4: Run** the full `ctest` → all PASS (also the Remote tests and `ModuleSafeLoad` through the module loading); `grep -rlE 'Poco/(SharedLibrary|Process|Pipe|PipeStream)\.h' lib apps` → empty.
- [ ] **Step 5: Commit** `feat: internal platform layer for dynamic libraries, processes and pipes`

---

### Task 7: The public logging API

**Files:**
- Modify: `include/Pothos/System/Logger.hpp`, `lib/System/Logger.cpp`
- Create: `lib/System/LoggerSinks.hpp`, `lib/System/LoggerSinks.cpp` (console/color/file/null, the formatter), `lib/System/Builtin/TestLogger.cpp`
- Modify: all files with `poco_(fatal|critical|error|warning|notice|information|debug|trace)`, `Poco::Logger`, `Poco/Logger.h`, `Poco/Message.h`, the Poco channels (`grep -rlE 'poco_(fatal|critical|error|warning|notice|information|debug|trace)|Poco::Logger|Poco/(Logger|Message|.*Channel|PatternFormatter|AutoPtr)\.h' lib apps`); `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (public, `include/Pothos/System/Logger.hpp`, namespace `Pothos::System`):
  - `enum class LogLevel : int { Fatal = 1, Critical, Error, Warning, Notice, Information, Debug, Trace };` (the numbers are the same as the syslog priorities of Poco — the syslog adapters need them).
  - In the class `Logger` (the existing static methods stay): `static void log(LogLevel level, const std::string &source, const std::string &message);` (the message is a ready text, not a format string), `static bool isEnabled(LogLevel level, const std::string &source);`, `static void setLevel(LogLevel level, const std::string &source = "");` (`""` is the root), `static LogLevel getLevel(const std::string &source = "");`, `static std::optional<LogLevel> parseLevel(std::string_view name);` (`fatal`…`trace`, case-insensitive).
  - Templates: `template <typename... Args> void log(LogLevel level, const std::string &source, std::format_string<Args...> fmt, Args &&...args);` — formats only if `isEnabled`.
  - Macros: `POTHOS_LOG_FATAL(source, ...)`, `POTHOS_LOG_CRITICAL`, `POTHOS_LOG_ERROR`, `POTHOS_LOG_WARNING`, `POTHOS_LOG_NOTICE`, `POTHOS_LOG_INFORMATION`, `POTHOS_LOG_DEBUG`, `POTHOS_LOG_TRACE` → `::Pothos::System::log(LogLevel::X, source, __VA_ARGS__)`.
  - The level of a source: the exact name, otherwise the root. The default threshold comes from `POTHOS_LOG_LEVEL`.
- Produces (internal, `lib/System/LoggerSinks.hpp`, namespace `Pothos::System::Detail`): `struct LogRecord { LogLevel level; std::string source; std::string text; std::chrono::system_clock::time_point time; unsigned long threadId; int pid; };` `using LogSink = std::function<void(const LogRecord &)>;` `void setLogSink(LogSink sink);` (it replaces the current sink; for the tests and the syslog adapters), `std::string formatLogRecord(const LogRecord &r, std::string_view pattern, bool utc);` — support the tokens `%Y %m %d %H %M %S %i`(ms) `%s`(source) `%t`(text) `%p`(level name) `%q`(first letter of the level) `%P`(pid) `%T`(thread id) `%%`. An unknown token is written as it is.
  - **Asynchronous mode** (Quill is not used — decision of the user; the `LogSink` interface lets you connect it later as a sink): `Logger::log` puts a `LogRecord` in a queue (mutex + `std::deque` + `condition_variable`). A background thread gives the records to the sink in order. This mode is on by default. `POTHOS_LOG_ASYNC=0` gives the synchronous mode. The rules: `Fatal` and `Critical` are written synchronously. `log()` first waits until the queue is empty, then writes itself, then flushes the sink. The public `static void Logger::flush();` waits until the queue is empty. `void Detail::shutdownLogging();` stops the thread and empties the queue. The library calls it at unload (a static object in `Logger.cpp`). After it, `log()` writes synchronously and does not crash (a protection against logging from the destructors of the static objects of plugins). `setLogSink` runs after `flush()`. The thread starts lazily at the first record (not at the load of the library). This keeps `fork()` safe for the `--daemon` mode of phase 5.
  - The syslog forwarding (`startSyslogForwarding`) is a sink. It turns a `LogRecord` into a `Poco::Message` and gives it to `Poco::Net::RemoteSyslogChannel` (in one `.cpp`, it stays in the allowlist until phase 4). The reception (`startSyslogListener`) is `Poco::Net::RemoteSyslogListener` + a small `Poco::Channel` that calls `Logger::log`. `lib/Remote/Server.cpp::pipeToSyslogWorker` writes through `Logger::log`.

- [ ] **Step 1: The tests** (`/system/tests`, the sink is replaced through `setLogSink` and restored at the end of the test):
  - `test_logger_levels`: `setLevel(Warning, "t.lvl")`; `POTHOS_LOG_INFORMATION("t.lvl", "x")` does not reach the sink; `POTHOS_LOG_ERROR("t.lvl", "v={}", 42)` reaches it with `text == "v=42"`, `level == LogLevel::Error`, `source == "t.lvl"`.
  - `test_logger_format_pattern`: `formatLogRecord({Error, "src", "msg", <2026-01-02 03:04:05.006 UTC>, ...}, "%Y-%m-%d %H:%M:%S.%i %p %s: %t", true) == "2026-01-02 03:04:05.006 Error src: msg"`.
  - `test_logger_braces_in_message` (RF 4): `Logger::log(Error, "t", "{raw}")` and `POTHOS_LOG_ERROR("t", "{}", "{\"a\":1}")` → the texts `{raw}` and `{"a":1}`, no exceptions.
  - `test_logger_concurrent` (RF 3): 8 threads × 1000 `POTHOS_LOG_ERROR("t.mt", "{} {}", thread, i)` → the sink (with a mutex) got 8000 records, and all texts are unique.
  - `test_logger_async_order`: in asynchronous mode, 1000 `POTHOS_LOG_ERROR("t.order", "{}", i)` from one thread → after `Logger::flush()` the sink got 1000 records in the order `0..999`.
  - `test_logger_fatal_sync`: a sink with an artificial delay of 1 ms for each record; 100 records `Error`, then `POTHOS_LOG_FATAL("t.fatal", "stop")` → when `log` returns, the sink already got all 101 records, and the last one is `stop` (with no call of `flush()`).
  - `test_logger_after_shutdown`: `Detail::shutdownLogging()`, then `POTHOS_LOG_ERROR("t.down", "late")` → the record reaches the sink synchronously, with no exception and no crash. Then the asynchronous mode is restored for the other tests (`Detail::restartLogging()` — a test function in the same internal header).
  - `test_logger_bad_level_env` (RF 5): `parseLevel("bogus") == std::nullopt`, `parseLevel("DEBUG") == LogLevel::Debug`. The function that initializes from the environment, with `POTHOS_LOG_LEVEL=bogus`, sets `Information` and logs one warning (make it available as `void Detail::configureFromEnvironment();` and call it in the test with a replaced environment through `Platform::setEnv`, and restore the environment after).
- [ ] **Step 2: Run** → FAIL at the build.
- [ ] **Step 3: Implement the core and the sinks. Translate all calls of `poco_*`/`Poco::Logger` to `POTHOS_LOG_*`. Write the syslog adapters. Update the list.**
- [ ] **Step 4: Run** the full `ctest` → all PASS. By hand: `POTHOS_LOG_LEVEL=debug build/stage/bin/PothosUtil --system-info` (with `POTHOS_ROOT=build/stage`) shows colored lines in the default format. `grep -rlE 'poco_(error|debug|information|warning)|Poco::Logger' lib apps` → only the syslog adapter in `lib/System/Logger.cpp`.
- [ ] **Step 5: Commit** `feat: public logging API on std::format; drop Poco::Logger`

---

### Task 8: DOT markup without Poco XML

**Files:**
- Modify: `lib/Framework/TopologyDotMarkup.cpp`, `lib/CMakeLists.txt` (remove `PocoXML` from the linking), `CMakeLists.txt` (remove `XML` from `install(TARGETS ...)`, the in-tree list of `PothosConfig.cmake`); `cmake/poco-allowlist.txt`
- Create: `lib/Framework/Builtin/TestDotMarkup.cpp`

**Interfaces:**
- Produces (inside `TopologyDotMarkup.cpp`): `static std::string htmlEscape(std::string_view s);` — `&`→`&amp;`, `<`→`&lt;`, `>`→`&gt;`, `"`→`&quot;`, `'`→`&#39;`. The code builds the HTML labels of DOT by concatenation, with the escape of all user strings (names of blocks, ports, values).

- [ ] **Step 1: The test** `test_dot_markup_escaping` (`/framework/tests`): a topology with a block whose name (`setName`) = `a<b>&"c"`; `topology.toDotMarkup()` contains `a&lt;b&gt;&amp;&quot;c&quot;` and does not contain `a<b>`; the markup starts with `digraph`. Also, the output of the current implementation for a simple topology (record it before the changes and save it as the expected string in the test) is the same as the new output byte by byte, except for allowed differences in whitespace. Record a whitespace normalization in the test.
- [ ] **Step 2: Run** → the escaping test PASS or FAIL on the current code (record which). The reference test PASS on the current code (it is recorded from this code).
- [ ] **Step 3: Rewrite the generation, remove PocoXML from the build and the install, update the list.**
- [ ] **Step 4: Run** the full `ctest` → all PASS; `ldd build/stage/lib/libPothos*.so | grep -c PocoXML` → `0`.
- [ ] **Step 5: Commit** `refactor: generate DOT markup without Poco XML`

---

### Task 9: Result of the phase

**Files:**
- Modify: `cmake/poco-allowlist.txt` (the final check), `CLAUDE.md`, `README.md` (the Dependencies section), `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (the status of phase 3)

- [ ] **Step 1: Make sure that the allowlist has only the files that need Poco because of Net/Util:** each remaining file includes `Poco/Net/` or `Poco/Util/` (or is the syslog adapter of the logger). If not, go back to the related task. Run: `ctest --test-dir build -R poco_allowlist` → PASS.
- [ ] **Step 2: Update the documentation** (CLAUDE.md: the logging API, `lib/Platform/`, the allowlist ratchet; roadmap: phase 3 is done, phase 2 is next).
- [ ] **Step 3: Run** the full `ctest` → all PASS.
- [ ] **Step 4: Commit** `docs: phase 3 complete — Poco confined to Net/Util users`
- [ ] **Step 5: With the consent of the user, push to `origin` (the phase branch → a PR to `pothos2`). Wait for the CI on all four jobs.**
