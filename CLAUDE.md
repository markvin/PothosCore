# CLAUDE.md

Project: PothosCore — local copy of the fork `git@github.com:markvin/PothosCore.git`
(upstream: https://github.com/pothosware/PothosCore).

## Goal

Restore / update the project so it builds and works on a modern toolchain.

## Repository layout

- `lib/`, `include/` — core framework
- Git submodules: spuce, poco, plotters, widgets, flow, soapy,
  blocks, python, audio, comms, libsimdpp

## Build

Design and plans: `docs/superpowers/specs/2026-10-26-core-modernization-design.md`,
`docs/superpowers/plans/` (roadmap + per-phase plans).

Only the core is built (toolkit submodules are not part of the build).
Core submodules: `git submodule update --init poco libsimdpp`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

- C++23, CMake >= 3.25 (works on 4.x). No network access during the build;
  `third_party/` holds vendored code (nlohmann/json, standalone Asio).
- Poco is a transitional, PRIVATE, in-tree dependency; it must
  never appear in the exported `Pothos::Pothos` target (checked by
  `pothos.consumer`).
- `Pothos::init()` requires an installed layout, so CTest first runs
  `pothos.stage` (`cmake --install` into `build/stage`), then
  `pothos.selftests` runs `stage/bin/PothosUtil --self-tests` with
  `POTHOS_ROOT` and isolated `XDG_*`/`APPDATA` dirs (`cmake/PothosTestEnv.cmake`).
- Single self-test:
  `cmake -DPREFIX=build/stage -DTEST_HOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
- CI: `.github/workflows/ci.yml` (Linux GCC/Clang, macOS, Windows MSVC).
- `build/compile_commands.json` is always generated (clangd finds it there).

## Conventions (modernization)

- Poco is confined by a ratchet: `cmake/poco-allowlist.txt` lists the only
  files allowed to use Poco (`pothos.poco_allowlist` test). Remove a line as
  soon as a file stops using Poco; never add one. After phase 4 only
  Poco::Util users remain (PothosUtil CLI/server, `PropertyFileConfiguration`);
  Poco Net is not built.
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
- `pothos_platform` and `pothos_net` (`pothos_internal_library()` in
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

<!-- TODO -->

## Notes

- Branches: `pothos2` is the integration branch of the modernization (new,
  incompatible API); every phase is merged into it via PR. `master` keeps the
  original API. CI runs on pushes and PRs.
- Remotes: `origin` = fork (markvin), `upstream` = pothosware/PothosCore.
- spuce: the URL in `.gitmodules` (audiofilter/spuce) is dead — the account was
  renamed and the repo is no longer public. The recorded commit `ad08df7` is
  available in the fork https://github.com/hayguen/spuce.git (one-off clone:
  `git -c submodule.spuce.url=https://github.com/hayguen/spuce.git submodule update --init spuce`).
- Nested submodules poco/gradle and poco/openssl are not initialized (not needed
  for a Linux build); plotters/qwt6 is initialized.
