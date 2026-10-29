# PothosCore core modernization — design

Date: 2026-10-26
Branch: `worktree-restore-update`

## Later changes to this design

This document is the first design of the modernization. Later user
decisions changed some parts of it. The current rules are in `CLAUDE.md`
and in the roadmap (`docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`).

- Dependencies (phase 9): the core does not keep its dependencies in the
  source tree. CMake finds nlohmann_json, Asio and xsimd with
  `find_package`: from the system packages or from Conan 2 with its
  toolchain file. CMake files do not refer to Conan. The build does not
  download files. The dependencies stay private. This replaces goal 4,
  the second anti-goal and "Vendored code" below.
- libsimdpp (phase 5a): xsimd replaces libsimdpp.
- Toolkits (phases 7, 9–13): the core repository has no submodules. Each
  toolkit is a separate fork (`markvin/Pothos*`, branch `pothos2`) and
  uses only the public API of the core. PothosBase (`markvin/PothosBase`)
  builds the core and the ported toolkits in one CMake tree.
- Namespace (phase 6): the namespace `Pothos::` stays.

## Why

PothosCore is a framework for stream data processing (blocks, ports,
topologies, plugins, remote execution). Nobody maintains the project. It
builds as C++11 with `cmake_minimum_required(VERSION 3.1.0)`. CMake 4.x
(the local version is 4.4.4) does not configure such projects. The core
has Poco as a public dependency. It downloads `json.hpp` from the internet
during the configure step.

The goal is a modern, self-contained C++23 library from this core. Later,
the modules (toolkits) will use this library.

## Goals

1. **C++23.** All code of the core compiles as C++23. Standard constructs
   replace old constructs (`std::format`, `std::filesystem`, `std::span`,
   `std::any`, `std::optional`, `<random>`, `<regex>`, `<chrono>` and
   others).
2. **Modern CMake.** The build uses targets, without global
   `include_directories`/`add_definitions`. The package export is correct
   (`find_package(Pothos CONFIG)` and an imported target, for example
   `Pothos::Pothos`). A new CMake 4.x builds the project.
3. **No Poco.** Poco is fully removed: from the sources, from the build,
   from `PothosConfig.cmake` and from the submodules of the core.
4. **Self-contained.** The build needs only CMake and a C++ compiler. If
   a dependency is necessary, it is in the source tree. Only the library
   uses it, and it **does not get into the public API**: not into the
   headers `include/Pothos/**` and not into the exported CMake
   dependencies. (Phase 9 changed this goal: refer to "Later changes".)
5. **Platforms:** Linux, macOS, Windows.
6. **CI on GitHub Actions** in the fork (`markvin/PothosCore`): the build
   and the self-tests on all three platforms.

## Non-goals

- Modules (`blocks`, `comms`, `audio`, `soapy`, `python`, `flow`,
  `widgets`, `plotters`): do not change them now, port them later. The
  core must build without them. `ENABLE_TOOLKITS` is off by default or
  removed; the builder decides.
- A change of the architecture: the model of blocks, ports and
  topologies, `Object`, `Callable`, `Proxy`, the `Plugin` registry and
  `Archive` stay as concepts.
- API/ABI backward compatibility is not necessary. The API can change to
  use standard types. The modules will follow later.

## Branches and version

- The modernization occurs in a separate integration branch **`pothos2`**.
  Each phase goes into it through a PR. `master` keeps the original API.
- **Open question:** the API changes in an incompatible way. Thus large
  changes are possible, also of the namespace (for example `Pothos` →
  `Pothos2`, or a versioned inline namespace), of the library name, of
  the CMake package name and of the header directory. The decision occurs
  before phase 6 (the public API modernization). Until then, new code
  does not put the namespace name in places where a change is difficult.
  (Phase 6 decision: `Pothos::` stays.)

## Future goal (out of the current scope)

**A web UI in place of PothosFlow:** a topology editor in the browser (an
SPA) and an HTTP/WebSocket server on Asio (for example a mode
`PothosUtil --web-server`). It uses the JSON interfaces that the core has
now (topologies, block descriptions, statistics) and `EvalEnvironment` on
the server side. In the future, a GUI block of `widgets`/`plotters` is a
pair: a server block and a browser component, with a WebSocket stream.
Security starts at the beginning: by default only localhost, and
authentication.

**Requirement for the current work (the network phase):** design the
internal network layer on Asio so that an HTTP/WebSocket server can go on
top of it without a redesign. Use one shared io_context and its threads,
and a connection abstraction that is not tied to the Remote protocol. Do
not implement HTTP/WebSocket now.

## Anti-goals (a failure, also when it "works")

- A type from an internal dependency (Asio, muparserx, json, libsimdpp)
  is visible in a public header, or `PothosConfig.cmake` requires it from
  a consumer.
- The build downloads something from the network, or it requires
  libraries installed in the system. (Phase 9 changed the second part:
  the dependencies come from `find_package`. The first part stays.)
- A rewrite of the architecture "at the same time" (a new scheduler
  model, a full replacement of `Object` with `std::any`, and similar).
- A loss of the Remote functions or of the support for one of the three
  platforms.

## Current state (results of the analysis)

- The core: approximately 35 thousand lines in `lib/`, `include/`, `apps/`.
- **The public headers do not include Poco.** The only leak is a macro in
  `include/Pothos/Exception.hpp:58` that catches `Poco::Exception`.
- But Poco is a public dependency at the CMake level: `PothosConfig.cmake`
  does `include(SetupPoco)`. The modules use Poco directly (mostly
  `Poco::Logger`/`poco_error(...)`).
- Poco use in `lib/` and `apps/` (the number of `#include` lines):
  - the standard can replace: `Format` (37), `Path` (17), `File` (13),
    `StringTokenizer` (9), `String` (8), `NumberFormatter` (7), `Types`,
    `TemporaryFile`, `Random`/`RandomStream`, `HashMap`, `Hash`,
    `ByteOrder`, `RegularExpression`, `Glob`, `NumberParser`, `Exception`,
    `AutoPtr`, `Bugcheck`;
  - own platform wrappers are necessary for: `SharedLibrary`
    (Plugin/Module, Loader, JIT), `Process`/`Pipe`/`PipeStream`
    (ModuleSafeLoad, the JIT compiler, HostInfo, Remote Server,
    PothosUtil), `Environment`, `Platform`, `URI`;
  - logs: `Logger` and the channels (25): `lib/System/Logger.cpp`, and
    calls in all of the core;
  - network (`lib/Remote/*`, `lib/Util/Network.cpp`, `System/Logger.cpp`):
    `TCPServer`, `ServerSocket`, `StreamSocket`, `SocketStream`,
    `SocketAddress`, `DatagramSocket`, `RemoteSyslogChannel/Listener`;
  - `Poco::Util` (`Application`, `ServerApplication`, `OptionSet`,
    `HelpFormatter`, `PropertyFileConfiguration`): `apps/PothosUtil*`;
  - XML DOM: only `lib/Framework/TopologyDotMarkup.cpp`.
- Other dependencies:
  - `json.hpp` (nlohmann, v2.1.1): the configure step downloads it;
  - `muparserx`: a submodule (v4.0.8, `cmake_minimum_required(2.8.12)`),
    used in `lib/Util/EvalEnvironment.cpp`;
  - `libsimdpp`: a submodule (`cmake_minimum_required(2.8.0)`), used in
    `lib/SIMD`, `lib/System/SIMD.cpp`, `cmake/Modules/PothosConfigSIMD.cmake`;
  - `spuce`: the core does not use it (only the modules need it). The
    URL of the submodule is dead (refer to CLAUDE.md).
- **Test of 2026-10-26 (GCC 16.2, CMake 4.4):** with
  `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, `-DCMAKE_SKIP_INSTALL_RULES=ON`
  (without it, the export fails: Poco is in `INTERFACE_INCLUDE_DIRECTORIES`)
  and a forced `-std=gnu++23` for `lib/`/`apps/`, the core with the old
  Poco 1.8.1 **builds without errors**. There are approximately 2400
  warnings, 90% of them are `-Wtemplate-id-cdtor`. Only the build of the
  muparserx example fails. `PothosUtil --self-tests` (with a temporary
  install root, `POTHOS_ROOT` and isolated `XDG_*` directories): **87 of
  88 pass**. `/framework/tests/test_mmap_shared_buffer` fails
  (`TestSharedBuffer.cpp:86`: the file size is 0, not 4096). This defect
  was there before the modernization. `Pothos::init()` requires an
  installed layout (`bin/PothosUtil`, the library, `include/`). Thus the
  self-tests do not run directly from the build tree.
- Tests: the macro `POTHOS_TEST_BLOCK` registers the self-tests as
  plugins. `PothosUtil --self-tests` runs them.
- Old CI: `.travis.yml`, `appveyor.yml`.

## Decisions (made by the user)

- The C++23 standard; the platforms Linux, macOS, Windows.
- Poco is fully removed.
- **Remote stays.** Rewrite it on an own network layer on top of
  **standalone Asio**, vendored into the source tree. Asio is not visible
  in the public API. (Phase 9: Asio comes from `find_package`.)
- Make the public API modern. Keep the architecture. Replace old
  constructs with standard ones.
- CI: GitHub Actions.
- **Compatibility of the external interfaces with old installations is
  not necessary:**
  - the Remote network protocol and the `Archive` serialization format can
    change. A new client does not have to work with an old
    `PothosUtil --proxy-server`;
  - the command line syntax of `PothosUtil` can change (`--opt=value` and
    the Windows syntax `/opt` are not necessary). Update the code in the
    core that calls it (`lib/Remote/Server.cpp`).
  - **Exception:** the log forwarding over UDP stays **compatible with
    syslog**. rsyslog/journald read the messages, and the listener accepts
    standard syslog.
- **All modes of `PothosUtil` stay** (now `Poco::Util::ServerApplication`
  gives them, `apps/PothosUtil.hpp:12`):
  - a correct stop on SIGTERM / SIGINT / Ctrl-C (as
    `waitForTerminationRequest`);
  - Unix: `--daemon`, `--pidfile`, `--umask`;
  - Windows: register, unregister and run as a Windows service (as
    `/registerService`, `/unregisterService`; the option names can change
    to one common style).
- **An own expression evaluator replaces muparserx** (C++23, no
  dependencies), behind the public API `Pothos::Util::EvalEnvironment`.
  Syntax compatibility with muparserx and with the expressions in saved
  PothosFlow topologies (`.pothos`) is **not necessary**. The `muparserx`
  submodule is removed.
  - Necessary functions (from `lib/Util/Builtin/TestEvalExpression.cpp`):
    bool, integers, float (double), complex numbers, strings with escapes,
    octal literals; arithmetic and math functions (`pow` and others);
    lists `[...]` and dictionaries `{k: v}` as full values (nested, with a
    trailing comma); element-wise operations on lists
    (`2*[1,2,3] + [3,2,1]`); constants from expressions and from
    `Pothos::Object` (`registerConstantExpr`/`registerConstantObj`/
    `unregisterConstant`). The result is a `Pothos::Object` (a list gives
    a `ProxyVector`, a dictionary gives a `ProxyMap`, as now). An error
    gives a `Pothos::Exception` with a clear message (the position of the
    error in the expression).
  - **Syntax (my decision):** Python-like expressions: the literals
    `True`/`False`, the imaginary unit `j` (and the suffix `1j`),
    `and`/`or`/`not`, comparisons, `**` for the power, `a if c else b`.
    Octal literals have the form that the current test
    `test_eval_octal_constant` expects. Where the new syntax is different,
    update the tests.
- **Tests:** a self-test can depend on the CI environment, not on the port
  (the JIT compiler at run time, IPv6, NUMA/CPU info and similar). You can
  disable such a test on one platform. Write the reason in a comment in
  the code or in CI. A separate phase to build the old tree with Poco for
  a baseline is not necessary.

## Proposals accepted as sensible defaults (approved in general)

- **The log system of the core does not use Quill (user decision,
  2026-10-26).** It stays possible to add Quill later. The public API does
  not depend on the implementation. The output goes through an internal
  sink interface. Quill can become one of the sinks without an API change.
  The own logger has an asynchronous mode (a background thread and a
  queue), so the work threads do not wait for I/O.
- **Logs:** an own public `Pothos` log API on `std::format`. The levels
  stay as now: fatal/critical/error/warning/notice/information/debug/trace.
  The loggers have names. This API replaces `Poco::Logger` in the core and,
  later, in the modules. These environment variables stay:
  `POTHOS_LOG_LEVEL`, `POTHOS_LOG_CHANNEL`, `POTHOS_LOG_FILE`. These
  functions of `Pothos::System::Logger` stay: `startSyslogListener`,
  `startSyslogForwarding`, `forwardStdIoToLogging`, `setupDefaultLogging`.
  The log forwarding over UDP uses Asio.
- **Platform layer** (internal, not public): load shared libraries
  (dlopen / LoadLibrary), start processes with pipes (posix_spawn or
  fork+exec / CreateProcess), environment variables, platform data.
- **`PothosUtil` stays** (the Remote server, the self-tests, the other
  commands). Rewrite the argument parser and the server mode without
  `Poco::Util`.
- **Vendored code:** `json.hpp` (a current version of nlohmann/json, one
  header), standalone Asio and libsimdpp are in the tree (muparserx is
  removed, refer to the text above). The builder selects the form (a
  submodule or a copy in `third_party/`). The build must not require the
  network, except for `git submodule update`. If the old CMake code of a
  vendored library is not compatible with CMake 4.x, the core builds it
  with its own CMake code. (Phase 9 replaced this item: refer to "Later
  changes".)
- **spuce:** remove it from the build of the core.
- **Minimum compilers (my decision):** GCC 13+, Clang 17+, Apple Clang
  from Xcode 15+, MSVC from Visual Studio 2022 17.8+. Use only the C++23
  functions that all of them support. For example, check
  `std::print`/`std::expected` in the support tables. If a function is
  missing, use `std::format` and a stream output.
- **Depth of the API modernization (my decision, from the user's words
  "keep the architecture, replace old constructs with standard ones"):**
  examine all of `include/Pothos/**`. Replace old home-made constructs and
  signatures with standard ones (`std::span` in buffer APIs,
  `std::optional`, standard synchronization primitives in place of
  home-made ones, and similar). Do not change the abstractions (`Object`,
  `Callable`, `Proxy`, blocks, ports, topologies).
- **macOS (my decision):** the minimum version is macOS 13.3. Apple libc++
  needs it for `std::format`/`std::to_chars` with floating point. CI:
  arm64 (`macos-latest`). x86_64 has no tests, but do not break it on
  purpose.
- **Completion criterion:** the core configures and builds "from zero" (a
  new clone and the submodules of the core, no network during the build,
  no system libraries except the standard OS libraries).
  `PothosUtil --self-tests` passes on all three platforms in CI (except
  the tests that depend on the environment and are disabled explicitly).
  The Remote tests (`lib/Remote/Builtin/TestRemote.cpp`) are enabled.

## Left to the builder

- The order of the work and its division into phases. An intermediate
  state can keep Poco for some time, if this keeps the tests green.
- The exact form of the log API and of the platform wrappers.
- The CMake structure (target names, options, install layout, pkg-config,
  uninstall, Doxygen), if it satisfies the goals above.
- What to do with `debian/`, `.travis.yml`, `appveyor.yml` (probably
  remove the old CI; keep debian as it is or mark it as old).
- A replacement for the XML DOM in `TopologyDotMarkup.cpp` (probably a
  simple string generator for DOT/HTML labels).
