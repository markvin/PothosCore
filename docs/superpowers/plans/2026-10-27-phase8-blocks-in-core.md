# Phase 8: blocks in the core tree — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The PothosBlocks toolkit lives in `blocks/` of the PothosCore tree. It builds together with the core (option `POTHOS_BUILD_BLOCKS`, ON) on the API of pothos2, without Poco and without its own copies of the third-party libraries. `/blocks/copier` and `/blocks/network_sink|source` ship with the core.

**Architecture:** A subtree merge puts the history of PothosBlocks (upstream `pothosware/PothosBlocks` master `4aef23f`) into `blocks/`. The blocks modules are built with `POTHOS_MODULE_UTIL` inside the project of the core. They link the internal targets of the core: `pothos_json` (nlohmann/json), `pothos_xsimd` (the SIMD kernels of stream), `pothos_net` -> `pothos_asio` (the network blocks). These targets are still not installed: the modules are part of the build of the core. `std::` replaces Poco (`std::format`, `std::byteswap`, `std::filesystem`, `<random>`, `<thread>`), and the API of the core also replaces it (`Pothos::System::log`, `lib/Net`).

**Tech Stack:** git subtree merge, CMake >= 3.25, C++23, standalone Asio 1.38.2, xsimd 14.3.0, nlohmann/json 3.11.3, `POTHOS_TEST_BLOCK` self-tests.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`. This phase reconsiders the decision of phase 7 (`docs/superpowers/plans/2026-10-27-phase7-repo-cleanup.md`, decision 2) for blocks.

## Decisions of the user (2026-10-27)

1. Do not duplicate the third-party libraries in the `third_party` of the toolkits: **move blocks into the tree of the core**, and use the `third_party` of the core.
2. The fork `markvin/PothosBlocks` and the clone `/home/mark/Projects/PothosBlocks` are not used. Their future is a separate question to the user (do not delete anything without an answer).

## Decisions that the plan proposes (approved together with the plan)

1. **The history stays**: a subtree merge (`git merge -s ours --allow-unrelated-histories` + `git read-tree --prefix=blocks/`), about 600 commits with the authorship. The PR to `pothos2` is merged with a merge commit (as #7-#9). Otherwise the history is lost.
2. One option `POTHOS_BUILD_BLOCKS` (ON; with `POTHOS_BUILD_APPS=OFF` the modules are built without the JSON docs). The single options `ENABLE_BLOCKS_*` are removed.
3. The network code and the SIMD code of blocks are rewritten, not wrapped: there will be no Poco-compatible layers.
4. The data format on the wire of the network blocks (the headers of SocketEndpoint, the byte order) does not change.

## Global Constraints

- Nothing is downloaded during the build. The CI guard `No network access` is extended to `blocks`.
- `pothos_json`, `pothos_xsimd`, `pothos_asio` and `pothos_net` are not installed and not exported. The installed public headers do not include them.
- After this phase, `blocks/` has no `#include <Poco/...>`, no `Poco::` and no `POCO_`.
- The plugin paths and the names of the blocks and tests of blocks do not change (`/blocks/copier`, `/blocks/tests/test_*` and so on). The modules are installed in `lib/Pothos/modules${POTHOS_ABI_VERSION}/blocks`.
- `POTHOS_WARNINGS_AS_ERRORS` applies to the blocks modules in the same way as to the core (linux-gcc, linux-clang).
- CI: the same 4 jobs (linux-gcc, linux-clang, macos, windows), all green.
- Do not touch the common `.git/modules` of the main clone. Do not use a bare `git stash`.

## Review Focus

1. **A build without apps or without blocks.** `-DPOTHOS_BUILD_BLOCKS=OFF` gives the old core (the error of phase 7 about the missing copier still works). `-DPOTHOS_BUILD_APPS=OFF` builds blocks without `--doc-parse`. The check is in Task 2, the configuration step of both variants.
2. **The network on IPv6 and the port 0.** `tcp://[::1]:0` and `udp://[::1]:0` with BIND get a real port that `getActualPort` returns, and a CONNECT to it transfers data. Test `test_network_blocks_ipv6` (Task 6). The test is skipped only when `ipv6Supported()` from `lib/Net/Address.hpp` is false.
3. **A connection break and the close of the topology.** The stop of a topology with a connected but silent TCP peer does not hang (deactivate unblocks the waiting read). Test `test_network_deactivate_unblocks` (Task 6) with a timeout.
4. **The SIMD path really runs.** On x86_64 and arm64 the stream tests pass, both with the SIMD dispatch and on the scalar path. The dispatcher does not select an architecture that the CPU does not support. Task 5: the test `test_simd_dispatch_matches_scalar` compares the results of both paths on a tail that is not a multiple of the batch width.
5. **A domain crossing inside a process.** A topology between the ports of incompatible domains now commits and transfers data through `/blocks/copier`. Test `test_copier_domain_crossing` (Task 4).

---

### Task 1: Merge the history of PothosBlocks into `blocks/`

**Files:**
- Create: `blocks/**` (the upstream tree `4aef23f`)
- Delete: `blocks/debian/`
- Modify: - (the root `CMakeLists.txt` does not include `blocks/` yet)

- [ ] **Step 1:** `git fetch https://github.com/pothosware/PothosBlocks master` and check that `FETCH_HEAD` = `4aef23f...`.
- [ ] **Step 2:** subtree merge:
  ```bash
  git merge -s ours --no-commit --allow-unrelated-histories FETCH_HEAD
  git read-tree --prefix=blocks/ -u FETCH_HEAD
  git commit -m "blocks: import PothosBlocks history (pothosware/PothosBlocks@4aef23f)"
  ```
- [ ] **Step 3:** `git rm -r blocks/debian` and the commit `blocks: drop debian packaging`.
- [ ] **Step 4: Check.** `git log --oneline -- blocks/stream/Copier.cpp | wc -l` > 1; `cmake --build build && ctest --test-dir build` -> the same result as before (6/6), `blocks/` is not part of the build.

### Task 2: The build of blocks in the core, the module packet

**Files:**
- Create: `blocks/CMakeLists.txt` (rewrite), `blocks/packet/CMakeLists.txt` (rewrite)
- Modify: `CMakeLists.txt` (the option, `add_subdirectory(blocks)`, ctest), `cmake/PothosRunSelfTests.cmake`, `.github/workflows/ci.yml` (guard: `+ blocks`), `blocks/packet/*.cpp` (`<json.hpp>` -> `<nlohmann/json.hpp>`)

**Interfaces:**
- Produces: the function `pothos_blocks_module(TARGET <name> SOURCES <...> [LIBRARIES <...>] [TESTS <test_name>...])` in `blocks/CMakeLists.txt`. It calls `POTHOS_MODULE_UTIL(... DESTINATION blocks [ENABLE_DOCS with POTHOS_BUILD_APPS])`, links `pothos_json`, adds `blocks/` to the include paths, applies `pothos_warnings_as_errors`, and appends `/blocks/tests/<test_name>` to the global property `POTHOS_BLOCKS_TESTS`. All the next tasks register the modules only through it.
- Produces: `PothosRunSelfTests.cmake` accepts `-DTEST_PATHS=<;-list>` and runs `--self-test1` for each path. It fails on the first failed test. The ctest `pothos.blocks.selftests` (FIXTURES_REQUIRED `pothos_stage`) passes `POTHOS_BLOCKS_TESTS` to it.

- [ ] **Step 1: Test.** In the root `CMakeLists.txt`, register `pothos.blocks.selftests` with `TEST_PATHS=/blocks/tests/test_packet_blocks;/blocks/tests/test_packet_to_stream;/blocks/tests/test_stream_to_packet` (temporarily as a list; after Step 3 it comes from the property).
- [ ] **Step 2: RED.** `ctest --test-dir build -R pothos.blocks` -> FAIL: `--self-test1 /blocks/tests/test_packet_blocks` does not find the plugin.
- [ ] **Step 3:** Add the option `option(POTHOS_BUILD_BLOCKS "Build the blocks toolkit (blocks/)" ON)`. Write `blocks/CMakeLists.txt` without `project()`/`find_package`, with `pothos_blocks_module`. Add the module `PacketBlocks` with `TESTS` of the three tests above. The ctest takes the list from the property.
- [ ] **Step 4: GREEN.** `ctest --test-dir build --output-on-failure` -> all tests pass, including `pothos.blocks.selftests`.
- [ ] **Step 5: Configuration variants.** `cmake -S . -B build-noblocks -DPOTHOS_BUILD_BLOCKS=OFF` builds, ctest has no `pothos.blocks.*`, and `test_domain_crossing_needs_blocks` passes. `cmake -S . -B build-noapps -DPOTHOS_BUILD_APPS=OFF` builds.
- [ ] **Step 6: Commit** `blocks: build in the core tree, packet module`.

### Task 3: The modules event, serialize, testers without Poco

**Files:**
- Modify: `blocks/{event,serialize,testers}/CMakeLists.txt` (through `pothos_blocks_module`), `blocks/event/{Evaluator,MessagePrinter}.cpp`, `blocks/serialize/{Serializer,Deserializer}.cpp`, `blocks/testers/{Abort,CollectorSink,TestConstantSource}.cpp`, `<json.hpp>` in all tests of these modules

The replacements (common also for Task 4-6): `Poco::format` / `NumberFormatter` -> `std::format`; `Poco::ByteOrder` -> `std::byteswap` + `std::endian`; `Poco::Logger` -> `Pothos::System::log` (`include/Pothos/System/Logger.hpp`, the same source name); `Poco::Thread::sleep` -> `std::this_thread::sleep_for`; `Poco::Random`/`RandomStream` -> `std::mt19937` / `std::random_device`; `SingletonHolder` -> a static local variable of a function; `Poco/Types.h`, `Platform.h`, `Foundation.h` -> `<cstdint>` or delete.

- [ ] **Step 1: RED.** Add the modules `EventBlocks`, `SerializeBlocks`, `TesterBlocks` to the build with their `TESTS` (all `POTHOS_TEST_BLOCK` from these directories, see the grep `POTHOS_TEST_BLOCK` in them). The build fails on `Poco/*.h`.
- [ ] **Step 2:** Replace Poco with the table above.
- [ ] **Step 3: GREEN.** `cmake --build build && ctest --test-dir build -R 'pothos.blocks|pothos.selftests' --output-on-failure` -> PASS. `grep -rn Poco blocks/{event,serialize,testers}` is empty.
- [ ] **Step 4: Commit** `blocks: port event, serialize and testers off Poco`.

### Task 4: The modules file and stream (scalar path), copier

**Files:**
- Modify: `blocks/{file,stream}/CMakeLists.txt`, `blocks/file/{BinaryFileSource,MemoryMappedBufferContainer,TestBinaryFileBlocks,TextFileSink}.cpp`, `blocks/stream/{Clamp,Round,TestFirstN}.cpp`, `<json.hpp>` in the tests
- Create: `blocks/stream/TestCopier.cpp`

The replacements in addition to Task 3: `Poco::File` -> `std::filesystem`; `Poco::TemporaryFile` -> a unique path in `std::filesystem::temp_directory_path()` with a delete in an RAII guard of the test. In this task, stream builds without `SIMD/` (as upstream without xsimd).

- [ ] **Step 1: Test `test_copier_domain_crossing`** in `blocks/stream/TestCopier.cpp`: two blocks with an output port and an input port of the domains `copier_test_a`/`copier_test_b` (each one throws `PortDomainError` for a foreign domain, as in `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`), with 1000 `int` elements between them. `topology.commit()` does not throw, `topology.waitInactive()` returns, and the received buffer is equal to the sent buffer.
- [ ] **Step 2: RED.** The modules `FileBlocks`, `StreamBlocks` in the build with their `TESTS` + `test_copier_domain_crossing` -> the build fails on Poco.
- [ ] **Step 3:** Replace Poco.
- [ ] **Step 4: GREEN.** ctest as in Task 3 -> PASS (including the core test `test_domain_crossing_needs_blocks`, which removes the copier from the registry itself).
- [ ] **Step 5: Commit** `blocks: port file and stream off Poco, in-process domain crossing works`.

### Task 5: The SIMD kernels of stream on xsimd 14

**Files:**
- Modify: `blocks/stream/CMakeLists.txt` (`PothosGenerateSIMDSources(... SIMD/StreamBlocks.json ...)`, `LIBRARIES pothos_xsimd`, `POTHOS_XSIMD`), `blocks/stream/SIMD/{Clamp,IsX,MinMax,Round}.cpp`, `blocks/stream/{Clamp,IsX,MinMax,Round}.cpp` (the dispatcher)
- Create: `blocks/stream/TestSIMDDispatch.cpp`

**Interfaces:**
- Consumes: `PothosGenerateSIMDSources(<var> <json> <inputs...>)` from `cmake/Modules/PothosConfigSIMD.cmake` (include it with `include(${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfigSIMD.cmake)`), `Pothos/Util/XSIMDTraits.hpp`.

- [ ] **Step 1: Test `test_simd_dispatch_matches_scalar`**: for `clamp`, `minmax`, `round`, `isnan/isinf` on `float` and `double`, with the length `4*batch+3` and values with signs, +/-inf and NaN, the result of the SIMD function that the dispatcher selected is the same as the scalar result (bit-identical for isX, equal for the others).
- [ ] **Step 2: RED.** Add the SIMD generation -> the build fails on the old API (`xsimd::simd_type`/`set_simd`/`batch_bool<T,N>`).
- [ ] **Step 3:** Port the SIMD kernels to xsimd 14: `xsimd::batch<T, Arch>`, `xsimd::batch_bool<T, Arch>`, `load_unaligned`/`store_unaligned`, `xsimd::select`, `xsimd::isnan`. Remove from the JSON the architectures of `StreamBlocks.json` that xsimd 14 does not have.
- [ ] **Step 4: GREEN.** ctest -> PASS on x86_64 locally. `ninja -t targets all | grep StreamBlocks_SIMD` shows the generated files.
- [ ] **Step 5: Commit** `blocks: stream SIMD kernels on xsimd 14`.

### Task 6: The network blocks on Asio

**Files:**
- Modify: `blocks/network/CMakeLists.txt` (`LIBRARIES pothos_net`), `blocks/network/{SocketEndpoint.hpp,SocketEndpoint.cpp,DatagramIO.cpp,NetworkSource.cpp,NetworkSink.cpp,TestNetworkBlocks.cpp}`

**Interfaces:**
- Consumes: `lib/Net/Uri.hpp` (the parse of `tcp://host:port`, `udp://...`), `lib/Net/Address.hpp` (`parseHostPort`, `resolveHost`, `isWildcard`, `ipv6Supported`), `pothos_asio`.
- Produces: the same interface `PothosPacketSocketEndpoint` from `SocketEndpoint.hpp`, public for the module (the constructor `(uri, opt)`, `openComms`, `closeComms`, `getActualPort`, `send`/`recv` with a timeout). The implementation uses its own `asio::io_context` for each endpoint, and the timeouts use `run_for`.

- [ ] **Step 1: Tests.** `test_network_blocks_ipv6` - a copy of `test_network_blocks` on `[::1]`, skipped with a message when `ipv6Supported()` is false. `test_network_deactivate_unblocks` - a sink with BIND, a raw TCP client connects and stays silent, and `topology.commit()` and then the destructor of the topology finish in 5 s or less (a watchdog thread as in `TestReentrantPlugins.cpp`).
- [ ] **Step 2: RED.** The module `NetworkBlocks` in the build with `TESTS test_network_blocks test_network_blocks_ipv6 test_network_deactivate_unblocks test_network_topology` -> the build fails on `Poco/Net/*`.
- [ ] **Step 3:** Rewrite `SocketEndpoint.cpp` and `DatagramIO.cpp` on Asio (tcp: acceptor/socket; udp: a socket with connect/bind, the MTU as before). The format of the headers and the byte order do not change.
- [ ] **Step 4: GREEN.** ctest -> PASS, including `test_network_topology` (a topology through processes with `/blocks/network_*`). `grep -rn Poco blocks` is empty.
- [ ] **Step 5: Commit** `blocks: network blocks on Asio`.

### Task 7: The core and the documentation after the move

**Files:**
- Modify: `lib/Framework/TopologyToolkitBlocks.hpp` (the text of the error), `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`, `include/Pothos/Framework/Topology.hpp` (the doc of `commit()`), `README.md`, `CLAUDE.md`, `Changelog.txt`, `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`, `blocks/README.md`
- Keep: `blocks/Changelog.txt` (the release history of PothosBlocks)

- [ ] **Step 1: Test.** In `test_domain_crossing_needs_blocks`, replace the expectation `"PothosBlocks"` with `"POTHOS_BUILD_BLOCKS"` (the other expectations - `/blocks/copier`, `in this process` - do not change).
- [ ] **Step 2: RED.** ctest `pothos.selftests` -> FAIL on the new expectation.
- [ ] **Step 3:** The message: `"... needs the block <path> from the blocks module, which is not installed <location> (build the core with POTHOS_BUILD_BLOCKS=ON)"`. The doc of `commit()` and the README: blocks are part of the core, the option is `POTHOS_BUILD_BLOCKS`. Remove PothosBlocks from the list of the external toolkits, and rewrite the section about the dependency on the toolkit. Changelog/roadmap: the line of phase 8. The license of blocks is BSL-1.0, the same as the license of the core.
- [ ] **Step 4: GREEN.** The complete `ctest --test-dir build --output-on-failure` -> PASS. `grep -rn "PothosBlocks" README.md CLAUDE.md include lib` finds only the historical reference to upstream in `blocks/README.md`.
- [ ] **Step 5: Commit** `core: blocks ship with the core, update docs and error text`.
