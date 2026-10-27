# Этап 6b: публичный API «умеренно» — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Самописные и устаревшие конструкции публичного API (`include/Pothos/**`) заменяются стандартными C++20/23; абстракции и сигнатуры, которыми пользуются блоки и тулкиты, не ломаются; сборка ядра без предупреждений на GCC и Clang, это закрепляется в CI.

**Architecture:** Точечные правки заголовков и их реализаций, задача на группу конструкций:
1. предупреждения;
2. блокировки;
3. шаблонные утилиты и `typedef`;
4. `std::span` в буферах.

Прежние публичные имена, которые нужны тулкитам (`Util::SpinLock`, `Util::index_sequence` и т.п.), остаются как тонкие обёртки или алиасы над стандартом.

**Tech Stack:** C++23 (`std::mutex`, `std::shared_mutex`, `std::atomic_flag::wait/notify`, `std::index_sequence`, `std::unwrap_ref_decay_t`, `std::span`), CMake, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` («Глубина модернизации API: … устаревшие самописные конструкции и сигнатуры заменяются стандартными (`std::span` в буферных API, `std::optional`, стандартные примитивы синхронизации вместо самописных и т.п.), но абстракции (`Object`, `Callable`, `Proxy`, блоки, порты, топологии) не меняются»). Решение пользователя 2026-10-27: глубина «умеренно» — сигнатуры, которыми пользуются блоки, не ломаются; пространство имён `Pothos::` сохраняется.

## Global Constraints

- C++23; Linux GCC/Clang, macOS, Windows MSVC — все CI-job зелёные; тест `pothos.subproject` и `pothos.consumer` проходят.
- Не ломать исходную совместимость для блоков/тулкитов: публичные классы, методы и макросы, которые они используют, остаются с теми же именами и сигнатурами (новое — добавляется рядом). Исключение — приватные члены и то, что используется только ядром (`Util::SpinLockRW`).
- `std::optional` не вводится: в публичном API нет функций «результат через выходной параметр» (проверено при разведке), а замена ради замены — смена абстракций.
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS.
- Push в `origin` — только с согласия пользователя.

## Решения (на утверждение)

1. **Предупреждения.**
   - Из `Config.hpp` убирается `#include <ciso646>`. В C++20 и новее альтернативные токены (`and`/`not`) встроены. MSVC с `/std:c++latest` включает `/permissive-`, и они работают без этого заголовка.
   - `POTHOS_TEST_EQUAL` и родственные макросы сравнивают C-строки по содержимому через `std::string_view`, а не по адресу. Это ещё и ошибка: два одинаковых литерала в разных местах могли считаться разными.
   - Исправить точечные предупреждения в `lib/Util/Expr/Stack.hpp`, `SharedBufferUnix.cpp`, `ThreadConfigUnix.cpp`.
   - Новая опция `POTHOS_WARNINGS_AS_ERRORS` (по умолчанию OFF, в CI на GCC и Clang — ON) действует только на цели ядра: предупреждения не вернутся. На MSVC и macOS пока не включается: их список предупреждений локально не проверить, это отдельная работа.
2. **Блокировки.**
   - Внутренние блокировки портов (`InputPort`/`OutputPort`: приватные члены и `bufferManagerPush`) переходят с `Util::SpinLock` на `std::mutex`. Короткие критические секции и неконкурентный захват — у `std::mutex` быстрый путь через futex, а при конкуренции поток спит, а не крутится.
   - `Util::SpinLockRW` используют только ядро (реестры, Remote, Object). Он заменяется на `std::shared_mutex`, а публичный заголовок `SpinLockRW.hpp` удаляется.
   - `Util::SpinLock` остаётся, так как его использует тулкит `comms` (`SimpleLlc.cpp`). Но он перестаёт крутиться вхолостую: `lock()` ждёт через `std::atomic_flag::wait`, а `unlock()` будит через `notify_one`.
3. **Шаблонные утилиты.**
   - Свои `integer_sequence`/`GenSeq`/`index_sequence` в `Util/Templates.hpp` становятся алиасами `std::integer_sequence`/`std::index_sequence`/`std::make_index_sequence`. Ядро (`Object`, `Callable`) переходит на `std::` напрямую.
   - `special_decay_t<T>` становится алиасом `std::unwrap_ref_decay_t<T>`.
   - `typedef` в `include/` заменяется на `using`.
4. **`std::span` в буферах.**
   - `BufferChunk` получает `template <typename T> std::span<T> asSpan(void) const`: число элементов — `length / sizeof(T)`, остаток байт не входит.
   - `InputPort::buffer()`/`OutputPort::buffer()` не меняются: `asSpan` вызывается на возвращаемом `BufferChunk`.
   - Существующие `as<T>()` и неявное преобразование к указателю остаются.

## Review Focus

1. **Блоки и тулкиты продолжают компилироваться.** Публичные заголовки после правок компилируются вместе с исходниками, которые используют `SpinLock`, `as<T>()`, `Util::index_sequence`, `special_decay_t`, `POTHOS_TEST_*`. Тест `test_api_compat_sources` (Task 4) компилирует фрагмент `comms/mac/SimpleLlc.cpp` и типичный блок.
2. **Порты под конкуренцией.** Работают асинхронные сообщения, slot-вызовы и буферы из нескольких потоков. Тесты топологий в `pothos.selftests` и `test_spinlock_contention` (Task 2), локально под TSan (`build-tsan`).
3. **`POTHOS_TEST_EQUAL`.** Сравнивает `const char*`/массивы/`std::string`/`std::string_view` по содержимому; `nullptr` не разыменовывается; числа и прочие типы — как раньше. Тест `test_testing_string_compare` (Task 1).
4. **`asSpan`.** Длина не кратна `sizeof(T)` — остаток отбрасывается; пустой `BufferChunk` → пустой span; `const T` и `T`; комплексные типы. Тест `test_buffer_chunk_span` (Task 4).
5. **MSVC без `<ciso646>` и с `/permissive-`.** Сборка Windows CI. Если MSVC не включает альтернативные токены, вернуть `<ciso646>` только для `_MSC_VER`.

---

### Task 1: Предупреждения и `POTHOS_TEST_*` на строках

**Files:**
- Modify: `include/Pothos/Config.hpp` (`<ciso646>`), `include/Pothos/Testing.hpp` (сравнение строк), `lib/Util/Expr/Stack.hpp`, `lib/Framework/SharedBufferUnix.cpp`, `lib/Framework/ThreadConfigUnix.cpp`, `CMakeLists.txt` и `lib/CMakeLists.txt` (опция `POTHOS_WARNINGS_AS_ERRORS` → `-Werror` на целях ядра), `.github/workflows/ci.yml` (`-DPOTHOS_WARNINGS_AS_ERRORS=ON` для linux-gcc и linux-clang)
- Create: `lib/Util/Builtin/TestTesting.cpp`

**Interfaces:**
- `Testing.hpp`: внутренний помощник `Pothos::Detail::testingValue(const T &)`; для `const char *` / `char[N]` он возвращает `std::string_view`, а `nullptr` даёт `"(null)"` и не равен никакой строке. Используется в `POTHOS_TEST_EQUAL`/`NOT_EQUAL`/`GT`… вместо прямого `(lhs) == (rhs)`.

- [ ] **Step 1: Тест `TestTesting.cpp`** (`/testing/tests`): `test_testing_string_compare` — `const char a[] = "abc"; std::string b = "abc";` → `POTHOS_TEST_EQUAL(a, "abc")`, `POTHOS_TEST_EQUAL(b.c_str(), a)`, `POTHOS_TEST_EQUAL(std::string_view("abc"), a)`, `POTHOS_TEST_NOT_EQUAL(a, "abd")`; макрос с `nullptr` против `"x"` даёт NOT_EQUAL без падения; числовые сравнения не меняются (`POTHOS_TEST_EQUAL(1, 1.0)`).
- [ ] **Step 2: Run** → FAIL (`POTHOS_TEST_EQUAL(b.c_str(), a)` сравнивает адреса).
- [ ] **Step 3: Реализовать** решение 1.
- [ ] **Step 4: Run** самотест и полный `ctest`; чистая сборка Clang и GCC с `-DPOTHOS_WARNINGS_AS_ERRORS=ON` → без предупреждений и ошибок; удалить каталоги.
- [ ] **Step 5: Commit** `build: no warnings in the core (ciso646, string compare in POTHOS_TEST_*), warnings as errors in Linux CI`

---

### Task 2: Блокировки на стандартных примитивах

**Files:**
- Modify: `include/Pothos/Util/SpinLock.hpp` (`atomic_flag::wait/notify`), `include/Pothos/Framework/InputPort.hpp`, `InputPortImpl.hpp`, `OutputPort.hpp`, `OutputPortImpl.hpp` (`std::mutex`), `lib/Framework/InputPort.cpp`, `OutputPort.cpp`, `WorkerActor.cpp`, `ActorInterface.hpp`, и все пользователи `SpinLockRW` в `lib/` (`Object/Convert.cpp`, `Compare.cpp`, `ToString.cpp`, `Hash.cpp`, `Proxy/Convert.cpp`, `Managed/Registry.cpp`, `Plugin/Registry.cpp`, `Plugin/ModulePaths.cpp`, `Remote/Client.cpp`)
- Delete: `include/Pothos/Util/SpinLockRW.hpp`
- Create: `lib/Util/Builtin/TestSpinLock.cpp`

- [ ] **Step 1: Тест** `test_spinlock_contention` (`/util/tests`): 8 потоков × 100 000 инкрементов под `std::lock_guard<Util::SpinLock>` → итог 800 000; `try_lock` на захваченном → false; время ожидающего потока не тратится на вращение (CPU-время процесса за 200 мс удержания блокировки одним потоком при 4 ожидающих < 100 мс — через `std::clock`).
- [ ] **Step 2: Run** → FAIL (CPU-время: нынешний `lock()` крутится).
- [ ] **Step 3: Реализовать** решение 2.
- [ ] **Step 4: Run** полный `ctest`; локально `build-tsan` (`-fsanitize=thread`) — `/framework/tests/*` и `/util/tests/test_spinlock_contention` без отчётов; удалить каталог.
- [ ] **Step 5: Commit** `refactor: standard locks in ports and registries, SpinLock waits instead of spinning`

---

### Task 3: Шаблонные утилиты и `typedef`

**Files:**
- Modify: `include/Pothos/Util/Templates.hpp`, `include/Pothos/Object/Object.hpp`, `ObjectImpl.hpp`, `RegisterToString.hpp`, `include/Pothos/Callable/CallableImpl.hpp`, `lib/Object/Builtin/ConvertCommonImpl.hpp`, тесты, использующие `Util::` последовательности; все `typedef` в `include/`

- [ ] **Step 1: Тест** `test_templates_aliases` (`/util/tests`): `static_assert(std::is_same_v<Pothos::Util::index_sequence<0,1>, std::index_sequence<0,1>>)`, `make_index_sequence<3>` = `std::make_index_sequence<3>`, `special_decay_t<std::reference_wrapper<int>>` = `int&`, `special_decay_t<const int&>` = `int`.
- [ ] **Step 2: Run** → FAIL (свои типы не совпадают со стандартными).
- [ ] **Step 3: Реализовать** решение 3 (ядро — на `std::` напрямую; `typedef` → `using` механически).
- [ ] **Step 4: Run** полный `ctest` → PASS.
- [ ] **Step 5: Commit** `refactor: standard index sequences and decay in the public templates, using instead of typedef`

---

### Task 4: `BufferChunk::asSpan` и проверка совместимости исходников

**Files:**
- Modify: `include/Pothos/Framework/BufferChunk.hpp`
- Create: `lib/Framework/Builtin/TestBufferChunkSpan.cpp`, `tests/api_compat/` (исходники-образцы: фрагмент `comms/mac/SimpleLlc.cpp` с `Util::SpinLock`, типичный блок с `as<T>()`, `special_decay_t`, `Util::index_sequence`, `POTHOS_TEST_*`), регистрация `pothos.api_compat` (компиляция против stage-заголовков, как `pothos.consumer`)

- [ ] **Step 1: Тесты:** `test_buffer_chunk_span` — `BufferChunk(…, 10 байт)`: `asSpan<std::uint32_t>().size() == 2`, данные по тем же адресам, что `as<std::uint32_t *>()`; `asSpan<const float>()`; пустой chunk → пустой span; `std::complex<float>`. `pothos.api_compat` — образцы компилируются и линкуются с установленным Pothos.
- [ ] **Step 2: Run** → FAIL (нет `asSpan`).
- [ ] **Step 3: Реализовать** решение 4.
- [ ] **Step 4: Run** полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(api): BufferChunk::asSpan, a source compatibility check for blocks`

---

### Task 5: Итог этапа

**Files:**
- Modify: `Changelog.txt` (pothos2: `asSpan`, `SpinLockRW` удалён, `SpinLock` ждёт, `<ciso646>` не включается, `POTHOS_TEST_EQUAL` сравнивает строки по содержимому), `CLAUDE.md` (правила: стандартные примитивы, без предупреждений), `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (этап 6 выполнен; следующий — этап 7), журнал `docs/superpowers/ledgers/2026-10-27-phase6b-public-api.md`

- [ ] **Step 1: Документация, roadmap, журнал. Commit** `docs: phase 6b complete — public API on standard C++`
- [ ] **Step 2: Свежее ревью всей ветки** (фокус — Review Focus), исправления отдельными коммитами с тестами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase6b-public-api`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- Обёртки над стандартом (`Util::SpinLock`, алиасы в `Templates.hpp`) можно удалить, когда тулкиты переедут и перестанут их использовать (этап 7 и дальше).
- Предупреждения как ошибки на MSVC/macOS — отдельно, после разбора их списка.
- Перевод исключений `Pothos::Exception` на иерархию `std::` и замена `Object` на `std::any` — смена абстракций, не цель (спека: анти-цель).
