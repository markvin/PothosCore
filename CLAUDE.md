# CLAUDE.md

Project: PothosCore — local copy of the fork `git@github.com:markvin/PothosCore.git`
(upstream: https://github.com/pothosware/PothosCore).

## Goal

Restore / update the project so it builds and works on a modern toolchain.

## Repository layout

- `lib/`, `include/` — core framework; `apps/` — PothosUtil; `cmake/` —
  build support (installed `PothosConfig.cmake`, `PothosUtil.cmake`);
  `tests/` — CTest projects; `cmake/PothosDependencies.cmake` — finds the
  dependencies; `conanfile.txt` — the dependencies for Conan 2
- No submodules. The toolkits are in their own repositories
  (pothosware/Pothos*). The local clones of the toolkits and of PothosBase
  are next to the clone of the core (paths `../Pothos*`). PothosBlocks
  (ported, phase 9) is the fork markvin/PothosBlocks, branch `pothos2`,
  local clone `../PothosBlocks`. The topology needs `/blocks/copier` and
  `/blocks/network_*` from PothosBlocks; without it commit() gives a clear
  error. PothosComms (ported, phase 10) is the fork
  markvin/PothosComms, branch `pothos2`, local clone
  `../PothosComms`. Its self-tests need PothosBlocks in the
  prefix of the core
- PothosWidgets (ported, phase 12) is the fork markvin/PothosWidgets,
  branch `pothos2`, local clone `../PothosWidgets`. It
  needs Qt 6 (6.5 or higher) from the system or from the Qt installer
  (`install-qt-action` in CI), not from Conan. Its self-tests use the Qt
  platform `offscreen` and need no display
- PothosSoapy (ported, phase 13) is the fork markvin/PothosSoapy, branch
  `pothos2`, local clone `../PothosSoapy`. It needs
  SoapySDR 0.8 or higher from the system or built from the upstream
  source (CI: tag `soapy-sdr-0.9.0` of pothosware/SoapySDR), not from
  Conan. Its self-tests need no SDR hardware: a test driver of SoapySDR
  (`pothos_soapy_test`) is registered only while a test runs.
  `-DSOAPY_TEST_DEVICE=driver=...` adds a test with a real device. CI
  uses only upstream pothosware/SoapySDR
- PothosBase (phase 11) is the aggregate build: repository
  markvin/PothosBase (branch `main`), local clone
  `../PothosBase`. The core, blocks, comms, widgets and
  soapy are submodules on `pothos2`, built with `add_subdirectory`.
  `-DPOTHOS_BASE_CORE_DIR=...` (and `_BLOCKS_DIR`, `_COMMS_DIR`,
  `_WIDGETS_DIR`, `_SOAPY_DIR`) builds a local clone in place of a
  submodule. `ctest` runs all self-tests from one stage prefix. A toolkit
  must build both standalone and in PothosBase; its `find_package(Pothos)`
  uses `NO_PACKAGE_ROOT_PATH` (CMP0144 NEW)

## Build

Design and plans: `docs/superpowers/specs/2026-10-26-core-modernization-design.md`,
`docs/superpowers/plans/` (roadmap + per-phase plans).

Dependencies (nlohmann_json, Asio, xsimd) come from `find_package`: system
packages, or Conan 2 with its toolchain file. CMake files must not refer to
Conan.

```sh
conan install . --output-folder=build/conan -s build_type=Debug -s compiler.cppstd=23 --build=missing
cmake -S . -B build/conan-dbg -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake
cmake --build build/conan-dbg
ctest --test-dir build/conan-dbg --output-on-failure
```

Without `CMAKE_TOOLCHAIN_FILE` the system packages are used (Arch: `asio
nlohmann-json xsimd`; Asio has no CMake config: `cmake/Findasio.cmake`).

- CMake options are prefixed `POTHOS_` (`POTHOS_BUILD_APPS`, `POTHOS_BUILD_DOCS`,
  `POTHOS_BUILD_TESTS`, ...). The core must work as a subproject
  (`pothos.subproject` test): no global `CMAKE_*` settings (use target
  properties, `pothos_target_settings()`), no generic option/target names,
  `PROJECT_BINARY_DIR`/`PROJECT_SOURCE_DIR` instead of `CMAKE_BINARY_DIR`,
  top-level-only behaviour under `PROJECT_IS_TOP_LEVEL`.
- C++23, CMake >= 3.25 (works on 4.x). No download during configure or
  build (no FetchContent/ExternalProject/`file(DOWNLOAD)`).
- Dependencies (nlohmann/json, Asio, xsimd) are PRIVATE through the internal
  wrapper targets `pothos_json`, `pothos_asio`, `pothos_xsimd`; they must
  never appear in the exported `Pothos::Pothos` target or in
  `PothosConfig.cmake` (checked by `pothos.consumer`, which compiles each
  public header alone; only `XSIMDCopy.hpp` is skipped). Minimum versions:
  json 3.11, asio 1.38, xsimd 14.2 (ConanCenter has no 14.3: do not use
  xsimd 14.3 API without a fallback). Poco is gone (phase 5): do not
  reintroduce it.
- `Pothos::init()` requires an installed layout, so CTest first runs
  `pothos.stage` (`cmake --install` into `build/stage`), then
  `pothos.selftests` runs `stage/bin/PothosUtil --self-tests` with
  `POTHOS_ROOT` and isolated `XDG_*`/`APPDATA` dirs (`cmake/PothosTestEnv.cmake`).
- Toolkits use only the public API: the network utilities for them are
  `Pothos::Util::parseUri`/`resolveHost` (`Pothos/Util/Network.hpp`).
- SIMD: a file compiled once per architecture (`pothos_multiarch`,
  `PothosGenerateSIMDSources`) includes `<Pothos/Util/XSIMDCopy.hpp>` first (own xsimd
  namespace per copy, AVX-512F workaround), never `<xsimd/xsimd.hpp>`
  directly; `Pothos::Util::XSIMDTraits<T>` follows the copy's xsimd arch.
  Both headers are public; the toolkit gives its own xsimd.
- Single self-test:
  `cmake -DPREFIX=build/stage -DTEST_HOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
- CI: `.github/workflows/ci.yml` (Linux GCC/Clang, macOS, Windows MSVC with
  Conan; `linux-system` with the Arch packages, without Conan).
- `build/compile_commands.json` is always generated (clangd finds it there).

## Conventions (modernization)

- Networking goes through the internal layer `lib/Net/` (namespace
  `Pothos::Net`, standalone Asio from `find_package(asio)`): `ioContext()` (one
  shared io_context + background thread), `Uri`/`HostPort` parsing,
  `resolveHost`, `TcpStream` (blocking `std::iostream`), `TcpServer` (thread
  per connection, protocol agnostic), syslog (`formatSyslog`/`parseSyslog`,
  `SyslogSender`, `SyslogListener`; RFC 5424 with `[pothos@32473 ...]`).
  Asio is included only from `lib/Net/` and never from `include/`. Built as the
  internal static library `pothos_net`. Tests: `/net/tests`.
- OS-specific code goes into the internal platform layer `lib/Platform/`
  (namespace `Pothos::Platform`: environment, host info, `DynamicLibrary`,
  `Process`/`Pipe`), built as the internal static library `pothos_platform`.
- `pothos_app` (`lib/App/`, namespace `Pothos::App`): PothosUtil support —
  command line parsing and help (`Options`), server modes (`Server`). Tests:
  `/app/tests`.
- `pothos_platform`, `pothos_net` and `pothos_app` (`pothos_internal_library()` in
  `lib/CMakeLists.txt`) are compiled once and linked into both libPothos and
  PothosUtil, because libPothos exports no internal symbols. They are PIC,
  hidden visibility, use the Pothos API as clients (no `POTHOS_DLL_EXPORTS`)
  and are never installed. Their self-tests stay in libPothos (`Builtin/`).
- Internal helpers: `lib/Util/StringUtils.hpp`, `FileSystem.hpp`,
  `TempFile.hpp` (namespace `Pothos::Util::Detail`). Not installed.
- Child processes: `Pothos::Platform::ProcessOptions`; to ignore a child's
  output set `discardOutput` (null device). Never pass pipes and close them:
  the child dies with SIGPIPE on its first print (that broke module safe load).
- `Pothos::Net::TcpStream`: a read never flushes the output (remote calls
  read and write one stream from several threads); writers flush.
- Logging: `POTHOS_LOG_ERROR("My.Source", "fmt {}", value)` etc. from
  `<Pothos/System/Logger.hpp>` (std::format, async writer, Fatal/Critical
  synchronous, `Logger::flush()`). Sinks/config internals:
  `lib/System/LoggerSinks.hpp`. Env: `POTHOS_LOG_LEVEL`, `POTHOS_LOG_CHANNEL`
  (color/console/file/null), `POTHOS_LOG_FILE`, `POTHOS_LOG_FORMAT`,
  `POTHOS_LOG_TIMEZONE`, `POTHOS_LOG_ASYNC`, `POTHOS_SYSLOG_ADDR`.
- Expressions (`Pothos::Util::EvalEnvironment`): own evaluator in
  `lib/Util/Expr/` (namespace `Pothos::Util::Expr`: lexer → parser → AST
  evaluator over `Value`, `Convert` to/from `Pothos::Object`). Not installed.
  Python-like syntax, a superset of JSON; documented in the Doxygen comment of
  `include/Pothos/Util/EvalEnvironment.hpp`. Tests: `lib/Util/Builtin/TestExpr.cpp`,
  `TestEvalExpression.cpp`.

## Conventions

- Public API (`include/Pothos/**`): prefer the standard primitives
  (`std::mutex`, `std::shared_mutex`, `std::span`, `std::index_sequence`, ...)
  over home-made ones. Keep names that toolkits use (`Util::SpinLock`,
  `Util::index_sequence`, ...) as thin aliases/wrappers; do not break block
  sources — `pothos.api_compat` (`tests/api_compat/`) builds old-style code
  against the staged install.
- `using` instead of `typedef`.
- No warnings: the Linux CI builds with `-DPOTHOS_WARNINGS_AS_ERRORS=ON`
  (GCC and Clang); fix warnings, do not silence them.

## Writing style: ASD-STE100

All English text in the repository is written in ASD-STE100 Simplified
Technical English: code comments, Doxygen, README, Changelog, docs (specs,
plans, ledgers), commit messages and PR descriptions. Identifiers, code,
command lines and quoted messages stay as they are.

- Use approved words with their one approved meaning. Technical names (API,
  types, tools, protocols) and technical verbs are allowed.
- Sentences: procedures max 20 words, descriptions max 25 words. One
  instruction per sentence; one topic per paragraph, max 6 sentences.
- Procedures (steps, how-to): imperative mood ("Run the self-tests").
  Use numbered lists for steps.
- Active voice in procedures, and as much as possible in descriptions.
- Simple verb tenses only (present, past, future). No "-ing" forms as
  verbs or nouns, except in technical names.
- Do not omit articles ("the", "a") and verbs to make text shorter.
- No more than 3 nouns in a noun cluster; split longer ones.
- Write a warning or a condition before the instruction it applies to.

## Notes

- Branches: `pothos2` is the integration branch of the modernization (new,
  incompatible API); every phase is merged into it via PR. `master` keeps the
  original API. CI runs on pushes and PRs.
- Remotes: `origin` = fork (markvin), `upstream` = pothosware/PothosCore.
- spuce (filter design library of the toolkits, no longer a submodule): the old
  URL audiofilter/spuce is dead; commit `ad08df7` is in the fork
  https://github.com/hayguen/spuce.git. Comms does not use spuce: it has its own
  filter design code (`filter/design/`, a port of `ad08df7`). Plotters still
  uses spuce until its phase.
