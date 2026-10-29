# Phase 5a: libsimdpp to xsimd — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The core does not use libsimdpp any more. The CPU feature detection and the SIMD buffer conversion use xsimd (a copy in `third_party/xsimd`). The build for many architectures uses the own `PothosConfigSIMD.cmake`. The `libsimdpp` submodule is removed. The toolkits do not change.

**Architecture:**
- `lib/System/SIMD.cpp` detects the CPU features with `xsimd::cpu_features`. It returns the old names (`x86_sse2`, ..., `neon`), because these names are the keys of the dispatchers of the toolkits.
- `lib/SIMD/SIMDConvert.cpp` is built in many copies with `pothos_multiarch` from `PothosConfigSIMD.cmake`, as the SIMD files of the toolkits. Each copy puts the implementation in its own namespace `POTHOS_SIMD_NAMESPACE`.
  - If the input type and the output type have the same number of elements in a register and xsimd supports them, the conversion uses `xsimd::batch_cast`.
  - Otherwise the conversion is a `static_cast` loop. The compiler vectorizes the loop with the flags of this copy.
- An own dispatcher (`lib/SIMD/SIMDConvertDispatch.cpp`) selects the best copy one time with `getOptimalSIMDFeatureSetKey`. CMake generates the list of the copies.

**Tech Stack:** C++23, xsimd 14.3.0 (headers only, BSD-3-Clause), CMake/CTest, `POTHOS_TEST_BLOCK` self-tests.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (vendoring of the dependencies into the tree, privacy, anti-goals). Roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 5a - decision of the user: xsimd as a copy in `third_party/xsimd`).

## Global Constraints

- C++23. Linux GCC/Clang, macOS and Windows MSVC: all four CI jobs are green.
- xsimd is not visible in `include/Pothos/**`, and the consumer does not need it. The test `pothos.consumer` gets `xsimd` in the regular expression in place of `simdpp`. The public `XSIMDTraits.hpp` stays as it is, because it does not include xsimd.
- The build does not download anything (xsimd is in the tree).
- The public API `Pothos::System::getSupportedSIMDFeatureSet()` / `getOptimalSIMDFeatureSetKey()` and the format of the keys (`x86_sse2__x86_avx2`, `fallback`) do not change. The toolkits (`blocks`, `comms`) build without changes.
- The result of `simdConvertBuffer<In, Out>` is bit-identical to the element-by-element `static_cast<Out>(in[i])` for all supported type pairs and for all buffer lengths.
- The check for each task: `cmake --build build && ctest --test-dir build --output-on-failure` -> all PASS.
- Push to `origin` only with the consent of the user.

## Decisions (for approval)

1. **The architecture names stay:** `x86_sse2`, `x86_sse3`, `x86_ssse3`, `x86_sse4_1`, `x86_popcnt`, `x86_avx`, `x86_avx2`, `x86_fma3`, `x86_fma4`, `x86_avx512f`, `x86_avx512bw`, `x86_avx512dq`, `x86_avx512vl`, `neon`, `neonfltsp`, `power_altivec`, `power_vsx_206`, `power_vsx_207`. The order "from slow to fast" also stays.
   - **`x86_xop` is not detected any more**: xsimd does not have it, and it is for AMD Bulldozer 2011-2015. On such CPUs the dispatchers of the toolkits select a neighbor architecture. There are no errors.
   - **`mips_msa` is not detected any more**: xsimd does not support MIPS.
2. **xsimd 14.3.0** - only `include/`, as a copy in `third_party/xsimd`, a private INTERFACE target `pothos_xsimd`, not installed. The toolkits still find xsimd themselves with `find_package(xsimd)`.
3. **`SIMDConvert` on xsimd.**
   - `xsimd::batch_cast` is used when `batch<In>::size == batch<Out>::size` and xsimd supports both type pairs (for example, `int32<->float`, `int64<->double`, `int8<->uint8`).
   - Otherwise the code is a `static_cast` loop that the compiler vectorizes (we enable `-O3`/`/O2` for this copy of the file).
   - As now, `float/double -> integer` always goes through a scalar `static_cast`. This way it is easier to guarantee the match with `static_cast`.
4. **The multi-architecture build of the core uses the functions of `PothosConfigSIMD.cmake`** (`pothos_get_compilable_archs` + `pothos_multiarch`), as the toolkits do, in place of `simdpp_get_runnable_archs`.
   - This changes the behavior: the build makes copies for all architectures that the compiler can build, not only for the architectures that can run on the build machine. The dispatcher still selects at run time.
   - The cost is more copies of one file in the build.
5. **An own dispatcher in place of `SIMDPP_MAKE_DISPATCHER`.** CMake generates `SIMDConvertArches.hpp` with the list of the namespaces of the copies. `SIMDConvertDispatch.cpp` selects one time (static) the table of function pointers of the best copy. The fallback is the `fallback` copy without SIMD flags.

## Review Focus

1. **The match with `static_cast` on all type pairs and lengths.**
   - Types: `char`, `int8...int64`, `long`, `long long`, their unsigned variants, `float`, `double`. The complex pairs are converted by parts.
   - Lengths: 0, 1, the length of a register +/- 1, odd lengths, 1000.
   - Values on the limits of the ranges: an integer narrowing gives a wrap by modulo, as `static_cast` does in C++20.

   Test `test_simd_convert_matches_scalar` (Task 2).
2. **Unaligned pointers and a crossing of the buffer limit.** The input and the output are at an offset of +1 byte, and the tail is smaller than a register. There is no access outside the limits. Test `test_simd_convert_unaligned` (Task 2; run it locally under ASan).
3. **The CPU detection is the same as in libsimdpp** on the build machine, except for `x86_xop` and `mips_msa`. Test `test_simd_features_match_simdpp` (Task 1) exists only while libsimdpp is in the tree. Task 3 removes it.
4. **Selection of the copy:** for a list with an unsupported architecture, `getOptimalSIMDFeatureSetKey` selects the best supported one. For an empty list it selects `fallback`. The `SIMDConvert` dispatcher reports the selected key, and the key is in the set of the supported keys. Test `test_simd_optimal_key` (Task 1), `test_simd_convert_dispatch` (Task 2).
5. **The build on all compilers:** the MSVC architecture flags (`/arch:AVX2`, `/arch:AVX512`), Apple clang on arm64 (`neon`), no architectures (`fallback`) - CI on four jobs. Task 3 - clean GCC and Clang builds locally.

---

### Task 1: xsimd in the tree and CPU detection

**Files:**
- Create: `third_party/xsimd/` (`include/`, `LICENSE`, `README.md` with the version 14.3.0 and the source), `lib/System/Builtin/TestSIMDFeatures.cpp`
- Modify: `third_party/CMakeLists.txt` (target `pothos_xsimd`), `lib/CMakeLists.txt` (link `pothos_xsimd` PRIVATE), `lib/System/SIMD.cpp`, `README.md`, `CLAUDE.md` (vendored code)

**Interfaces:**
- `pothos_xsimd`: INTERFACE, `SYSTEM` include `third_party/xsimd/include`.
- `std::vector<std::string> Pothos::System::Detail::detectSIMDFeatures(void);` (internal `lib/System/SIMDFeatures.hpp`) - the detection with `xsimd::cpu_features` as in decision 1, in the order from slow to fast. `getSupportedSIMDFeatureSet()` caches its result. `getOptimalSIMDFeatureSetKey()` does not change.

- [ ] **Step 1: Copy xsimd** (tag `14.3.0`, `include/` + the license) and add the target `pothos_xsimd`.
- [ ] **Step 2: Tests `TestSIMDFeatures.cpp`** (`/system/tests`):
  - `test_simd_feature_names`: each name from `getSupportedSIMDFeatureSet()` is in the list of decision 1. On x86-64 `x86_sse2` is present, and on aarch64 `neon` is present. The list is sorted from slow to fast (the index in the list of decision 1 grows).
  - `test_simd_optimal_key`: `getOptimalSIMDFeatureSetKey({})` -> `"fallback"`. `{"x86_sse2", "<all supported, joined with __>", "no_such_arch"}` -> the key from all supported ones. A key with only an unsupported architecture -> `"fallback"`.
  - `test_simd_features_match_simdpp` (temporary, while libsimdpp is in the tree): `detectSIMDFeatures()` = the set that `simdpp::get_arch_*` gives through the old table (moved into the test), without `x86_xop`/`mips_msa`.
- [ ] **Step 3: Run** -> FAIL (no `lib/System/SIMDFeatures.hpp` / `detectSIMDFeatures`).
- [ ] **Step 4: Implement** on xsimd. The old libsimdpp table stays only in the test.
- [ ] **Step 5: Run** the self-tests `/system/tests/test_simd_*` and the complete `ctest` -> PASS.
- [ ] **Step 6: Commit** `feat(simd): vendored xsimd, CPU feature detection on xsimd`

---

### Task 2: `SIMDConvert` on xsimd and the own dispatcher

**Files:**
- Modify: `lib/SIMD/SIMDConvert.cpp` (the implementation on xsimd in `namespace POTHOS_SIMD_NAMESPACE`), `lib/SIMD/SIMDConvert.hpp`, `lib/CMakeLists.txt` (the block "Generate dynamic dispatch files": `include(PothosConfigSIMD)` from `cmake/Modules`, `pothos_get_compilable_archs` + `pothos_multiarch`, the generation of `SIMDConvertArches.hpp`), `lib/Framework/Builtin/TestBufferConvert.cpp`
- Create: `lib/SIMD/SIMDConvertDispatch.cpp`, `lib/SIMD/SIMDConvertArches.hpp.in`, `lib/SIMD/Builtin/TestSIMDConvert.cpp`

**Interfaces:**
- `template <typename In, typename Out> void simdConvertBuffer(const void *in, void *out, size_t len);` - the signature and the explicit instantiations (13 scalar types x 13, the complex pairs) do not change. The callers (`BufferConvert.cpp`, `ConvertCommonImpl.hpp`) do not change.
- In each copy: `namespace PothosSIMDConvert { namespace POTHOS_SIMD_NAMESPACE { template <In, Out> void convert(const void *, void *, size_t); } }`. The copy without SIMD builds with `POTHOS_SIMD_NAMESPACE=fallback`.
- `std::string Pothos::Detail::simdConvertBufferArch(void);` (internal, `SIMDConvert.hpp`) - the key of the selected copy, for the tests.

- [ ] **Step 1: Tests `TestSIMDConvert.cpp`** (`/framework/tests`):
  - `test_simd_convert_matches_scalar`: all pairs of the scalar types and the complex pairs. The lengths are `{0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 33, 63, 65, 1000}`. The inputs are random values in the range where `static_cast` is defined (for `float/double -> integer`, in the range of the target type; for integers, the complete range of the input type, with min/max). The test compares with `std::memcmp` against the element-by-element `static_cast`.
  - `test_simd_convert_unaligned`: buffers with an offset of +1 byte and with "guard" bytes before and after the output. The guards are not touched, and the result is the same as for `static_cast`.
  - `test_simd_convert_dispatch`: `simdConvertBufferArch()` is equal to `"fallback"` or to a key whose parts are all in `getSupportedSIMDFeatureSet()`.
- [ ] **Step 2: Run** -> FAIL (no `simdConvertBufferArch`).
- [ ] **Step 3: Implement** with decisions 3-5 (`-O3` only for the copies of `SIMDConvert`). Remove the `simdpp` include and `SIMDPP_*` from `SIMDConvert.cpp`.
- [ ] **Step 4: Run** `/framework/tests/test_simd_convert_*`, `/framework/tests/test_buffer_convert` and the complete `ctest` -> PASS. One time locally with `-fsanitize=address,undefined` (`build-asan`) -> PASS. Delete the directory.
- [ ] **Step 5: Commit** `feat(simd): buffer conversion on xsimd with its own dispatcher`

---

### Task 3: Remove libsimdpp

**Files:**
- Modify: `.gitmodules` and the index (`git rm libsimdpp`), `lib/CMakeLists.txt` (include `libsimdpp`), `lib/System/Builtin/TestSIMDFeatures.cpp` (delete `test_simd_features_match_simdpp`), `tests/consumer/CMakeLists.txt` (`nlohmann|xsimd|asio`), `.github/workflows/ci.yml` (no `git submodule update` for the core - the core has no submodules left; remove the step or keep it empty), `README.md`, `CLAUDE.md` (the core submodules: none), `Changelog.txt` (pothos2: libsimdpp -> xsimd, `x86_xop`/`mips_msa` are not detected)

- [ ] **Step 1: RED:** `grep -rniI 'simdpp' --exclude-dir={build,.cache,docs,third_party,.git,.superpowers,flow,blocks,comms,plotters,widgets,soapy,audio,python,spuce} .` -> record the number of matches.
- [ ] **Step 2: Delete** (`git submodule deinit -f libsimdpp && git rm libsimdpp`; do not touch `.git/modules/libsimdpp`).
- [ ] **Step 3: Clean configurations** `build-clean` (GCC) and `build-clang` -> the complete `ctest` PASS. The second grep finds only the history in `Changelog.txt` and the comments. Delete the directories.
- [ ] **Step 4: Commit** `build: remove the libsimdpp submodule`

---

### Task 4: End of the phase

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 5a is done; the next one is phase 6), the ledger `docs/superpowers/ledgers/2026-10-27-phase5a-xsimd.md`

- [ ] **Step 1: Update the roadmap and the ledger. Commit** `docs: phase 5a complete — xsimd instead of libsimdpp`
- [ ] **Step 2: Do a fresh review of the complete branch** (the focus is Review Focus). Make the corrections in separate commits with tests.
- [ ] **Step 3: With the consent of the user: push the branch `phase5a-xsimd`, make a PR to `pothos2`, and check that CI passes on all four jobs.**

## Out of scope

- The SIMD code of the toolkits (`blocks`, `comms`) already uses xsimd and does not change. The move of the toolkits is phase 7.
- The cleanup of `PothosConfigSIMD.cmake` (the Intel compiler, the MIPS/PowerPC branches) is not a goal of this phase.
