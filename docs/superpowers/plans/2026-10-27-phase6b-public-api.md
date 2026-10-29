# Phase 6b: public API, "moderate" — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Standard C++20/23 replaces the home-made and old constructs of the public API (`include/Pothos/**`). The abstractions and the signatures that the blocks and the toolkits use do not break. The build of the core has no warnings on GCC and Clang, and CI enforces this.

**Architecture:** Small changes to the headers and to their implementations, one task for each group of constructs:
1. warnings;
2. locks;
3. template utilities and `typedef`;
4. `std::span` in the buffers.

The old public names that the toolkits need (`Util::SpinLock`, `Util::index_sequence` and so on) stay as thin wrappers or aliases over the standard.

**Tech Stack:** C++23 (`std::mutex`, `std::shared_mutex`, `std::atomic_flag::wait/notify`, `std::index_sequence`, `std::unwrap_ref_decay_t`, `std::span`), CMake, `POTHOS_TEST_BLOCK` self-tests.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` ("The depth of the API modernization: ... the standard constructs and signatures replace the old home-made ones (`std::span` in the buffer APIs, `std::optional`, the standard synchronization primitives in place of the home-made ones and so on), but the abstractions (`Object`, `Callable`, `Proxy`, blocks, ports, topologies) do not change"). Decision of the user 2026-10-27: the depth is "moderate" - the signatures that the blocks use do not break, and the namespace `Pothos::` stays.

## Global Constraints

- C++23. Linux GCC/Clang, macOS and Windows MSVC: all CI jobs are green. The tests `pothos.subproject` and `pothos.consumer` pass.
- Do not break the source compatibility for the blocks and toolkits. The public classes, methods and macros that they use stay with the same names and signatures (a new item is added next to the old one). The exceptions are the private members and the items that only the core uses (`Util::SpinLockRW`).
- `std::optional` is not introduced: the public API has no functions with "a result through an output parameter" (checked during the survey), and a replacement for its own sake is a change of the abstractions.
- The check for each task: `cmake --build build && ctest --test-dir build --output-on-failure` -> all PASS.
- Push to `origin` only with the consent of the user.

## Decisions (for approval)

1. **Warnings.**
   - Remove `#include <ciso646>` from `Config.hpp`. In C++20 and later, the alternative tokens (`and`/`not`) are built in. MSVC with `/std:c++latest` enables `/permissive-`, and the tokens work without this header.
   - `POTHOS_TEST_EQUAL` and the related macros compare the C strings by content through `std::string_view`, not by address. The old behavior was also an error: two equal literals in different places could be different.
   - Correct the single warnings in `lib/Util/Expr/Stack.hpp`, `SharedBufferUnix.cpp` and `ThreadConfigUnix.cpp`.
   - A new option `POTHOS_WARNINGS_AS_ERRORS` (the default is OFF, and in CI on GCC and Clang it is ON) applies only to the core targets. This way the warnings do not come back. It is not enabled on MSVC and macOS yet: we cannot check their list of warnings locally, and this is separate work.
2. **Locks.**
   - The internal locks of the ports (`InputPort`/`OutputPort`: the private members and `bufferManagerPush`) change from `Util::SpinLock` to `std::mutex`. The critical sections are short and the lock is mostly not contended. `std::mutex` has a fast path through futex, and under contention the thread sleeps and does not spin.
   - Only the core uses `Util::SpinLockRW` (the registries, Remote, Object). It is replaced with `std::shared_mutex`, and the public header `SpinLockRW.hpp` is deleted.
   - `Util::SpinLock` stays, because the toolkit `comms` uses it (`SimpleLlc.cpp`). But it does not spin idle any more: `lock()` waits with `std::atomic_flag::wait`, and `unlock()` wakes it with `notify_one`.
3. **Template utilities.**
   - The own `integer_sequence`/`GenSeq`/`index_sequence` in `Util/Templates.hpp` become aliases of `std::integer_sequence`/`std::index_sequence`/`std::make_index_sequence`. The core (`Object`, `Callable`) uses `std::` directly.
   - `special_decay_t<T>` becomes an alias of `std::unwrap_ref_decay_t<T>`.
   - `using` replaces `typedef` in `include/`.
4. **`std::span` in the buffers.**
   - `BufferChunk` gets `template <typename T> std::span<T> asSpan(void) const`: the number of elements is `length / sizeof(T)`, and the remaining bytes are not included.
   - `InputPort::buffer()`/`OutputPort::buffer()` do not change: `asSpan` is called on the returned `BufferChunk`.
   - The existing `as<T>()` and the implicit conversion to a pointer stay.

## Review Focus

1. **The blocks and toolkits continue to compile.** After the changes, the public headers compile together with the sources that use `SpinLock`, `as<T>()`, `Util::index_sequence`, `special_decay_t` and `POTHOS_TEST_*`. The test `test_api_compat_sources` (Task 4) compiles a fragment of `comms/mac/SimpleLlc.cpp` and a typical block.
2. **The ports under contention.** The asynchronous messages, the slot calls and the buffers work from many threads. The topology tests in `pothos.selftests` and `test_spinlock_contention` (Task 2), locally under TSan (`build-tsan`).
3. **`POTHOS_TEST_EQUAL`.** It compares `const char*`/arrays/`std::string`/`std::string_view` by content. It does not dereference `nullptr`. The numbers and the other types work as before. Test `test_testing_string_compare` (Task 1).
4. **`asSpan`.** If the length is not a multiple of `sizeof(T)`, the remainder is dropped. An empty `BufferChunk` gives an empty span. It works with `const T` and `T`, and with the complex types. Test `test_buffer_chunk_span` (Task 4).
5. **MSVC without `<ciso646>` and with `/permissive-`.** The Windows CI build checks this. If MSVC does not enable the alternative tokens, return `<ciso646>` only for `_MSC_VER`.

---

### Task 1: Warnings and `POTHOS_TEST_*` on strings

**Files:**
- Modify: `include/Pothos/Config.hpp` (`<ciso646>`), `include/Pothos/Testing.hpp` (the string compare), `lib/Util/Expr/Stack.hpp`, `lib/Framework/SharedBufferUnix.cpp`, `lib/Framework/ThreadConfigUnix.cpp`, `CMakeLists.txt` and `lib/CMakeLists.txt` (the option `POTHOS_WARNINGS_AS_ERRORS` -> `-Werror` on the core targets), `.github/workflows/ci.yml` (`-DPOTHOS_WARNINGS_AS_ERRORS=ON` for linux-gcc and linux-clang)
- Create: `lib/Util/Builtin/TestTesting.cpp`

**Interfaces:**
- `Testing.hpp`: an internal helper `Pothos::Detail::testingValue(const T &)`. For `const char *` / `char[N]` it returns `std::string_view`, and `nullptr` gives `"(null)"` and is not equal to any string. `POTHOS_TEST_EQUAL`/`NOT_EQUAL`/`GT`... use it in place of the direct `(lhs) == (rhs)`.

- [ ] **Step 1: Test `TestTesting.cpp`** (`/testing/tests`): `test_testing_string_compare` - `const char a[] = "abc"; std::string b = "abc";` -> `POTHOS_TEST_EQUAL(a, "abc")`, `POTHOS_TEST_EQUAL(b.c_str(), a)`, `POTHOS_TEST_EQUAL(std::string_view("abc"), a)`, `POTHOS_TEST_NOT_EQUAL(a, "abd")`. The macro with `nullptr` against `"x"` gives NOT_EQUAL without a crash. The numeric compares do not change (`POTHOS_TEST_EQUAL(1, 1.0)`).
- [ ] **Step 2: Run** -> FAIL (`POTHOS_TEST_EQUAL(b.c_str(), a)` compares the addresses).
- [ ] **Step 3: Implement** decision 1.
- [ ] **Step 4: Run** the self-test and the complete `ctest`. Make a clean build with Clang and GCC with `-DPOTHOS_WARNINGS_AS_ERRORS=ON` -> no warnings and no errors. Delete the directories.
- [ ] **Step 5: Commit** `build: no warnings in the core (ciso646, string compare in POTHOS_TEST_*), warnings as errors in Linux CI`

---

### Task 2: Locks on standard primitives

**Files:**
- Modify: `include/Pothos/Util/SpinLock.hpp` (`atomic_flag::wait/notify`), `include/Pothos/Framework/InputPort.hpp`, `InputPortImpl.hpp`, `OutputPort.hpp`, `OutputPortImpl.hpp` (`std::mutex`), `lib/Framework/InputPort.cpp`, `OutputPort.cpp`, `WorkerActor.cpp`, `ActorInterface.hpp`, and all users of `SpinLockRW` in `lib/` (`Object/Convert.cpp`, `Compare.cpp`, `ToString.cpp`, `Hash.cpp`, `Proxy/Convert.cpp`, `Managed/Registry.cpp`, `Plugin/Registry.cpp`, `Plugin/ModulePaths.cpp`, `Remote/Client.cpp`)
- Delete: `include/Pothos/Util/SpinLockRW.hpp`
- Create: `lib/Util/Builtin/TestSpinLock.cpp`

- [ ] **Step 1: Test** `test_spinlock_contention` (`/util/tests`): 8 threads x 100 000 increments under `std::lock_guard<Util::SpinLock>` -> the total is 800 000. `try_lock` on a held lock -> false. A waiting thread does not use time to spin (the CPU time of the process in 200 ms, while one thread holds the lock and 4 threads wait, is less than 100 ms - measured with `std::clock`).
- [ ] **Step 2: Run** -> FAIL (CPU time: the current `lock()` spins).
- [ ] **Step 3: Implement** decision 2.
- [ ] **Step 4: Run** the complete `ctest`. Locally, run `build-tsan` (`-fsanitize=thread`) - `/framework/tests/*` and `/util/tests/test_spinlock_contention` give no reports. Delete the directory.
- [ ] **Step 5: Commit** `refactor: standard locks in ports and registries, SpinLock waits instead of spinning`

---

### Task 3: Template utilities and `typedef`

**Files:**
- Modify: `include/Pothos/Util/Templates.hpp`, `include/Pothos/Object/Object.hpp`, `ObjectImpl.hpp`, `RegisterToString.hpp`, `include/Pothos/Callable/CallableImpl.hpp`, `lib/Object/Builtin/ConvertCommonImpl.hpp`, the tests that use the `Util::` sequences, all `typedef` in `include/`

- [ ] **Step 1: Test** `test_templates_aliases` (`/util/tests`): `static_assert(std::is_same_v<Pothos::Util::index_sequence<0,1>, std::index_sequence<0,1>>)`, `make_index_sequence<3>` = `std::make_index_sequence<3>`, `special_decay_t<std::reference_wrapper<int>>` = `int&`, `special_decay_t<const int&>` = `int`.
- [ ] **Step 2: Run** -> FAIL (the own types are not the same as the standard types).
- [ ] **Step 3: Implement** decision 3 (the core uses `std::` directly; `typedef` -> `using` mechanically).
- [ ] **Step 4: Run** the complete `ctest` -> PASS.
- [ ] **Step 5: Commit** `refactor: standard index sequences and decay in the public templates, using instead of typedef`

---

### Task 4: `BufferChunk::asSpan` and the check of the source compatibility

**Files:**
- Modify: `include/Pothos/Framework/BufferChunk.hpp`
- Create: `lib/Framework/Builtin/TestBufferChunkSpan.cpp`, `tests/api_compat/` (the sample sources: a fragment of `comms/mac/SimpleLlc.cpp` with `Util::SpinLock`, a typical block with `as<T>()`, `special_decay_t`, `Util::index_sequence`, `POTHOS_TEST_*`), the registration of `pothos.api_compat` (a compile against the stage headers, as `pothos.consumer` does)

- [ ] **Step 1: Tests:** `test_buffer_chunk_span` - `BufferChunk(..., 10 bytes)`: `asSpan<std::uint32_t>().size() == 2`, the data is at the same addresses as `as<std::uint32_t *>()`; `asSpan<const float>()`; an empty chunk -> an empty span; `std::complex<float>`. `pothos.api_compat` - the samples compile and link with the installed Pothos.
- [ ] **Step 2: Run** -> FAIL (no `asSpan`).
- [ ] **Step 3: Implement** decision 4.
- [ ] **Step 4: Run** the complete `ctest` -> PASS.
- [ ] **Step 5: Commit** `feat(api): BufferChunk::asSpan, a source compatibility check for blocks`

---

### Task 5: End of the phase

**Files:**
- Modify: `Changelog.txt` (pothos2: `asSpan`, `SpinLockRW` removed, `SpinLock` waits, `<ciso646>` is not included, `POTHOS_TEST_EQUAL` compares strings by content), `CLAUDE.md` (the rules: standard primitives, no warnings), `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 6 is done; the next one is phase 7), the ledger `docs/superpowers/ledgers/2026-10-27-phase6b-public-api.md`

- [ ] **Step 1: Documentation, roadmap, ledger. Commit** `docs: phase 6b complete — public API on standard C++`
- [ ] **Step 2: Do a fresh review of the complete branch** (the focus is Review Focus). Make the corrections in separate commits with tests.
- [ ] **Step 3: With the consent of the user: push the branch `phase6b-public-api`, make a PR to `pothos2`, and check that CI passes on all four jobs.**

## Out of scope

- You can delete the wrappers over the standard (`Util::SpinLock`, the aliases in `Templates.hpp`) when the toolkits move and do not use them any more (phase 7 and later).
- Warnings as errors on MSVC/macOS is separate work, after the analysis of their list.
- The change of `Pothos::Exception` to a `std::` hierarchy and the replacement of `Object` with `std::any` are a change of abstractions, not a goal (spec: anti-goal).
