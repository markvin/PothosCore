# Phase 9: build dependencies via find_package (Conan 2 optional), blocks out of the core

Date: 2026-10-28. Base branch: `pothos2` (after PR #10). Work branch: `phase9-conan-deps`.

## Intent

Stop shipping third party code inside the repository. The core finds its
build dependencies the ordinary CMake way (`find_package`), from the system
or from a Conan 2 install; the build scripts know nothing about Conan. The
toolkits (blocks now, comms next) are separate repositories that consume an
installed core and declare their own dependencies the same way.

## Decisions (user)

1. Conan 2 with a simple dependency configuration: a `conanfile.txt`
   (`[requires]` + `[generators] CMakeDeps CMakeToolchain`), no `conanfile.py`.
2. CMake uses `find_package` only and knows nothing about Conan (no
   `conan_*` calls, no `CONAN_*` variables, no checks for the toolchain).
3. Dependencies come from the system, or from Conan by passing
   `-DCMAKE_TOOLCHAIN_FILE=<conan output>/conan_toolchain.cmake`.
4. Toolkit modules (blocks, comms, ...) are external, each with its own
   `conanfile.txt`. **Phase 8 is undone**: `blocks/` leaves the core and
   goes back to its own repository, a fork `markvin/PothosBlocks`.
5. `third_party/` is deleted.
6. The core stays self-contained: build dependencies do not leak into the
   API (installed headers, exported targets, `PothosConfig.cmake`).
7. `include/Pothos/Util/XSIMDTraits.hpp` stays public in the core (it does
   not include xsimd; a module that uses it brings its own xsimd). Modules do
   not get copies of it.
8. Blocks' network code must not depend on core internals: the public
   `include/Pothos/Util/Network.hpp` is extended with `parseUri` and
   `resolveHost` that use std types only (no Asio in the API).
9. CI: the 4 existing jobs (linux-gcc, linux-clang, macos, windows) get
   their dependencies from Conan; one additional job builds the core with
   system packages (Arch Linux container: `asio`, `nlohmann-json`, `xsimd`
   from `extra`) so the "no Conan" path is checked.
10. Scope: core + external blocks. Comms is the next phase, same scheme.
11. `lib/SIMD/XSIMDCopy.hpp` becomes public as `include/Pothos/Util/XSIMDCopy.hpp`
    next to `XSIMDTraits.hpp` (needed by blocks' and later comms' SIMD kernels
    built with `pothos_multiarch`). Like XSIMDTraits it is usable only by a
    module with its own xsimd: the second documented exception to decision 6.

## Goals

- `third_party/` gone; `nlohmann_json`, `asio`, `xsimd` found with `find_package`.
- Root `conanfile.txt` lists the core's three dependencies.
- The installed `PothosConfig.cmake` has no `find_dependency` for them and
  the exported `Pothos::Pothos` target does not reference them
  (`pothos.consumer` keeps checking this).
- Core builds and passes its tests both with Conan and with system packages.
- `markvin/PothosBlocks` branch `pothos2` builds against an installed core,
  with its own `conanfile.txt`, and its self-tests pass in its own CI.

## Non-goals

- Publishing the core (or blocks) as a Conan package (`conanfile.py`, ConanCenter).
- Porting comms or other toolkits (next phases).
- Changing the SIMD dispatch design, the plugin system, or the public API
  beyond decision 8.

## Anti-goals (would be a failure even if it builds)

- Any mention of Conan in CMake files (beyond docs/README/CI).
- A dependency header or target appearing in the installed core API, or
  `PothosConfig.cmake` requiring consumers to find the core's dependencies
  (exceptions: `XSIMDTraits.hpp` and `XSIMDCopy.hpp`, decisions 7 and 11,
  documented as requiring the consumer's own xsimd; `pothos.consumer` must
  skip exactly these two headers in its leak check).
- Downloading during configure/build (FetchContent, ExternalProject,
  `file(DOWNLOAD)`); Conan downloads happen only in an explicit
  `conan install` step before CMake.
- Losing core fixes that came with phase 8 (see below).

## Constraints / current state

- C++23, CMake >= 3.25, must keep working as a subproject (`pothos.subproject`
  test: `add_subdirectory` and FetchContent with `SOURCE_DIR`). The parent
  is responsible for making the dependencies findable; the nested configure
  in `cmake/PothosSubprojectTest.cmake` must forward `CMAKE_TOOLCHAIN_FILE`
  / `CMAKE_PREFIX_PATH` so the test works in both modes.
- Current vendored versions: nlohmann/json 3.11.3, Asio 1.38.2, xsimd 14.3.0.
  ConanCenter: `nlohmann_json/3.12.0`, `asio/1.38.2`, `xsimd/14.2.0` (no 14.3.0).
  Arch `extra`: asio 1.38.2, nlohmann-json 3.12.0, xsimd 14.3.0.
- Current internal targets (`third_party/CMakeLists.txt`): `pothos_json`,
  `pothos_asio` (adds `ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`, on Windows
  `_WIN32_WINNT=0x0A00`, `ws2_32`, `mswsock`), `pothos_xsimd`. Used by
  `lib/CMakeLists.txt` (PRIVATE on `Pothos`, `pothos_net` links `pothos_asio`)
  and `apps/CMakeLists.txt`.
- Packaged Asio has no CMake config (Arch, Debian/Ubuntu). Conan's CMakeDeps
  generates `asio-config.cmake` with target `asio::asio`. The core therefore
  ships its own `cmake/FindAsio.cmake` (header search + version from
  `asio/version.hpp`) that defines the same `asio::asio` target, used only
  when no config package is found. This is plain CMake, not Conan-specific.
- xsimd is used inside the core only: CPU feature detection
  (`lib/System/SIMD.cpp`) and the multi-arch buffer conversion
  (`lib/SIMD/SIMDConvert.cpp` through the private `lib/SIMD/XSIMDCopy.hpp`).
- CI guard "No network access" greps build scripts for
  FetchContent/ExternalProject/`file(DOWNLOAD)`; keep it, drop `third_party`
  and `blocks` from its path list. (It also matches the word in comments.)

### Undoing phase 8 (what goes, what stays)

Phase 8 (PR #10, merge 1f79019d) added `blocks/` and also fixed the core.
Remove: `blocks/`, `POTHOS_BUILD_BLOCKS`, `pothos.blocks.selftests`, the
blocks parts of `CLAUDE.md`, `README.md`, `Changelog.txt`, CI, and restore
the phase-7 wording of the `Topology::commit()` error for missing
`/blocks/copier` / `/blocks/network_*` ("install PothosBlocks"), incl.
`lib/Framework/TopologyToolkitBlocks.hpp`, its test and `Topology.hpp` docs.
Keep: the core fixes from that PR (`lib/Platform/Process*`, `lib/Net/TcpStream.cpp`,
`lib/Plugin/ModuleSafeLoad.cpp`, `TestRemote.cpp`, XSIMDTraits on xsimd 14,
`TestXSIMDTraits.cpp`, `XSIMDCopy.hpp`, SIMD dispatcher utils changes,
`PothosUtil.cmake`/`PothosRunSelfTests.cmake` improvements that are not
blocks-specific). The builder diffs `af8361bc..1f79019d` outside `blocks/`
and decides file by file.

## The core: what changes

- Delete `third_party/`; in `CMakeLists.txt` replace `add_subdirectory(third_party)`
  with `find_package(nlohmann_json ... REQUIRED)`, `find_package(asio ...)`
  (config, then `FindAsio.cmake` fallback), `find_package(xsimd ... REQUIRED)`.
  Link the imported targets PRIVATE; keep the Asio compile definitions and
  Windows libraries on the core's side (they are the core's configuration,
  not the package's). Internal wrapper targets may stay if they make linking
  simpler; they must not be installed/exported.
- Minimum versions in `find_package`: no higher than the lowest version CI
  tests (xsimd 14.2.0 from Conan), and high enough that the code compiles
  (xsimd 14 API; check json/asio minimums against the code). Builder's call.
- Root `conanfile.txt`:
  `[requires] nlohmann_json/3.12.0 asio/1.38.2 xsimd/14.2.0`,
  `[generators] CMakeDeps CMakeToolchain`, plus `[layout] cmake_layout`
  only if it keeps the README command simple (builder's call).
- `include/Pothos/Util/Network.hpp`: add `POTHOS_API` functions
  - `parseUri(std::string_view)` returning a public struct
    `{std::string scheme; std::string host; std::uint16_t port;}`
    (port 0 when absent; IPv6 host without brackets; throws
    `Pothos::InvalidArgumentException`), plus its `toString` if blocks needs it;
  - `resolveHost(const std::string &host, std::chrono::microseconds timeout)`
    returning the address as `std::string` (same semantics as
    `Pothos::Net::resolveHost`: IPv4 first, timeout/IO exceptions).
  Implemented on `lib/Net/Uri` / `lib/Net/Address`, which stay internal.
  Exact names/namespace (`Pothos::Util::`) follow the existing header.
- `pothos.api_compat` / `pothos.consumer` keep passing; add self-tests for
  the new functions.
- Docs: README build section with both ways (system packages; `conan install`
  then `-DCMAKE_TOOLCHAIN_FILE=...`), `CLAUDE.md` updated, Changelog entry.
  `XSIMDTraits.hpp` documented as "include after your own `<xsimd/xsimd.hpp>`".

## CI (core)

- Jobs linux-gcc, linux-clang, macos, windows: install Conan 2 (pip),
  `conan profile detect`, `conan install . --output-folder=... -s build_type=<config>
  --build=missing`, then configure with the toolchain file. The profile must
  use the job's compiler (`CC`/`CXX` / `-s compiler...`); with Ninja on
  Windows the toolchain generator setting must match. Keep
  `POTHOS_WARNINGS_AS_ERRORS` and the existing extras.
- New job `linux-system` in an `archlinux` container: `pacman -S` toolchain +
  `asio nlohmann-json xsimd`, configure without a toolchain file, build, ctest.
- Caching the Conan cache is optional (builder's call).

## PothosBlocks (external)

- Repository: fork `pothosware/PothosBlocks` → `markvin/PothosBlocks`
  (create only with the user's go-ahead at that plan step). Branch `pothos2`
  built from the history of `blocks/` (`git subtree split --prefix=blocks`
  or equivalent) so the phase-8 commits are kept and the upstream history
  stays the ancestor.
- Own top-level `CMakeLists.txt`: `project()`, `find_package(Pothos CONFIG REQUIRED)`,
  `find_package` for its dependencies, the `pothos_blocks_module()` helper
  moved from the core's `blocks/CMakeLists.txt` (minus `${PROJECT_SOURCE_DIR}/lib`
  include path and `pothos_json` → `nlohmann_json::nlohmann_json`).
- Own `conanfile.txt`: `nlohmann_json`, `asio`, `xsimd` (same versions as core).
  The core itself is not a Conan package: it comes from the system or from
  `CMAKE_PREFIX_PATH` pointing at an installed core.
- Uses of core internals to replace:
  - `Net/Uri.hpp`, `Net/Address.hpp`, `Pothos::Net::ipv6Supported`
    → `Pothos/Util/Network.hpp` (`parseUri`, `resolveHost`, `isIPv6Supported`)
    + its own Asio (`asio::ip::make_address`);
  - `SIMD/XSIMDCopy.hpp` (4 files in `stream/SIMD/`) → `<Pothos/Util/XSIMDCopy.hpp>`
    (decision 11);
  - `Util/TempFile.hpp` (`file/TestBinaryFileBlocks.cpp`) → a small local
    helper in the test (builder's call).
- Its own `FindAsio.cmake` copy (CMake module, not C++ code) — or the core
  installs its `FindAsio.cmake`? Builder's call; must not make
  `PothosConfig.cmake` depend on Asio.
- Tests: stage-install blocks into a prefix and run `PothosUtil --self-test1`
  for each blocks test with `POTHOS_ROOT` pointing at the installed core and
  `POTHOS_PLUGIN_PATH` at the staged blocks modules (or install blocks into
  the core's prefix in CI). Reuse the core's `PothosRunSelfTests.cmake`
  approach; copying that CMake script is acceptable.
- CI in the blocks repo: build + install the core from
  `markvin/PothosCore@pothos2` (Conan), then blocks (Conan), run tests; at
  least Linux; same 4-OS matrix if it does not cost much (builder's call).

## Calls made by the designer (not user decisions; check them)

- One phase-9 PR into `pothos2`. Until it is merged, blocks CI builds the core
  from `markvin/PothosCore@phase9-conan-deps`; after the merge it switches to
  `pothos2`. Approval points: creating the fork, each first push (core branch,
  blocks branch), the PR, the merge.
- xsimd floor is 14.2.0 (newest on ConanCenter). If the core uses something
  that appeared in 14.3, the internal code is changed to stay within 14.2;
  no custom Conan recipe. `XSIMDTraits.hpp` only needs `has_simd_register`.
- `Util/TempFile.hpp` in one blocks test is replaced by a local helper.

## Order of work (suggested)

1. Core: `find_package` + `FindAsio.cmake` + `conanfile.txt`, delete `third_party/`
   (blocks still in tree, to keep tests green).
2. Core: public `parseUri`/`resolveHost` and `Pothos/Util/XSIMDCopy.hpp`;
   move blocks' code to them; replace `TempFile.hpp` in the blocks test.
3. Split blocks history into `markvin/PothosBlocks@pothos2`, add its build
   files, conanfile, CI.
4. Core: remove `blocks/` and the phase-8 glue; docs, Changelog.
5. CI: Conan in the 4 jobs + `linux-system`.
