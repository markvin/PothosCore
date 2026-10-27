# CLAUDE.md

Project: PothosCore — local copy of the fork `git@github.com:markvin/PothosCore.git`
(upstream: https://github.com/pothosware/PothosCore).

## Goal

Restore / update the project so it builds and works on a modern toolchain.

## Repository layout

- `lib/`, `include/` — core framework
- Git submodules: spuce, plotters, widgets, flow, soapy,
  blocks, python, audio, comms (none of them is part of the core build)

## Build

Design and plans: `docs/superpowers/specs/2026-10-26-core-modernization-design.md`,
`docs/superpowers/plans/` (roadmap + per-phase plans).

Only the core is built (toolkit submodules are not part of the build).
The core needs no submodule: dependencies are vendored in `third_party/`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

- CMake options are prefixed `POTHOS_` (`POTHOS_BUILD_APPS`, `POTHOS_BUILD_DOCS`,
  `POTHOS_BUILD_TESTS`, ...). The core must work as a subproject
  (`pothos.subproject` test): no global `CMAKE_*` settings (use target
  properties, `pothos_target_settings()`), no generic option/target names,
  `PROJECT_BINARY_DIR`/`PROJECT_SOURCE_DIR` instead of `CMAKE_BINARY_DIR`,
  top-level-only behaviour under `PROJECT_IS_TOP_LEVEL`.
- C++23, CMake >= 3.25 (works on 4.x). No network access during the build;
  `third_party/` holds vendored code (nlohmann/json, standalone Asio, xsimd).
- Internal dependencies (nlohmann/json, Asio, xsimd) are PRIVATE; they
  must never appear in the exported `Pothos::Pothos` target (checked by
  `pothos.consumer`). Poco is gone (phase 5): do not reintroduce it.
- `Pothos::init()` requires an installed layout, so CTest first runs
  `pothos.stage` (`cmake --install` into `build/stage`), then
  `pothos.selftests` runs `stage/bin/PothosUtil --self-tests` with
  `POTHOS_ROOT` and isolated `XDG_*`/`APPDATA` dirs (`cmake/PothosTestEnv.cmake`).
- Single self-test:
  `cmake -DPREFIX=build/stage -DTEST_HOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
- CI: `.github/workflows/ci.yml` (Linux GCC/Clang, macOS, Windows MSVC).
- `build/compile_commands.json` is always generated (clangd finds it there).

## Conventions (modernization)

- Networking goes through the internal layer `lib/Net/` (namespace
  `Pothos::Net`, standalone Asio from `third_party/asio`): `ioContext()` (one
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

## Notes

- Branches: `pothos2` is the integration branch of the modernization (new,
  incompatible API); every phase is merged into it via PR. `master` keeps the
  original API. CI runs on pushes and PRs.
- Remotes: `origin` = fork (markvin), `upstream` = pothosware/PothosCore.
- spuce: the URL in `.gitmodules` (audiofilter/spuce) is dead — the account was
  renamed and the repo is no longer public. The recorded commit `ad08df7` is
  available in the fork https://github.com/hayguen/spuce.git (one-off clone:
  `git -c submodule.spuce.url=https://github.com/hayguen/spuce.git submodule update --init spuce`).
- plotters/qwt6 (nested submodule) is initialized.
