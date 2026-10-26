# SDD ledger — plan: docs/superpowers/plans/2026-10-26-phase1-build-foundation.md

Spec: docs/superpowers/specs/2026-10-26-core-modernization-design.md

## Pre-flight
- T1→T2: T2 needs working install rules; T1 makes Poco PRIVATE (removes INTERFACE include leak that broke export). OK.
- T2→T3: T3 uses PothosRunSelfTests.cmake with -DTEST_PATH; T2 Produces it. OK.
- T2→T4: consumer test shares env setup; T4 extracts cmake/PothosTestEnv.cmake from T2 script. OK (refactor in T4).
- T1→T4: T1 keeps Poco libs in PothosConfig in-tree IN_TREE_LIBRARIES; T4 rewrites installed branch only. OK.
- T4→T6: CI runs all three ctest tests. OK.
- T5: libsimdpp bump may change simdpp_multiarch API; isolated to lib/CMakeLists SIMD section. OK.
Task 1: Ruling: muparserx built as own static PIC target `pothos_muparserx` from muparserx/parser/*.cpp (third_party/CMakeLists.txt) instead of add_subdirectory(EXCLUDE_FROM_ALL) — muparserx's CMake installs its target unconditionally, which would break/pollute install — cost if wrong: none, target is removed in phase 2.
Task 1: Ruling: added .gitignore (/build/, /.superpowers/) — repo had none, build dir in worktree root — cost if wrong: trivial.
Task 1: Ruling: kept "overwrite poco's uninstall script" branch — poco defines target `uninstall` (CMP0002 clash) — cost if wrong: none, goes away with poco in phase 5.
Task 1: complete (commits 2df9e7c..044fe30, tests: sh -c 'cmake --build build && ./build/apps/PothosUtil --success-code 200; test $? -eq 200' → ninja: no work to do.)
Task 2: Ruling: env setup extracted to cmake/PothosTestEnv.cmake already in T2 (plan had it in T4) — avoids rewriting the same lines in T4 — cost if wrong: none.
Task 2: complete (commits 044fe30..ae065a0, tests: ctest --test-dir build → pothos.stage PASS; pothos.selftests 87/88, only /framework/tests/test_mmap_shared_buffer FAIL — exactly plan Expected; task-done script not used because the expected state is red until T3)
Task 3: Ruling: fixed the test, not library code — root cause: std::ofstream not flushed before size check (libstdc++ filebuf buffers 8192 B > 4096 B written), file size 0; mmap write-back assertions later in the same test pass, proving MemoryMappedBufferContainer correct — cost if wrong: a real mmap bug stays hidden (unlikely: test exercises read+write-back).
Task 3: complete (commits ae065a0..09ba872, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  10.13 sec)
Task 4: Ruling: installed PothosTargets-<config>.cmake still lists Poco targets in IMPORTED_LINK_DEPENDENT_LIBRARIES (CMake-generated for PRIVATE shared deps; linker rpath hint, not interface) — consumer test passes with it; disappears when Poco is removed in phase 5 — cost if wrong: a consumer linker warning on some platforms until phase 5.
Task 4: Ruling: lib/Pothos.in.pc needed no change (no Poco in it; muparserx -l flag already dropped in T1) — cost if wrong: none.
Task 4: complete (commits 09ba872..81524eb, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.19 sec)
Task 5: Ruling: libsimdpp updated to upstream master 55860b50 (describe: v2.0-503, plan said v2.0-rc1-574 — same commit, different tag naming) ; simdpp_multiarch API unchanged, no CMake adaptation needed; FreeBSD -stdlib branch left as is — cost if wrong: none.
Task 5: complete (commits 81524eb..cca452e, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  11.29 sec)
Task 6: Ruling: README "Dependencies" section rewritten too (was describing optional external poco/muparserx/spuce installs, now false) — cost if wrong: none.
Task 6: Ruling: push to origin + `gh run watch` (Step 5) deferred to user approval — outward-facing action — CI not yet run; macOS/Windows never built.
Task 6: Review Focus #5 local check (Release, -DCMAKE_INSTALL_LIBDIR=lib64) FAILED: Poco 1.8.1 installs to lib${LIB_SUFFIX}, ignoring CMAKE_INSTALL_LIBDIR -> libPocoUtil.so.60 not found. Fixed: derive LIB_SUFFIX from CMAKE_INSTALL_LIBDIR inside the poco block (0eac66d) — lib64 build 3/3 PASS, default build 3/3 PASS.
Task 6: complete (commits cca452e..0eac66d, tests: ctest --test-dir build → 3/3 PASS, selftests 88/88; lib64 Release build 3/3 PASS; CI pending push)
Final review: opus reviewer, verdict "With fixes" (2 Critical, 4 Important, 13 Minor).
Final: Critical 2 (lib64 Poco libdir) already fixed in 0eac66d — lib64 Release 3/3 before review fixes, 4/4 after.
Final: fixed Critical 1 muparserx std::binary_function on libc++/MSVC (e37d5f6) — NOT locally verifiable (no libc++/MSVC here); verification = first CI run on macos/windows.
Final: fixed Important 4 pothos_test_env deleted TEST_HOME_DIR recursively — tests/test_env_guard.cmake RED (fake home deleted) → GREEN, suite 4/4 (d055dde).
Final: fixed Important 2 no install rpath — selftests+consumer RED without LD_LIBRARY_PATH ("libPothosd.so... cannot open") → GREEN with $ORIGIN/@loader_path relative rpath, suite 4/4 (c7fbbfb).
Final: fixed Important 1 Poco dev files installed + weak consumer — consumer RED ("include directory has more than Pothos/: Poco;Pothos") → GREEN: poco EXCLUDE_FROM_ALL + install only runtime libs (NAMELINK_SKIP), consumer compiles every public header; suite 4/4, lib64 4/4 (bf79a88).
Final: fixed Important 3 consumer exe path with multi-config generators — Ninja Multi-Config RED ("consumer run failed: no such file") → GREEN via RUNTIME_OUTPUT_DIRECTORY bin$<0:> (bf79a88).
Final: Ruling: declined-to-judge items (IMPORTED_LINK_DEPENDENT_LIBRARIES, Poco::Exception macro, PothosConfig hard-coded bin, absolute CMAKE_INSTALL_*DIR, simdpp arm64 arch detection, Poco throw() specs, version git suffix, MSVC JIT test) — all transitional/pre-existing or environment-dependent; stand as-is for phase 1 — cost if wrong: surfaced by CI or later phases.
Final: minor (deferred): developer POTHOS_PLUGIN_PATH/POTHOS_CONF_PATH/POTHOS_LOG_* leak into self-tests
Final: minor (deferred): pothos.stage does not clean build/stage (stale files can be loaded)
Final: minor (deferred): poco forces CMAKE_BUILD_TYPE=RelWithDebInfo before PothosConfig default Release applies
Final: minor (deferred): Pothos.pc says -lPothos but Debug installs libPothosd (poco forces CMAKE_DEBUG_POSTFIX=d)
Final: minor (deferred): dead vars Pothos_LINKER_FLAGS/COMPILE_OPTIONS/COMPILE_DEFINITIONS in lib/CMakeLists.txt
Final: minor (deferred): <ciso646> in include/Pothos/Config.hpp warns under C++20+ (MSVC STL4036)
Final: minor (deferred): CMAKE_OSX_DEPLOYMENT_TARGET=13.3 only in CI, not defaulted in CMakeLists
Final: minor (deferred): no-network grep does not cover poco/** and libsimdpp/cmake
Final: minor (deferred): CI lacks permissions: contents: read, concurrency cancel; push+PR double runs
Final: minor (deferred): consumer leak check ignores INTERFACE_COMPILE_OPTIONS/LINK_OPTIONS
Final: minor (deferred): plan line says Poco builds as C++23 — actually poco/cmake/CXX1x.cmake forces C++14
Final: minor (deferred): PocoEncodings built/installed though unused — now moot (EXCLUDE_FROM_ALL, not linked, not built)
CI run 40336459273 (9fae150): linux-clang PASS; linux-gcc FAIL /object/tests/test_compare_to (exact compare() value -15 vs -1; API only promises the sign); windows FAIL build: shared_ptr::unique removed in C++20 (SharedBuffer.hpp:193); macos CANCELLED — GitHub never acquired a macOS arm64 runner (capacity), not our code.
CI: Ruling: fixed test_compare_to to compare signs (test bug, Object.hpp:247 documents negative/zero/positive) — RED = CI linux-gcc (not reproducible with local GCC 16) — cost if wrong: none.
CI: Ruling: shared_ptr::unique() -> use_count()==1 at SharedBuffer.hpp, ThreadEnvironment.cpp, Module.cpp; throw() -> noexcept in Exception.hpp/.cpp (removed in C++20) — RED = CI windows (local libstdc++ keeps both without warnings) — cost if wrong: none (same semantics).
CI: pushed 64f59c6, f1eed95 to origin/pothos2 (fast-forward); rerun 40339728576.
