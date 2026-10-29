# Phase 5: PothosUtil without Poco::Util, remove Poco — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The core and `PothosUtil` do not use Poco. The `poco` submodule, its build, the allowlist ratchet and all references to Poco are removed. All `PothosUtil` modes stay, including the server, the Unix daemon and the Windows service.

**Architecture:**
- **Property files.** Its own reader and writer of the Poco `PropertyFileConfiguration` format (`lib/Util/PropertyFile.*`). They read and write the `.conf` files and the cache of the safe module load.
- **New internal static library `pothos_app`** (`lib/App/`, made with `pothos_internal_library`). `PothosUtil` and libPothos link it. libPothos links it for the self-tests. It contains:
  - the command line parser and the help text (`Options`);
  - the server mode: the wait for a termination request, the daemon and the pid file on Unix, and the Windows service (`Server`).
- **`PothosUtilBase`** does not inherit from `ServerApplication` any more. `config()` returns its own parsed `CommandLine` with the same methods (`has`, `getString`, `getUInt`, ...). Thus the command handlers change very little.

**Tech Stack:** C++23, POSIX (`sigwait`, `fork`/`setsid`), Win32 (`SetConsoleCtrlHandler`, Service Control Manager), CMake/CTest, `POTHOS_TEST_BLOCK` self-tests.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (section "Decisions": Poco is removed completely, the `PothosUtil` modes stay in full, the command line syntax can change). Roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`.

## Global Constraints

- C++23. Linux GCC/Clang, macOS and Windows MSVC: all four CI jobs are green.
- "Poco is removed completely". Nothing from `pothos_app` and `lib/Util/PropertyFile` goes into `include/Pothos/**`, and the install does not include it.
- "The `PothosUtil` modes stay in full":
  - a correct exit on SIGTERM / SIGINT / Ctrl-C (the equivalent of `waitForTerminationRequest`);
  - Unix: `--daemon`, `--pidfile`, `--umask`;
  - Windows: register, unregister and start as a Windows service. The option names can follow one common style.
- "The `PothosUtil` command line syntax can change ... the calling code in the core is updated to match".
  - The calls from the core and from the CMake modules must work:
    - `--require-active --proxy-server=URI` (`lib/Remote/Server.cpp`);
    - `--load-module PATH --success-code N` (`lib/Plugin/ModuleSafeLoad.cpp`, the values are separate arguments);
    - `--self-tests[=PATH]`, `--self-test1=PATH` (`cmake/PothosRunSelfTests.cmake`);
    - `--doc-parse FILES... --output FILE`, `--generate-simd-dispatchers=FILE --simd-arches=LIST --output=FILE` (`cmake/Modules/*.cmake`).
- The format of the `.conf` files and of the module loader cache does not change. The code reads the existing files in the same way as Poco read them.
- The check for each task: `cmake --build build && ctest --test-dir build --output-on-failure` -> all PASS.
- Push to `origin` only with the consent of the user.

## Decisions (for approval)

1. **Command line.**
   - An option with a required value accepts both `--opt=value` and `--opt value`. An optional value is given only with `=`. `-h` is the same as `--help`.
   - An unknown option or an invalid value gives a message and the code 64, as `EXIT_USAGE` does now.
   - The parser first parses the complete line. Then the commands run in the order of their appearance. Unlike Poco, the order of the options does not matter any more: `--self-tests --num-trials=3` also works. `--help` cancels the other commands.
2. **Server mode.** The commands that wait for the end (`--proxy-server`, `--run-topology` without a duration) wait for these events:
   - on Unix: SIGINT, SIGTERM, SIGQUIT (as in Poco);
   - on Windows: Ctrl-C, Ctrl-Break, the close of the console and the named termination event.

   The name of the event changes from `POCOTRM<pid>` to `PothosUtilTerminate<pid>` (8 hex digits). `Platform::Process::requestTermination` and `requestSelfTermination` change together with it.
3. **The Unix daemon is the same as in Poco:**
   - `--daemon` does `fork`, then `setsid`, sets the default umask 027 and redirects stdin, stdout and stderr to `/dev/null`;
   - `--umask=OCT` sets the umask;
   - `--pidfile=PATH` writes the pid and removes the file at exit.
4. **The Windows service has options in the common style:** `--register-service[=NAME]` (default `PothosUtil`), `--unregister-service[=NAME]`, `--display-name=...`, `--description=...`, `--startup=auto|manual`.
   - Unlike Poco, the registration keeps the other command line options in the start path of the service: `"PothosUtil.exe" --run-service=NAME <options>`. A service that starts without arguments would do nothing.
   - The hidden option `--run-service` starts the dispatcher of the Service Control Manager. A stop of the service is a termination request.
5. **`lib/App/` is the internal static library `pothos_app`.** The self-tests can check the command line code and the server code. A manual check is not the only way.

## Review Focus

1. **The calls to `PothosUtil` from the core and from CMake** (the list is in Global Constraints) parse into the same options and values as before. Test `test_app_core_command_lines` (Task 2).
2. **Server termination:** SIGTERM and SIGINT to the proxy server and to `RemoteServer::~RemoteServer` (through `requestTermination`) end the process with the code 0 in 5 s or less. Test `test_app_proxy_server_terminates` (Task 3).
3. **Daemon:**
   - `--daemon --pidfile=F --proxy-server=tcp://127.0.0.1:0`: the parent exits with the code 0, and the pid file contains a live pid;
   - SIGTERM to this pid ends it and removes the file;
   - `--umask` is applied.

   Test `test_app_daemon_pidfile` (Task 3, Unix only).
4. **Compatibility of the property format:** the comments `#`/`!`, `key = value` and `key: value`, the line continuation `\`, the escapes, the spaces around, CRLF, the empty values, a repeated key (the last one wins). All of them work as in Poco. Test `test_property_file_poco_compat` (Task 1).
5. **Windows service:**
   - register -> start through the SCM -> the service runs -> stop -> unregister;
   - without administrator rights, the result is a clear error, not a crash.

   Test `test_app_windows_service` (Task 4). On CI with administrator rights the test runs. Otherwise the test is skipped with a message.

---

### Task 1: Property files without Poco

**Files:**
- Create: `lib/Util/PropertyFile.hpp`, `lib/Util/PropertyFile.cpp`, `lib/Util/Builtin/TestPropertyFile.cpp`
- Modify: `lib/ConfLoader/ConfLoader.cpp`, `lib/Plugin/ModuleSafeLoad.cpp`, `lib/CMakeLists.txt`, `cmake/poco-allowlist.txt` (-2 lines)

**Interfaces (namespace `Pothos::Util::Detail`):**
- `using PropertyMap = std::map<std::string, std::string>;`
- `PropertyMap parseProperties(std::istream &is);` - the grammar of `PropertyFileConfiguration::parseLine`/`readChar` from Poco. The parser skips the spaces at the start of a line. `#`/`!` start a comment. The key ends at `=`/`:`/the end of the line (with a trim of the spaces). The value has the escapes `\t \r \n \f`. `\<any>` gives the character. `\` at the end of a line is a continuation. The value is trimmed.
- `std::string formatProperties(const PropertyMap &map);` - as `PropertyFileConfiguration::save`: `key: value\n`, with the escapes `\t \r \n \f \\`.
- `PropertyMap loadPropertyFile(const std::filesystem::path &path);` / `void savePropertyFile(const std::filesystem::path &path, const PropertyMap &map);` - the errors are `Pothos::FileException` with the path.

- [ ] **Step 1: Tests `TestPropertyFile.cpp`** (`/util/tests`):
  - `test_property_file_poco_compat`: a text with `# c`, `! c`, `  a = 1  `, `b: two words`, `c=`, `d`, `e = x\tz` (literally `\t`), `f = line1 \` + a line feed + `  line2`, `g = \\`, `CRLF` at the end of the lines, `a = 3` (a repeat) -> `{a:"3", b:"two words", c:"", d:"", e:"x<TAB>z", f:"line1   line2" (as in Poco: the spaces of the continuation stay), g:"\\"}`.
  - `test_property_file_roundtrip`: a map with values that contain `\n`, `\t`, `\\`, `:` and `=` -> `formatProperties` -> `parseProperties` -> the same map.
  - `test_property_file_io`: `savePropertyFile` to a temporary file (`Util/TempFile.hpp`) -> `loadPropertyFile` -> the same map. `loadPropertyFile("no-such-file")` -> `Pothos::FileException`.
- [ ] **Step 2: Run** -> FAIL (no header).
- [ ] **Step 3: Implement.** Change `ConfLoader.cpp` to use `loadPropertyFile` (the error is `Pothos::Exception("ConfLoader(path)", ...)` as now). Change `ModuleSafeLoad.cpp` (the cache uses `loadPropertyFile`/`savePropertyFile`, a missing key is `map.find`). Remove both lines from the allowlist.
- [ ] **Step 4: Run** the self-tests and the complete `ctest` -> PASS (the load of the modules and of the `.conf` files goes through `pothos.selftests`).
- [ ] **Step 5: Commit** `feat(util): Poco-compatible property files without Poco`

---

### Task 2: Command line parser (`pothos_app`)

**Files:**
- Create: `lib/App/Options.hpp`, `lib/App/Options.cpp`, `lib/App/Builtin/TestOptions.cpp`
- Modify: `lib/CMakeLists.txt` (`pothos_internal_library(pothos_app ...)`, link into `Pothos`), `apps/CMakeLists.txt` (link `pothos_app`), `CLAUDE.md` (the section about the internal libraries)

**Interfaces (namespace `Pothos::App`):**
- `enum class Arg { None, Required, Optional };`
- `struct OptionSpec { std::string name; char shortName = 0; Arg arg = Arg::None; std::string argName; std::string help; bool repeatable = false; bool hidden = false; };`
- `class CommandLine` - the result of the parse:
  - `const std::vector<std::pair<std::string, std::string>> &given() const;` - the options in the order of their appearance;
  - `const std::vector<std::string> &positional() const;`
  - `bool has(std::string_view name) const;` and `bool hasOption(std::string_view name) const;` (a synonym for the old code);
  - `std::string getString(std::string_view name, std::string def = {}) const;` (the last value);
  - `std::vector<std::string> getAll(std::string_view name) const;`
  - `int getInt(..., int def) const;`, `unsigned getUInt(..., unsigned def) const;`, `double getDouble(..., double def) const;` - a conversion error gives `Pothos::InvalidArgumentException` with the name of the option.
- `CommandLine parseCommandLine(const std::vector<OptionSpec> &specs, const std::vector<std::string> &args);` - as in decision 1. `--` ends the options. The errors are `Pothos::InvalidArgumentException` ("unknown option '--x'", "option '--x' requires a value", "option '--x' takes no value", "option '--x' given more than once").
- `std::string formatHelp(const std::vector<OptionSpec> &specs, std::string_view usage, std::string_view header);` - `--name[=ARG]` / `-h, --help`, a description with line wrap. The output does not include the hidden options.

- [ ] **Step 1: Tests `TestOptions.cpp`** (`/app/tests`):
  - `test_app_parse_forms`: `--a=1 --b 2 --c=x=y -h --flag pos1 -- --not-an-option` -> given `[(a,1),(b,2),(c,x=y),(help,""),(flag,"")]`, positional `[pos1, --not-an-option]`. An optional value `--opt` without `=` is empty, and the next argument is positional.
  - `test_app_parse_errors`: an unknown option, `--flag=1` for `Arg::None`, `--req` at the end of the line, a repeat of an option that is not repeatable, `getUInt("n")` for `"x"` and `"-1"` -> `Pothos::InvalidArgumentException`. `--var=a:1 --var=b:2` (repeatable) -> `getAll("var") == {"a:1","b:2"}`.
  - `test_app_core_command_lines`: the table of the `PothosUtil` options (Task 3 exports it with the function `pothosUtilOptions()` from `apps/PothosUtilOptions.cpp`. Before Task 3 the test uses a copy of the necessary options). It parses the lines from Global Constraints: `--require-active --proxy-server=tcp://[::]:0`, `--load-module /p/m.so --success-code 200`, `--self-tests`, `--self-tests=/util`, `--self-test1=/util/tests/x`, `--doc-parse a.cpp b.cpp --output out.json`, `--generate-simd-dispatchers=in.json --simd-arches=sse2,avx2 --output=h.hpp` - with the expected values.
  - `test_app_help`: `formatHelp` contains `--proxy-server[=URI]`, `-h, --help`, and does not contain the hidden option.
- [ ] **Step 2: Run** -> FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `/app/tests/*` and the complete `ctest` -> PASS.
- [ ] **Step 5: Commit** `feat(app): command line parser for PothosUtil`

---

### Task 3: `PothosUtil` without `Poco::Util`; termination wait and daemon

**Files:**
- Create: `lib/App/Server.hpp`, `lib/App/ServerUnix.cpp`, `lib/App/ServerWindows.cpp` (the termination wait; Task 4 adds the service here), `lib/App/Builtin/TestServer.cpp`, `apps/PothosUtilOptions.cpp` (the table of the options)
- Modify: `apps/PothosUtil.hpp`, `apps/PothosUtil.cpp`, `apps/PothosUtil*.cpp` (only `config()`/`waitForTerminationRequest` if necessary), `lib/Platform/Process.hpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/Builtin/TestProcess.cpp` (the name of the event), `cmake/poco-allowlist.txt` (-`apps/PothosUtil.cpp`, -`apps/PothosUtil.hpp`)

**Interfaces:**
- Consumes: `parseCommandLine`, `formatHelp`, `CommandLine` (Task 2).
- `void Pothos::App::waitForTerminationRequest(void);` - as in decision 2.
- Unix: `void Pothos::App::becomeDaemon(std::optional<unsigned> umaskValue);` and `class Pothos::App::PidFile { explicit PidFile(std::filesystem::path); ~PidFile(); };` (the constructor writes the file, the destructor removes it). On Windows, `--daemon`/`--umask`/`--pidfile` give the error "not supported on Windows".
- `std::wstring Pothos::Platform::terminationEventName(int pid)` -> `L"PothosUtilTerminate" + 8 hex`.
- `PothosUtilBase` (without a base class): `const Pothos::App::CommandLine &config() const;`, `void waitForTerminationRequest();` (it calls `App::`), the old command methods. `std::vector<Pothos::App::OptionSpec> pothosUtilOptions();` - the same options and help texts as now, and also the server options of decisions 3-4.
- `main`: parse -> `--help` (or no arguments) -> help, code 0. Otherwise: the server options (daemon, pid file) -> the commands in order. An exception of a command gives the text on stderr and the code 64. Otherwise the code is `--success-code` (0 by default).

- [ ] **Step 1: Tests:**
  - `TestServer.cpp` (`/app/tests`):
    - `test_app_proxy_server_terminates` - start `PothosUtil --proxy-server=tcp://127.0.0.1:0`, read `Port:` and call `requestTermination()` -> code 0 in 5 s or less. On Unix the test also does a direct `kill(pid, SIGINT)` -> code 0.
    - `test_app_daemon_pidfile` (Unix): `PothosUtil --daemon --umask=077 --pidfile=<tmp> --proxy-server=tcp://127.0.0.1:0` -> the parent exits with the code 0. The pid file appears in 5 s or less and has a live pid. `kill(pid, SIGTERM)` -> the process disappears and the file is removed in 5 s or less.
  - `TestProcess.cpp`: `test_termination_event_name` -> `L"PothosUtilTerminate0000ABCD"`.
  - With `Process::launch`: `test_app_pothosutil_cli` - `PothosUtil --help` -> code 0, stdout contains `--self-tests`. `PothosUtil --no-such-option` -> code 64, stderr contains `--no-such-option`. `PothosUtil --var=bad` -> code 64. `PothosUtil --load-module <a module that does not exist> --success-code 7` -> neither 0 nor 7.
- [ ] **Step 2: Run** -> FAIL.
- [ ] **Step 3: Implement** `Server*` and `PothosUtilOptions.cpp`. Rewrite `PothosUtil.cpp/.hpp` (the code does not need to restore the log configuration after Poco any more). Update the name of the event. Remove the two lines of the allowlist.
- [ ] **Step 4: Run** `/app/tests/*`, `/platform/tests/*`, `/proxy/remote/tests/*` and the complete `ctest` -> PASS.
- [ ] **Step 5: Commit** `feat(app): PothosUtil without Poco::Util — own options, termination wait, daemon and pid file`

---

### Task 4: Windows service

**Files:**
- Modify: `lib/App/Server.hpp`, `lib/App/ServerWindows.cpp`, `lib/App/Builtin/TestServer.cpp`, `apps/PothosUtil.cpp`

**Interfaces (`_WIN32` only):**
- `struct ServiceOptions { std::string name = "PothosUtil", displayName, description, startup = "manual"; std::vector<std::string> args; };`
- `void registerService(const ServiceOptions &opts);` - `CreateServiceW` with the path `"<exe>" --run-service=NAME <args>` (the quotes come from `Platform::buildWindowsCommandLine`). The description uses `ChangeServiceConfig2W`. The errors are `Pothos::SystemException` with the Win32 text (also "Access is denied" without administrator rights).
- `void unregisterService(const std::string &name);`
- `int runAsService(const std::string &name, std::function<int()> main);` - `StartServiceCtrlDispatcherW`. `ServiceMain` reports `SERVICE_RUNNING` and starts `main` in a thread. `SERVICE_CONTROL_STOP`/`SHUTDOWN` -> the termination event (`waitForTerminationRequest` waits for it). The code of `main` -> `SERVICE_STOPPED`.
- `PothosUtil`: `--register-service`/`--unregister-service` run instead of the commands (the other options, except the service options, stay in `args`). `--run-service=NAME` -> `runAsService(NAME, <the usual start of the commands>)`.

- [ ] **Step 1: Test `test_app_windows_service`** (Windows; without administrator rights the test prints `std::cout` "skipped: not elevated" and exits): the name is `PothosUtilTest<pid>`, `registerService({name, ..., args={"--proxy-server=tcp://127.0.0.1:0"}})` -> `StartServiceW` -> the state `SERVICE_RUNNING` in 10 s or less -> `ControlService(STOP)` -> `SERVICE_STOPPED` in 10 s or less -> `unregisterService` -> `OpenServiceW` does not find the service. After an error, the test always unregisters the service. On other platforms the test prints "skipped: Windows only".
- [ ] **Step 2: Run** (locally on Linux, only the compile and the skip. Check RED on Windows CI and record it in the ledger).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the complete `ctest` -> PASS (Linux); Windows is checked in CI.
- [ ] **Step 5: Commit** `feat(app): run PothosUtil as a Windows service`

---

### Task 5: Remove Poco

**Files:**
- Modify: `.gitmodules` and the index (`git rm poco`), `CMakeLists.txt` (the Poco build block, `POCO_IN_TREE`, the install of the Poco libraries, the Poco uninstall script), `cmake/Modules/PothosConfig.cmake` (`POCO_IN_TREE`), `lib/CMakeLists.txt` and `apps/CMakeLists.txt` (the link of `PocoFoundation PocoUtil`), `tests/consumer/CMakeLists.txt` (the regular expression without `Poco`), `.github/workflows/ci.yml` (`git submodule update --init libsimdpp`), `debian/control` (`libpoco-dev`), `README.md`, `CLAUDE.md` (ratchet -> "Poco removed"), `Changelog.txt` (pothos2: Poco removed, the new `PothosUtil` command line, the options of the Windows service), the comments in `lib/Platform` about Poco
- Delete: `cmake/poco-allowlist.txt`, `cmake/PothosCheckPoco.cmake` and the registration of the test `pothos.poco_allowlist`

- [ ] **Step 1: RED:** `grep -rniI 'poco' --exclude-dir={build,.cache,docs,poco,flow,blocks,comms,third_party,.git} .` -> record the number of matches. Historical lines in `Changelog.txt` are allowed.
- [ ] **Step 2: Delete** (`git submodule deinit -f poco && git rm poco`; do not touch `.git/modules/poco`).
- [ ] **Step 3: Clean configurations** `build-clean` (GCC) and `build-clang` -> the complete `ctest` PASS (the number of tests becomes 4: without `pothos.poco_allowlist`). The second grep finds only the history in `Changelog.txt`. Delete the directories.
- [ ] **Step 4: Commit** `build: remove Poco`

---

### Task 6: End of the phase

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 5 is done; the next one is phase 6), the ledger `docs/superpowers/ledgers/2026-10-27-phase5-remove-poco.md`

- [ ] **Step 1: Update the roadmap and the ledger. Commit** `docs: phase 5 complete — Poco removed`
- [ ] **Step 2: Do a fresh review of the complete branch** (the focus is Review Focus). Make the corrections in separate commits with tests.
- [ ] **Step 3: With the consent of the user: push the branch `phase5-pothosutil-without-poco`, make a PR to `pothos2`, and check that CI passes on all four jobs.**

## Out of scope

- The replacement of libsimdpp with xsimd is a separate phase (roadmap).
- The toolkits (`flow`, `blocks`, `comms`) still use Poco themselves. The change of the toolkits is not a goal of the core phases.
