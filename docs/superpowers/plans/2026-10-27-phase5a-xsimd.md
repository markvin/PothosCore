# Этап 5a: libsimdpp → xsimd — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ядро больше не использует libsimdpp: определение возможностей CPU и SIMD-конвертация буферов построены на xsimd (копия в `third_party/xsimd`), сборка под несколько архитектур идёт через собственный `PothosConfigSIMD.cmake`, сабмодуль `libsimdpp` удалён. Тулкиты не меняются.

**Architecture:**
- `lib/System/SIMD.cpp` определяет возможности CPU через `xsimd::cpu_features`. Отдаёт он прежние имена (`x86_sse2`, …, `neon`), потому что они составляют ключи диспетчеров тулкитов.
- `lib/SIMD/SIMDConvert.cpp` собирается в нескольких копиях через `pothos_multiarch` из `PothosConfigSIMD.cmake`, как SIMD-файлы тулкитов. Каждая копия помещает реализацию в своё пространство имён `POTHOS_SIMD_NAMESPACE`.
  - Если типы входа и выхода имеют одинаковое число элементов в регистре и поддерживаются xsimd, преобразование идёт через `xsimd::batch_cast`.
  - Иначе — цикл `static_cast`, который компилятор векторизует под флаги этой копии.
- Собственный диспетчер (`lib/SIMD/SIMDConvertDispatch.cpp`) один раз выбирает лучшую копию через `getOptimalSIMDFeatureSetKey`. Список копий генерирует CMake.

**Tech Stack:** C++23, xsimd 14.3.0 (только заголовки, BSD-3-Clause), CMake/CTest, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (вендоринг зависимостей в дерево, приватность, анти-цели). Roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (этап 5a — решение пользователя: xsimd копией в `third_party/xsimd`).

## Global Constraints

- C++23; Linux GCC/Clang, macOS, Windows MSVC — все четыре CI-job зелёные.
- xsimd не виден в `include/Pothos/**` и не требуется потребителю: тест `pothos.consumer` получает в регулярку `xsimd` вместо `simdpp`. Публичный `XSIMDTraits.hpp` остаётся как есть: он не включает xsimd.
- Сборка ничего не скачивает (xsimd в дереве).
- Публичный API `Pothos::System::getSupportedSIMDFeatureSet()` / `getOptimalSIMDFeatureSetKey()` и формат ключей (`x86_sse2__x86_avx2`, `fallback`) не меняются. Тулкиты (`blocks`, `comms`) собираются без изменений.
- Результат `simdConvertBuffer<In, Out>` побитово совпадает с поэлементным `static_cast<Out>(in[i])` для всех поддерживаемых пар типов и любых длин буфера.
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS.
- Push в `origin` — только с согласия пользователя.

## Решения (на утверждение)

1. **Имена архитектур сохраняются:** `x86_sse2`, `x86_sse3`, `x86_ssse3`, `x86_sse4_1`, `x86_popcnt`, `x86_avx`, `x86_avx2`, `x86_fma3`, `x86_fma4`, `x86_avx512f`, `x86_avx512bw`, `x86_avx512dq`, `x86_avx512vl`, `neon`, `neonfltsp`, `power_altivec`, `power_vsx_206`, `power_vsx_207`. Порядок «от медленных к быстрым» тоже прежний.
   - **`x86_xop` больше не определяется**: в xsimd его нет, это AMD Bulldozer 2011–2015. Диспетчеры тулкитов на таких CPU выберут соседнюю архитектуру, ошибок не будет.
   - **`mips_msa` больше не определяется**: xsimd не поддерживает MIPS.
2. **xsimd 14.3.0** — только `include/` копией в `third_party/xsimd`, приватная INTERFACE-цель `pothos_xsimd`, не устанавливается. Тулкиты по-прежнему ищут xsimd сами через `find_package(xsimd)`.
3. **`SIMDConvert` на xsimd.**
   - `xsimd::batch_cast` — когда `batch<In>::size == batch<Out>::size` и xsimd поддерживает обе пары типов (например, `int32↔float`, `int64↔double`, `int8↔uint8`).
   - Иначе — цикл `static_cast`, который компилятор векторизует (для этой копии файла включаем `-O3`/`/O2`).
   - Как и сейчас, `float/double → целое` всегда идёт через скалярный `static_cast`: так проще гарантировать совпадение с `static_cast`.
4. **Многоархитектурная сборка ядра — функции `PothosConfigSIMD.cmake`** (`pothos_get_compilable_archs` + `pothos_multiarch`), как у тулкитов, вместо `simdpp_get_runnable_archs`.
   - Это меняет поведение: копии собираются для всех архитектур, которые умеет компилятор, а не только для тех, что исполнимы на машине сборки. Диспетчер всё равно выбирает во время выполнения.
   - Цена — больше копий одного файла при сборке.
5. **Свой диспетчер вместо `SIMDPP_MAKE_DISPATCHER`.** CMake генерирует `SIMDConvertArches.hpp` со списком пространств имён копий. `SIMDConvertDispatch.cpp` один раз (static) выбирает таблицу указателей на функции лучшей копии; запасной вариант — копия `fallback` без SIMD-флагов.

## Review Focus

1. **Совпадение со `static_cast` на всех парах типов и длинах.**
   - Типы: `char`, `int8…int64`, `long`, `long long`, их беззнаковые варианты, `float`, `double`; комплексные пары — по частям.
   - Длины: 0, 1, длина регистра ± 1, нечётные, 1000.
   - Значения на границах диапазонов: целочисленное сужение даёт перенос по модулю, как `static_cast` в C++20.

   Тест `test_simd_convert_matches_scalar` (Task 2).
2. **Невыровненные указатели и пересечение границы буфера.** Вход и выход по смещению +1 байт, хвост меньше регистра — без выхода за границы. Тест `test_simd_convert_unaligned` (Task 2; прогнать локально под ASan).
3. **Определение CPU совпадает с libsimdpp** на машине сборки, кроме `x86_xop` и `mips_msa`. Тест `test_simd_features_match_simdpp` (Task 1) — существует только пока libsimdpp в дереве, удаляется в Task 3.
4. **Выбор копии:** `getOptimalSIMDFeatureSetKey` для списка с неподдерживаемой архитектурой выбирает лучшую поддерживаемую, для пустого списка — `fallback`. Диспетчер `SIMDConvert` сообщает выбранный ключ, и он входит в множество поддерживаемых. Тест `test_simd_optimal_key` (Task 1), `test_simd_convert_dispatch` (Task 2).
5. **Сборка на всех компиляторах:** флаги архитектур MSVC (`/arch:AVX2`, `/arch:AVX512`), Apple clang на arm64 (`neon`), отсутствие архитектур (`fallback`) — CI на четырёх job-ах; Task 3 — чистые сборки GCC и Clang локально.

---

### Task 1: xsimd в дереве и определение CPU

**Files:**
- Create: `third_party/xsimd/` (`include/`, `LICENSE`, `README.md` с версией 14.3.0 и источником), `lib/System/Builtin/TestSIMDFeatures.cpp`
- Modify: `third_party/CMakeLists.txt` (цель `pothos_xsimd`), `lib/CMakeLists.txt` (линковка `pothos_xsimd` PRIVATE), `lib/System/SIMD.cpp`, `README.md`, `CLAUDE.md` (вендоренный код)

**Interfaces:**
- `pothos_xsimd`: INTERFACE, `SYSTEM` include `third_party/xsimd/include`.
- `std::vector<std::string> Pothos::System::Detail::detectSIMDFeatures(void);` (внутренний `lib/System/SIMDFeatures.hpp`) — определение через `xsimd::cpu_features` по решению 1, порядок от медленных к быстрым; `getSupportedSIMDFeatureSet()` кеширует её результат; `getOptimalSIMDFeatureSetKey()` без изменений.

- [ ] **Step 1: Скопировать xsimd** (тег `14.3.0`, `include/` + лицензия) и добавить цель `pothos_xsimd`.
- [ ] **Step 2: Тесты `TestSIMDFeatures.cpp`** (`/system/tests`):
  - `test_simd_feature_names`: каждое имя из `getSupportedSIMDFeatureSet()` входит в список решения 1; на x86-64 есть `x86_sse2`, на aarch64 — `neon`; список отсортирован от медленных к быстрым (индекс в списке решения 1 растёт).
  - `test_simd_optimal_key`: `getOptimalSIMDFeatureSetKey({})` → `"fallback"`; `{"x86_sse2", "<все поддерживаемые через __>", "no_such_arch"}` → ключ из всех поддерживаемых; ключ только с неподдерживаемой архитектурой → `"fallback"`.
  - `test_simd_features_match_simdpp` (временно, пока libsimdpp в дереве): `detectSIMDFeatures()` = множество, которое даёт `simdpp::get_arch_*` через старую таблицу (перенесённую в тест), без `x86_xop`/`mips_msa`.
- [ ] **Step 3: Run** → FAIL (нет `lib/System/SIMDFeatures.hpp` / `detectSIMDFeatures`).
- [ ] **Step 4: Реализовать** на xsimd; старая таблица libsimdpp остаётся только в тесте.
- [ ] **Step 5: Run** самотесты `/system/tests/test_simd_*` и полный `ctest` → PASS.
- [ ] **Step 6: Commit** `feat(simd): vendored xsimd, CPU feature detection on xsimd`

---

### Task 2: `SIMDConvert` на xsimd и свой диспетчер

**Files:**
- Modify: `lib/SIMD/SIMDConvert.cpp` (реализация на xsimd в `namespace POTHOS_SIMD_NAMESPACE`), `lib/SIMD/SIMDConvert.hpp`, `lib/CMakeLists.txt` (блок «Generate dynamic dispatch files»: `include(PothosConfigSIMD)` из `cmake/Modules`, `pothos_get_compilable_archs` + `pothos_multiarch`, генерация `SIMDConvertArches.hpp`), `lib/Framework/Builtin/TestBufferConvert.cpp`
- Create: `lib/SIMD/SIMDConvertDispatch.cpp`, `lib/SIMD/SIMDConvertArches.hpp.in`, `lib/SIMD/Builtin/TestSIMDConvert.cpp`

**Interfaces:**
- `template <typename In, typename Out> void simdConvertBuffer(const void *in, void *out, size_t len);` — сигнатура и явные инстанциации (13 скалярных типов × 13, комплексные пары) не меняются; вызывающие (`BufferConvert.cpp`, `ConvertCommonImpl.hpp`) не трогаются.
- В каждой копии: `namespace PothosSIMDConvert { namespace POTHOS_SIMD_NAMESPACE { template <In, Out> void convert(const void *, void *, size_t); } }`; копия без SIMD собирается с `POTHOS_SIMD_NAMESPACE=fallback`.
- `std::string Pothos::Detail::simdConvertBufferArch(void);` (внутренняя, `SIMDConvert.hpp`) — ключ выбранной копии, для тестов.

- [ ] **Step 1: Тесты `TestSIMDConvert.cpp`** (`/framework/tests`):
  - `test_simd_convert_matches_scalar`: все пары скалярных типов и комплексные пары; длины `{0, 1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 33, 63, 65, 1000}`; входы — случайные значения в диапазоне, где `static_cast` определён (для `float/double → целое` — в диапазоне целевого типа; для целых — весь диапазон входного типа, включая min/max); сравнение `std::memcmp` с поэлементным `static_cast`.
  - `test_simd_convert_unaligned`: буферы со смещением +1 байт и «стражевыми» байтами до и после выхода — стражи не тронуты, результат как у `static_cast`.
  - `test_simd_convert_dispatch`: `simdConvertBufferArch()` равен `"fallback"` или ключу, все части которого входят в `getSupportedSIMDFeatureSet()`.
- [ ] **Step 2: Run** → FAIL (нет `simdConvertBufferArch`).
- [ ] **Step 3: Реализовать** по решениям 3–5 (`-O3` только для копий `SIMDConvert`); удалить include `simdpp` и `SIMDPP_*` из `SIMDConvert.cpp`.
- [ ] **Step 4: Run** `/framework/tests/test_simd_convert_*`, `/framework/tests/test_buffer_convert`, полный `ctest` → PASS; локально один раз с `-fsanitize=address,undefined` (`build-asan`) → PASS, каталог удалить.
- [ ] **Step 5: Commit** `feat(simd): buffer conversion on xsimd with its own dispatcher`

---

### Task 3: Удаление libsimdpp

**Files:**
- Modify: `.gitmodules` и индекс (`git rm libsimdpp`), `lib/CMakeLists.txt` (include `libsimdpp`), `lib/System/Builtin/TestSIMDFeatures.cpp` (удалить `test_simd_features_match_simdpp`), `tests/consumer/CMakeLists.txt` (`nlohmann|xsimd|asio`), `.github/workflows/ci.yml` (без `git submodule update` для ядра — сабмодулей ядра не остаётся; убрать шаг или оставить пустым), `README.md`, `CLAUDE.md` (сабмодули ядра: нет), `Changelog.txt` (pothos2: libsimdpp → xsimd, `x86_xop`/`mips_msa` не определяются)

- [ ] **Step 1: RED:** `grep -rniI 'simdpp' --exclude-dir={build,.cache,docs,third_party,.git,.superpowers,flow,blocks,comms,plotters,widgets,soapy,audio,python,spuce} .` → найденное (зафиксировать число).
- [ ] **Step 2: Удалить** (`git submodule deinit -f libsimdpp && git rm libsimdpp`; `.git/modules/libsimdpp` не трогать).
- [ ] **Step 3: Чистые конфигурации** `build-clean` (GCC) и `build-clang` → полный `ctest` PASS; повторный grep → только история в `Changelog.txt` и комментарии; удалить каталоги.
- [ ] **Step 4: Commit** `build: remove the libsimdpp submodule`

---

### Task 4: Итог этапа

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (этап 5a выполнен; следующий — этап 6), журнал `docs/superpowers/ledgers/2026-10-27-phase5a-xsimd.md`

- [ ] **Step 1: Обновить roadmap и журнал. Commit** `docs: phase 5a complete — xsimd instead of libsimdpp`
- [ ] **Step 2: Свежее ревью всей ветки** (фокус — Review Focus), исправления отдельными коммитами с тестами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase5a-xsimd`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- SIMD-код тулкитов (`blocks`, `comms`) уже на xsimd и не меняется; переезд тулкитов — этап 7.
- Чистка `PothosConfigSIMD.cmake` (Intel-компилятор, MIPS/PowerPC-ветки) — не цель этапа.
