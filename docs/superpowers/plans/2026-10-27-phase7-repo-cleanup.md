# Phase 7: repository cleanup — plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Only the core stays in PothosCore (`lib`, `include`, `apps`, `cmake`, `tests`, `third_party`, `docs`). All submodules of the toolkits are removed. The core is self-sufficient. Neighbor projects include it through `find_package(Pothos)`, both after the install and as a subproject.

**Architecture:** The toolkits already live in their own repositories of pothosware (PothosBlocks, PothosFlow, ...). This phase only removes their submodules from the core. The port of the toolkits to the API of pothos2 is a separate phase in their repositories. The two run-time dependencies of the core on blocks (`/blocks/copier`, `/blocks/network_sink|source`) stay as plugin paths, but they give a clear error and are documented. For the build "core + toolkits" as subprojects, the core writes a redirect config in `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`.

**Tech Stack:** git, CMake >= 3.25 (`CMAKE_FIND_PACKAGE_REDIRECTS_DIR`, 3.24+), C++23, `POTHOS_TEST_BLOCK` self-tests.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`; roadmap `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (the line of phase 7).

## Decisions of the user (2026-10-27)

1. Toolkits: **only remove** the submodules (spuce, plotters, widgets, flow, soapy, blocks, python, audio, comms) and `sync_toolkits`. The forks and the port of the toolkits are not part of this phase.
2. `/blocks/copier` and `/blocks/network_*`: **document** the dependency on the blocks toolkit and give a clear error message. Do not move them into the core.
3. `debian/`: **delete** (the packages are 0.7.1, `pothos-all` depends on the toolkits; the packaging of pothos2 is later).

## Global Constraints

- Do not touch the common `.git/modules` of the main clone. The submodules are removed only from the tree and from the index of the branch (`git rm`). The directories `.git/modules/*` are not cleaned.
- CMake >= 3.25, C++23. The build does not download anything.
- The public API does not change (except the text of the exceptions).
- CI: the same 4 jobs (linux-gcc, linux-clang with `POTHOS_WARNINGS_AS_ERRORS`, macos, windows).

## Review Focus

1. **A fresh clone without submodules.** `git clone` + build + `ctest` pass. There are no references to the removed directories anywhere (CMake, CI, scripts, documentation). The check is in Task 1, the grep step and the clean build from `git archive`.
2. **A topology with a domain crossing without blocks.** The error is `Pothos::TopologyConnectError`, and the text has the path `/blocks/copier` and the word `PothosBlocks`. The error is not a `PluginRegistryError` without context. Test `test_domain_crossing_needs_blocks` (Task 2).
3. **The network without blocks on a remote host.** The same form of the error for `/blocks/network_sink|source`. The code is in Task 2 (one helper for both paths). There is no separate test (it needs a second host), and the common helper covers it.
4. **`find_package(Pothos)` after `add_subdirectory` of the core and in a neighbor subdirectory.** It finds the in-tree configuration, `Pothos::Pothos` and `POTHOS_UTIL_EXE` are set, and a repeated `find_package` is harmless. In a standalone build, the redirect is not written. The test is an extension of `pothos.subproject` (Task 3).
5. **Documentation.** The README has no section "Toolkits as submodules". It has the repositories of the toolkits and the run-time dependency on blocks. The licenses of the third-party code (kissfft, Qwt, qtcolorpicker) are not listed any more, because their code is not in the tree. Task 4.

---

### Task 1: Remove the submodules, `sync_toolkits`, `debian/`

**Files:**
- Delete: `.gitmodules`, the submodules `spuce plotters widgets flow soapy blocks python audio comms`, `sync_toolkits`, `debian/`
- Modify: everything that the grep below finds (expected: nothing in CMake/CI)

- [ ] **Step 1:** `git rm -r --cached`/`git rm` for the 9 submodules, `.gitmodules`, `sync_toolkits`, `debian`. Delete the directories of the submodules in the working tree (`rm -rf <dir>` - only the working copies in this worktree; do not touch `.git/modules`).
- [ ] **Step 2: Check the references:** `git ls-files | xargs grep -nE '(^|[^/])\b(spuce|plotters|widgets|flow|soapy|python|audio|comms)/|sync_toolkits|debian/'` (exclude `docs/superpowers/`, `Changelog.txt`) -> Expected: empty, except the README (changed in Task 4).
- [ ] **Step 3: Clean build from the archive:** `git archive HEAD` into a scratch directory, configure + build + `ctest` -> Expected: 6/6.
- [ ] **Step 4: Commit** `chore: remove the toolkit submodules, sync_toolkits and the 0.7 debian packaging`

### Task 2: A clear error when a topology needs the blocks toolkit

**Files:**
- Modify: `lib/Framework/TopologyDomainFlows.cpp:85`, `lib/Framework/TopologyNetworkFlows.cpp:40-50`, `include/Pothos/Framework/Topology.hpp` (the doc comment of `commit()`)
- Create: `lib/Framework/TopologyToolkitBlocks.hpp` (internal), `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`

**Interfaces:**
- Produces: `Pothos::Proxy makeToolkitBlock(const Pothos::Proxy &registry, const std::string &path, ArgsType&&... args)` in `TopologyToolkitBlocks.hpp` (internal, a template). It calls `registry.call(path, args...)`. If the block does not exist (`registry.call("doesBlockExist"...)` or a catch of the registry error), it throws `Pothos::TopologyConnectError("Pothos::Topology::commit()", "<path> is needed ... install the blocks toolkit (PothosBlocks)")`.

- [ ] **Step 1: Test** `test_domain_crossing_needs_blocks` (`/framework/tests`): two blocks of the test - a source with an output in the domain `"test_domain_a"`, a sink with an input in `"test_domain_b"`. Both return `"ERROR"` from `getOutputBufferManager`/`getInputBufferManager` for a foreign domain. In the process of the self-test, `/blocks/copier` is temporarily removed from the registry (`PluginRegistry::remove("/blocks/copier")`, if it exists, with a restore). `topology.commit()` throws `Pothos::TopologyConnectError`, and `displayText()` contains `"/blocks/copier"` and `"PothosBlocks"`.
- [ ] **Step 2: Run** -> FAIL (the code throws a plugin registry error without this text).
- [ ] **Step 3:** Add `makeToolkitBlock` and its call in both places. Update the doc comment of `Topology::commit()`: the crossing of incompatible domains and the topologies on many hosts need the blocks toolkit.
- [ ] **Step 4: Run** the complete `ctest` -> PASS.
- [ ] **Step 5: Commit** `feat(topology): a clear error when the blocks toolkit is needed (copier, network flows)`

### Task 3: `find_package(Pothos)` for the neighbors of a core that is a subproject

**Files:**
- Modify: `CMakeLists.txt` (after `find_package(Pothos CONFIG REQUIRED)`), `tests/subproject/CMakeLists.txt`
- Create: `cmake/PothosRedirectConfig.cmake.in` (or a write through `file(WRITE)`)

**Interfaces:**
- Produces: with `NOT PROJECT_IS_TOP_LEVEL`, the file `${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/pothos-config.cmake`. It sets `POTHOS_IN_TREE_SOURCE_DIR` and `POTHOS_BUILD_APPS` to the values of the core, and includes `${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfig.cmake`. It also makes `pothos-config-version.cmake` (the version comes from `POTHOS_ABI_VERSION`/`project(VERSION)`, `SameMajorVersion`).

- [ ] **Step 1: Test:** in `tests/subproject/CMakeLists.txt`, after `add_subdirectory(${POTHOS_SOURCE} pothos)`, add `add_subdirectory(sibling)` (a new directory `tests/subproject/sibling/`, a toolkit project: `find_package(Pothos CONFIG REQUIRED)`, the checks `TARGET Pothos::Pothos`, `POTHOS_UTIL_EXE` not empty with `POTHOS_BUILD_APPS`, the library `add_library(sibling_module MODULE m.cpp)` links with `Pothos::Pothos`). Also add `find_package(Pothos CONFIG REQUIRED)` in the parent itself. The check "no new cache entries without a prefix" continues to pass.
- [ ] **Step 2: Run** `ctest -R pothos.subproject` -> FAIL (`Could not find a package configuration file provided by "Pothos"`).
- [ ] **Step 3:** Write the redirect files (not at the top level only).
- [ ] **Step 4: Run** the complete `ctest` -> PASS. In a standalone build, the redirect file is not created.
- [ ] **Step 5: Commit** `build: find_package(Pothos) finds the core added with add_subdirectory/FetchContent`

### Task 4: Documentation, roadmap, ledger, review, PR

**Files:**
- Modify: `README.md` (without submodules; where the toolkits are: the links to the repositories of pothosware; the run-time dependency on blocks; the build as a subproject and `find_package`; the section of the licenses without kissfft/Qwt/qtcolorpicker), `Changelog.txt`, `CLAUDE.md` (the layout without submodules, delete the notes about spuce/qwt), the roadmap (phase 7 is done), the ledger `docs/superpowers/ledgers/2026-10-27-phase7-repo-cleanup.md`

- [ ] **Step 1: Documentation, roadmap, ledger. Commit** `docs: phase 7 complete — the repository holds only the core`
- [ ] **Step 2: Do a fresh review of the complete branch** (the focus is Review Focus). Make the corrections in separate commits with tests.
- [ ] **Step 3: With the consent of the user: push the branch `phase7-repo-cleanup`, make a PR to `pothos2`, and check that CI passes on all four jobs.**

## Out of scope

- The port of the toolkits to the API of pothos2 (Poco, libsimdpp, json) is in their repositories, in separate phases.
- The packaging of pothos2 (deb/rpm) is later.
- The increase of `POTHOS_ABI_VERSION` (the layout of the ports changed in 6b) is for the release of pothos2.
