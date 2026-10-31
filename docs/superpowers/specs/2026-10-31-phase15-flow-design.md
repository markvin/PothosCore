# Phase 15: PothosFlow on the pothos2 core

Date: 2026-10-31. Status: design. The work has two parts. First, a PR
into the core `pothos2`: a log listener API and the fix of known data
races. Then a new fork `markvin/PothosFlow`, and after it
`markvin/PothosBase`.

## Intent

PothosFlow is the graphical designer of Pothos: a Qt application that
shows the block tree, the graph editor, the properties panel, the host
explorer and the message window. The roadmap ports the toolkits one at a
time. python is skipped for now, thus flow is phase 15 and the last
toolkit of the roadmap.

At this time the user wants only one result: the application starts on
the new core. The user checks it like this: start PothosFlow, the main
window comes into view. Then the menu "?" (Help) → "About Pothos" opens a
modal window. Topologies, graph activation and remote hosts are not in
this phase.

The core work comes first. The message window of flow must get all log
messages, and the new core Logger has no API for this. The user also
decided to fix the known data races of the core in the same PR.

When this phase is complete:

- the core has a public log listener API and has no known data races;
- flow builds against an installed pothos2 core with Qt 6, has no Poco
  and no compiler warnings, starts, shows the main window and the About
  window, and stops without an error;
- an automatic test does this check without a display on Linux, macOS
  and Windows in CI;
- flow is a submodule of PothosBase.

## Decisions of the user

1. python is skipped for now. flow is phase 15 (2026-10-30).
2. The goal of the phase is that the application starts on the new core.
   The check of the user: the main window, then "?" → "About" shows a
   modal window (2026-10-31).
3. The core PR adds a log listener API in this form (2026-10-31):

   ```cpp
   struct LogRecord { LogLevel level; std::chrono::system_clock::time_point time;
                      std::string source; std::string text; };
   static std::size_t Logger::addListener(std::function<void(const LogRecord &)>);
   static void Logger::removeListener(std::size_t id);
   ```

4. The same core PR fixes the known data races (2026-10-31). Refer to
   "Core: data races".
5. Qt 6 only, Qt from the system or from `install-qt-action` in CI, not
   from Conan (the rule of phases 12 and 14).

## Current state (recon on upstream `bab554f`, 2026-10-30)

- A Qt GUI application: 74 `.cpp` files, about 18.7k lines. The
  directories: `AffinitySupport`, `BlockTree`, `ColorUtils`, `EvalEngine`,
  `GraphEditor`, `GraphObjects`, `HostExplorer`, `MainWindow`,
  `MessageWindow`, `PropertiesPanel`, `PothosFlow.cpp` (the `main`).
- A shared library `PothosQtColorPicker` (`qtcolorpicker/`, LGPL, a copy
  of the Qt Solutions color picker). It is installed.
- A module `CoreEditWidgets` (`EditWidgets/`, 11 edit widgets for the
  properties panel), `DESTINATION flow`.
- `Desktop/` (freedesktop files: `.desktop`, MIME XML, icons, install and
  uninstall scripts), `PothosFlowOSX.sh` (the command line script of the
  macOS bundle), `icons/`, `debian/`. No tests.
- CMake: `cmake_minimum_required(VERSION 3.1.0)`, Qt 5 or Qt 6
  (`find_package(QT NAMES Qt6 Qt5)`, components `Widgets Concurrent`;
  flow uses `QtConcurrent`), `cmake_dependent_option(ENABLE_FLOW ...)`
  (a missing Qt turns flow off without an error), links the target
  `Pothos` (the pothos2 core exports `Pothos::Pothos`). A Windows
  `WIN32` executable, a macOS bundle (`MACOSX_BUNDLE`), a UNIX executable
  with `Desktop/` on other systems.
- Poco in 31 files:
  - `Poco::Logger::get(...)` 21 times, and `Poco::Logger &_logger`
    members with calls like `_logger.information(...)`.
  - `Poco::Message` priorities (the Qt message handler of `main`, the
    colors of the message window).
  - `Poco::Optional` 2 times.
  - `Poco::DateTimeFormatter::format` (the time in the message window).
  - `Poco::Environment::set("POTHOS_SYSLOG_ADDR", ...)` in `main`.
  - `Poco::URI`, `Poco::Net::SocketAddress`, `Poco::Net::IPAddress` in
    `EvalEngine/EnvironmentEval.cpp`: the syslog address that a remote
    host uses to send its logs to flow (with the IPv4-mapped case).
  - `MessageWindow/LoggerChannel`: a `Poco::Channel` that flow adds to
    the `SplitterChannel` of the root logger. It sets the root level to
    `PRIO_TRACE` while it is open, and sets the old level again at
    `disconnect()`. It keeps the last 30 messages in a queue; a timer of
    `LoggerDisplay` takes them. The display uses the level, the time, the
    source and the text.
- `main` starts the syslog listener of the core and sets
  `POTHOS_SYSLOG_ADDR` to it. Thus the server processes that flow starts
  send their logs to flow.
- `MainWindow::setupServer` starts a local `Pothos::RemoteServer` (a
  child `PothosUtil` process) on the locator port, and connects to it.
  The destructor of `MainWindow` calls `Pothos::deinit()` and stops the
  server. The constructor emits `initDone` directly, thus
  `handleInitDone` logs "Initialization complete" before the constructor
  returns. The real asynchronous work comes after it: `BlockCache::update`
  queries the server with `QtConcurrent::mapped` and emits
  `blockDescReady` at the end ("Block cache updated"). The host explorer
  trees also use `QtConcurrent`.
- `handleShowAbout` calls `QMessageBox::about`. On Linux and Windows Qt
  shows it with `exec()` (a modal window). On macOS Qt shows it with
  `setModal(false)` and `show()` (not modal). The `offscreen` platform
  does not change this: Qt selects it at compile time.
- Spike (2026-10-30, scratchpad: core `pothos2`, Arch Qt 6.12): 39 of 79
  objects do not compile, all of them because of Poco includes. The other
  objects compile.

## Core: log listener API (decision 3)

The core Logger has an internal record `Detail::LogRecord` (level,
source, text, time, thread id, process id) in `lib/System/LoggerSinks.hpp`,
one active sink, and an asynchronous writer thread. The syslog listener
gives the records from remote processes to `Detail::deliver` without a
level filter.

- Put `LogRecord` into the public header `Pothos/System/Logger.hpp`
  (namespace `Pothos::System`), with the fields of decision 3. The
  internal record becomes this public type (an alias or the same type).
  Decision of the builder: if the public type also has `threadId` and
  `pid`. If it has them, document them.
- Add to `Logger`:
  - `static std::size_t addListener(std::function<void(const LogRecord &)> listener)`
    returns an identifier that is not 0. Identifiers are not used again
    in one process.
  - `static void removeListener(std::size_t id)`. An unknown identifier
    does nothing.
- The rules (write them in the Doxygen comments):
  1. The core calls each listener for each record that it gives to the
     sink: the records that pass the level filter, and the records of the
     syslog listener. The core calls the listeners also when the sink is
     empty (`POTHOS_LOG_CHANNEL=null`) and when syslog forwarding replaces
     the sink.
  2. The core calls the listeners one at a time, in the order of the
     records, from the thread that writes the record (the writer thread
     in asynchronous mode). A listener must return quickly.
  3. After `removeListener` returns, the core does not call that listener
     again, and no call of it continues in an other thread. Thus the
     owner of the listener can delete its data after `removeListener`.
     Do not call `removeListener` from a listener: write this in the
     comment. Decision of the builder: if the core detects it.
  4. The core catches an exception from a listener and ignores it.
  5. A log call from a listener must not cause a deadlock or an infinite
     recursion, in asynchronous and in synchronous mode (`POTHOS_LOG_ASYNC`
     off, and Fatal/Critical). Decision of the builder: the method (for
     example, a thread-local guard that drops the nested record for the
     listeners and gives it only to the sink). Document the result.
  6. After `Logger::flush()` returns, the listeners got all records that
     were logged before the call.
- No managed class registration of the listener calls: a
  `std::function` cannot go to a remote process.
- Self-tests in the core (`/system/tests` or the directory of the
  current logger tests):
  - a listener gets level, time, source and text of a message, after
    `flush()`;
  - a record below the level filter does not go to the listener;
  - a record of `Detail::deliver` (the syslog path) goes to the listener;
  - two listeners get the same records; after `removeListener` the
    removed listener gets nothing;
  - a listener that throws does not stop the other listeners or the
    sink;
  - a listener that logs does not block (sync and async mode).
- `pothos.consumer` and `pothos.api_compat` must pass. The Changelog of
  the core records the new API.

## Core: data races (decision 4)

The known races (ledger of phase 6b, TSan on `/framework/tests`, and the
final review of 6b):

1. `ThreadEnvironment::_waitModeEnabled` (`lib/Framework/ThreadEnvironment.hpp`)
   is a plain `bool`. `registerTask` and `unregisterTask` change it with
   `std::swap` while the worker threads read it. Make it
   `std::atomic<bool>`, and replace the `std::swap` calls with
   `exchange`/`store`.
2. `ActorInterface::_waitModeEnabled` (`lib/Framework/ActorInterface.hpp`)
   is a plain `bool`. `enableWaitMode` (from `Block.cpp`, when the thread
   pool changes) writes it while the worker thread reads it. Make it
   `std::atomic<bool>`.
3. `PluginRegistry::get`, `empty`, `exists` and `list`
   (`lib/Plugin/Registry.cpp`) and `handlePluginEvent` walk the tree with
   `std::map::operator[]` under a `shared_lock`. `operator[]` inserts an
   empty node when a path is missing: two readers write the map at the
   same time. Also an empty node stays in the tree after a read. No
   public call shows it (`dump()` and `list()` walk only
   `nodeNamesOrdered`, and only `add` writes it). Walk the tree with `find` in all
   functions with a `shared_lock`. A missing node gives the same result
   as before (`get` throws, `empty` is true, `exists` is false, `list` is
   empty).
4. `getPluginPaths` (`lib/Plugin/ModulePaths.cpp`) uses `operator[]`
   under a `shared_lock`. Use `find`; a missing module gives an empty
   list.
5. `TestTopology.cpp` (test code): the tests read block counters without
   synchronization while the topology runs (13 TSan reports in 6b). Fix
   the test code so that the TSan run is clean (for example, read the
   counters after `waitInactive`, or with atomics).

Tests:

- A self-test for items 3 and 4 that keeps the public behavior: `get`,
  `empty`, `exists` and `list` on a missing path give the results of
  item 3, and `dump()` is the same before and after these calls. This
  test also passes before the fix: no public call shows the empty nodes.
  Thus the evidence of the fix is the stress test and TSan, not a test
  that fails first (my call; no test hook in the registry).
- A stress self-test: several threads call `get`/`exists`/`list` on
  missing paths while an other thread adds and removes a plugin. It must
  pass; the real check is TSan (next item).
- A local TSan run (GCC or Clang, `-fsanitize=thread`, a separate build
  directory) of the self-tests `/framework/tests` and the plugin and
  system tests. The result: no report about the 5 items. The execution
  ledger records the result. If TSan shows other races, record them and
  ask the user before a fix.
- No TSan job in CI (non-goal).

## Goals

- The core PR (listener API and races) is merged into `pothos2` before
  the flow CI can pass.
- The fork `markvin/PothosFlow`, branch `pothos2`, builds against an
  installed pothos2 core and Qt 6 (6.5 or newer), with CMake 3.25 or
  newer (also CMake 4). No Poco.
- No compiler warnings with `-Wall -Wextra` (GCC and Clang).
  `FLOW_WARNINGS_AS_ERRORS=ON` turns them into errors.
- The smoke test (refer to "Test") passes on Linux, macOS and Windows in
  CI.
- The message window shows the log messages of flow and of its server
  processes, with the same colors and the same time format as before.
- PothosBase builds flow as a submodule and runs its smoke test.
- The core docs record phase 15.

## Non-goals

- No automatic test of topologies (open, save, `.pth` files), of graph
  activation, of the block tree content, of the properties panel or of
  remote hosts. The code of these parts is ported (it must compile
  without Poco), but only the smoke test checks it.
- No new features, no change of the user interface, no change of the
  file format of the topologies.
- No packages (Debian, installer, macOS DMG). No signing of the macOS
  bundle.
- No TSan job in CI.
- No change of the public API of the core other than the listener API.

## Anti-goals

These results are failures, also when the build is correct:

- The configure step continues without Qt 6 and gives a build without
  flow. A missing Qt 6 must stop the configure step with a clear message.
- The smoke test passes because it skips a step without a message, or
  because it does not wait for the end of the initialization.
- The test needs a display, or the test hangs. A hang must stop with a
  timeout and a failure.
- flow stops with a crash, a hang or an error message at exit (the
  `Pothos::deinit()` path, the static Qt objects, the server process). On
  macOS and Windows phase 14 found this type of problem.
- The listener API leaks a dependency (json, Asio, xsimd) into a public
  header, or the listener needs internal headers of the core.
- The race fixes change the behavior of the public API (for example,
  `exists` gives a different result).
- A CMake file refers to Conan, or the configure or build step downloads
  files.
- flow builds in PothosBase but not standalone against an installed core
  (or the opposite).

## Repository and history

1. Fork `pothosware/PothosFlow` to `markvin/PothosFlow`. Create the fork
   only after the user agrees at that step of the plan.
2. Make the branch `pothos2` from upstream `master` (`bab554f`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosFlow` (it exists, upstream clone). Set
   `origin` to the fork (SSH) and `upstream` to pothosware, as in
   plotters.
4. The work goes into a work branch and a PR into `pothos2` of the fork.
   The user approves the fork, the first push and each merge.

## Build (the same method as widgets and plotters)

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosFlow CXX)`,
  C++23 (`CXX_EXTENSIONS OFF`).
- `cmake_policy(SET CMP0144 NEW)` and
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with the
  comment of widgets that tells why.
- `find_package(Qt6 6.5 CONFIG COMPONENTS Widgets Concurrent)` with the
  clear error of widgets. Remove the Qt 5 code and `QEnterEventCompat`
  (use `QEnterEvent`).
- Remove `ENABLE_FLOW` and `cmake_dependent_option`. flow always builds.
- `AUTOMOC` (and `AUTORCC`/`AUTOUIC` if the sources need them) as target
  properties, not global variables.
- Keep the 3 kinds of executable (Windows `WIN32`, macOS bundle, UNIX
  with `Desktop/`). Set the Windows entry point as a target link option,
  not with `CMAKE_EXE_LINKER_FLAGS`.
- Put the sources of the application (all except `PothosFlow.cpp`) into
  an internal static library (not installed). The executable and the
  smoke test link it. Decision of the builder: the name, and how much of
  `main` moves into a function that the test also calls (the Qt message
  handler, the syslog listener, the `QApplication` settings, the main
  window).
- `PothosQtColorPicker` stays a shared library inside flow (my call:
  keep it, refer to "Decisions of the designer"), installed into the
  library directory (`bin` for the DLL on Windows).
- `CoreEditWidgets` stays a module with `DESTINATION flow`.
- The default install prefix is the prefix of the core when the user
  gives no `CMAKE_INSTALL_PREFIX` (as in widgets).
- Options: `FLOW_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `FLOW_WARNINGS_AS_ERRORS` (default OFF).
- Warnings: `-Wall -Wextra` for GCC and Clang, `-Werror` with
  `FLOW_WARNINGS_AS_ERRORS`. Qt headers come as system includes from the
  imported targets. Fix the warnings in the flow code; do not turn them
  off. Warnings in `qtcolorpicker/` (a third party copy) are fixed too;
  if a fix is not possible without a large change, ask the user.
- Run time: the installed executable and module find Qt and
  `PothosQtColorPicker` (an RPATH as in plotters: relative to the
  library directory of the prefix, plus the Qt library directory when it
  is not a system directory). On Windows the Qt `bin` directory is on
  `PATH`.
- flow needs no Conan packages. It has no `conanfile.txt`.
- Delete `debian/`. Keep `Desktop/`, `icons/` and `PothosFlowOSX.sh`.
- Update `README.md` (dependencies: the pothos2 core, Qt 6; build; run
  the test) and `Changelog.txt` (a "Release 0.8.0 (pending)" entry: the
  port to the pothos2 core, Qt 6 only, no Poco).

## Port of the Poco code

- `Poco::Logger::get(name)` and `_logger.xxx(...)` → `POTHOS_LOG_*`
  macros with the same source names (for example
  `"PothosFlow.MainWindow"`). Remove the `_logger` members.
- The Qt message handler in `main` → `Logger::log` with the level from
  `QtMsgType` and the source from the Qt category (same mapping as
  before).
- `Poco::Optional` → `std::optional`.
- `Poco::DateTimeFormatter::format(time, "%H:%M:%s")` → `std::format` with
  the same result as before: UTC (the default of Poco for a
  `Timestamp`), `HH:MM:SS.ffffff` (Poco `%s` gives seconds with
  microseconds). My call: keep the old output (no change of the user
  interface), also if UTC looks strange in a GUI.
- `Poco::Environment::set` → `qputenv`.
- `EnvironmentEval.cpp`: the URI and the socket address →
  `Pothos::Util::parseUri`, `Pothos::Util::resolveHost` and the standard
  library. Keep the behavior: the syslog address of a remote host is the
  peering address with the listener port; for an IPv4-mapped IPv6
  address use the IPv4 form; for a local host use `127.0.0.1`.
- `MessageWindow/LoggerChannel` → a `QObject` with a core listener
  (decision 3). The listener only puts the record into the queue (max 30,
  the oldest goes out). The constructor sets the root level to `Trace`
  and keeps the old level; `disconnect()` removes the listener and sets
  the old level again. `LoggerDisplay` uses `LogRecord` and
  `Pothos::System::LogLevel` for the colors.
- `MainWindow` destructor: the root level `Information` before
  `Pothos::deinit()`, with `Logger::setLevel`.

## Test

- The method of plotters, but flow is an application, not modules: the
  smoke test is an executable of flow, not a self-test of `PothosUtil`.
  1. CTest test `flow.stage` installs flow into `build/stage`.
  2. CTest test `flow.smoke` runs the smoke test executable with
     `POTHOS_ROOT` = the prefix of the core, the flow module from the
     stage, isolated `XDG_*`/`APPDATA` directories (no settings of the
     user), and `QT_QPA_PLATFORM=offscreen`.
- The smoke test does the check of the user (decision 2):
  1. Make the `QApplication` and the main window with the same code as
     `main`.
  2. Wait until the main window is visible and the block cache emits
     `blockDescReady` ("Initialization complete" is logged in the
     constructor, thus it is not a sufficient signal). Then check that
     the block cache has the description of `/blocks/copier` (PothosBlocks
     is in the prefix in CI; this is the only check of the block tree).
     Before step 5, wait until the other `QtConcurrent` work of the start
     is complete (the host explorer trees). Decision of the builder: how
     the test sees this. Timeout: a fixed limit (for example 60 s), then
     fail.
  3. Trigger the action `showAboutAction` (Help → About). Check that a
     visible top-level `QMessageBox` with the title "About Pothos" comes
     into view. On Linux and Windows also check that it is modal
     (`QApplication::activeModalWidget()`). On macOS Qt shows this window
     without modality (refer to "Current state"): the test checks only
     that it is visible, and writes a message that tells why. This is
     not a skip: it is the behavior of Qt on that system (my call; no
     change of the application). Close the window.
  4. Log a message with `POTHOS_LOG_INFO` and check that the message
     window shows it (the core listener on a real user).
  5. Close the main window. The process exits with code 0, without a
     crash and without a hang.
- The test fails on each error at Fatal, Critical or Error level in the
  log during the run. Decision of the builder: if some known errors are
  not failures; each such error is recorded in the ledger and told to the
  user.
- The test does not skip a step without a message.
- Risk: `MainWindow` starts a `RemoteServer` on the fixed locator port.
  Two tests at the same time (for example, PothosBase CTest with `-j`)
  can collide. Mark `flow.smoke` with `RUN_SERIAL` or a
  `RESOURCE_LOCK`, or start the server on a free port in the test.
  Decision of the builder; do not change the behavior of the application.
- Risk: on macOS the test executable is not in the bundle. If Qt or the
  application needs the bundle (icons, menu), stop and ask the user.

## CI of flow

- The same structure as the CI of plotters: linux-gcc (Debug, `-Werror`),
  linux-clang (Release, `-Werror`), macos (Intel runner,
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC Release). The Qt
  versions as in widgets (6.5.3; 6.8.3 on Windows).
  1. Check out flow, `markvin/PothosCore@pothos2` and
     `markvin/PothosBlocks@pothos2`.
  2. Install the dependencies of the core and blocks with Conan.
  3. Install Qt 6 with `install-qt-action`.
  4. Build and install the core, then blocks into the core prefix (their
     own tests off). Blocks give a real block tree at the start.
  5. Build flow and run `ctest -V`.
- Job `linux-system` in an `archlinux` container (`--init`): system
  packages with `qt6-base`, no Conan.
- Keep the guard against downloads in the CMake files.
- Until the core PR is merged, the flow CI can build the core from the
  core work branch (as in phase 14). After the merge, set it back to
  `pothos2`.

## PothosBase

After `pothos2` of the fork has flow:

- Add the submodule `flow` (HTTPS URL, `branch = pothos2`), the options
  `POTHOS_BASE_WITH_FLOW` (ON by default) and `POTHOS_BASE_FLOW_DIR`, and
  `FLOW_WARNINGS_AS_ERRORS` from `POTHOS_BASE_WARNINGS_AS_ERRORS`.
- `base.modules` loads the flow module (`/flow/`). The flow smoke test
  runs from the stage prefix of PothosBase. Decision of the builder: the
  name of the CTest test and how PothosBase runs it.
- CI of PothosBase: no new dependency (Qt 6 is there for widgets).
- README of PothosBase: flow in the list of toolkits.

## Changes in the core

- The listener API and the race fixes (refer to their sections), with
  Changelog entries.
- Roadmap: phase 15 row and status. All toolkits of the roadmap are
  ported, except python (skipped for now).
- `CLAUDE.md`: a short note about the flow fork, its branch, its local
  clone, its dependencies (Qt 6) and its smoke test.

## Decisions of the designer (the user must check them)

- `PothosQtColorPicker` stays inside flow as a shared library (a copy of
  a third party component, LGPL). It does not go to a separate
  repository or to Conan.
- `debian/` is deleted; `Desktop/` stays (the other toolkits have no
  `debian/`).
- The smoke test is a separate test executable that links the flow
  sources as a static library. The installed `PothosFlow` gets no test
  mode or hidden command line option.
- The flow CI builds the core and PothosBlocks (not comms, widgets,
  soapy or plotters).
- The race fixes include the test code of `TestTopology.cpp`, so that the
  TSan run is clean.
- The race check is a local TSan run, not a CI job.
- Changelog entry "Release 0.8.0 (pending)" (upstream is at 0.7.1).
- The About window on macOS is not modal (the behavior of
  `QMessageBox::about` in Qt). The test accepts a visible window there
  and does not change the application.
- The smoke test waits for `blockDescReady` and checks that
  `/blocks/copier` is in the block cache. This is the only check of the
  block tree.
- The time in the message window stays as before: UTC with
  microseconds.
- The registry race fix has no test that fails before the fix. The
  evidence is the stress test and the local TSan run.
- Order: core PR → flow → PothosBase → core docs.

## Recommended order of work

1. Core: the listener API with its self-tests; the race fixes with their
   self-tests; the local TSan run. A PR into `pothos2`.
2. Fork and branch of flow. New CMake, the port of the Poco code, the
   Qt 6 cleanup. The build is clean locally with Arch Qt 6.12, against a
   core with the listener API.
3. The static library, the smoke test, the CTest tests. Manual check by
   the user: start PothosFlow, the main window, Help → About.
4. README, Changelog, delete `debian/`.
5. CI (4 jobs with Conan and `install-qt-action`, and `linux-system`).
   PR into `pothos2` of the fork.
6. PothosBase: the flow submodule, the tests, CI.
7. Core docs: roadmap and `CLAUDE.md`.
