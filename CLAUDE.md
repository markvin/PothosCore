# CLAUDE.md

Project: PothosCore — local copy of the fork `git@github.com:markvin/PothosCore.git`
(upstream: https://github.com/pothosware/PothosCore).

## Goal

Restore / update the project so it builds and works on a modern toolchain.

## Repository layout

- `lib/`, `include/` — core framework
- Git submodules: spuce, muparserx, poco, plotters, widgets, flow, soapy,
  blocks, python, audio, comms, libsimdpp

## Build

Design and plans: `docs/superpowers/specs/2026-10-26-core-modernization-design.md`,
`docs/superpowers/plans/` (roadmap + per-phase plans).

Only the core is built (toolkit submodules are not part of the build).
Core submodules: `git submodule update --init poco muparserx libsimdpp`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

- C++23, CMake >= 3.25 (works on 4.x). No network access during the build;
  `third_party/` holds vendored code (nlohmann/json).
- Poco and muparserx are transitional, PRIVATE, in-tree dependencies; they must
  never appear in the exported `Pothos::Pothos` target (checked by
  `pothos.consumer`).
- `Pothos::init()` requires an installed layout, so CTest first runs
  `pothos.stage` (`cmake --install` into `build/stage`), then
  `pothos.selftests` runs `stage/bin/PothosUtil --self-tests` with
  `POTHOS_ROOT` and isolated `XDG_*`/`APPDATA` dirs (`cmake/PothosTestEnv.cmake`).
- Single self-test:
  `cmake -DPREFIX=build/stage -DHOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
- CI: `.github/workflows/ci.yml` (Linux GCC/Clang, macOS, Windows MSVC).

## Conventions

<!-- TODO -->

## Notes

- Remotes: `origin` = fork (markvin), `upstream` = pothosware/PothosCore.
- spuce: the URL in `.gitmodules` (audiofilter/spuce) is dead — the account was
  renamed and the repo is no longer public. The recorded commit `ad08df7` is
  available in the fork https://github.com/hayguen/spuce.git (one-off clone:
  `git -c submodule.spuce.url=https://github.com/hayguen/spuce.git submodule update --init spuce`).
- Nested submodules poco/gradle and poco/openssl are not initialized (not needed
  for a Linux build); plotters/qwt6 is initialized.
