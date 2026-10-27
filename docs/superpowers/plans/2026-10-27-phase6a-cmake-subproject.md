# Этап 6a: CMake-гигиена (ядро как подпроект) — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ядро можно подключить к другому проекту через `add_subdirectory`/`FetchContent`. Оно не меняет родителю опции, кеш, глобальные настройки, тесты и цели, а самостоятельная сборка ядра работает как прежде.

**Architecture:** Корневой `CMakeLists.txt` разделяет поведение по `PROJECT_IS_TOP_LEVEL`. Опции получают префикс `POTHOS_`, лишние опции-«компоненты» удаляются. Глобальные `CMAKE_*` заменяются свойствами целей, а `CMAKE_BINARY_DIR` — на `PROJECT_BINARY_DIR`. Новый тест `pothos.subproject` конфигурирует маленький родительский проект с `add_subdirectory(<ядро>)` и проверяет, что ядро ничего ему не испортило.

**Tech Stack:** CMake ≥ 3.25 (`PROJECT_IS_TOP_LEVEL` есть с 3.21), CTest.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` («Структура CMake … на усмотрение исполнителя»; анти-цель: потребителю не должны требоваться внутренние зависимости). Roadmap: этап 6, пункт «CMake-гигиена» (решение пользователя 2026-10-27; этап 6 делится на 6a и 6b, пространство имён `Pothos::` сохраняется).

## Global Constraints

- Самостоятельная сборка не меняется:
  - те же цели;
  - та же установка (раскладка, `PothosConfig.cmake`, `Pothos.pc`);
  - те же тесты CTest;
  - `compile_commands.json` создаётся;
  - rpath установленных `libPothos` и `PothosUtil` прежний.
- Linux GCC/Clang, macOS, Windows MSVC — все четыре CI-job зелёные.
- Совместимость имён CMake-опций не требуется (спека: «Структура CMake … на усмотрение исполнителя»); переименования фиксируются в `Changelog.txt` и README.
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS.
- Push в `origin` — только с согласия пользователя.

## Решения (на утверждение)

1. **Опции.**
   - Удаляются `ENABLE_LIBRARY`, `ENABLE_LIBRARY_LIB`, `ENABLE_LIBRARY_INCLUDE`, `ENABLE_LIBRARY_CMAKE`: библиотека и есть проект, выключать её бессмысленно.
   - Новые опции:
     - `POTHOS_BUILD_APPS` (PothosUtil; по умолчанию ON — нужна самотестам и безопасной загрузке модулей);
     - `POTHOS_BUILD_DOCS` (по умолчанию ON, если найден Doxygen и проект верхнего уровня);
     - `POTHOS_BUILD_TESTS` (регистрация тестов CTest; по умолчанию `${PROJECT_IS_TOP_LEVEL}`).
   - Уже есть с префиксом: `POTHOS_ENABLE_LIBNUMA`, `POTHOS_SELFTEST_TIMEOUT`, `POTHOS_ROOT`, `POTHOS_ROOT_ENV`. `APPDATA_ENV` переименовывается в `POTHOS_APPDATA_ENV`.
2. **Только на верхнем уровне:**
   - `enable_testing()` по умолчанию (через `POTHOS_BUILD_TESTS`);
   - `feature_summary`;
   - `CMAKE_EXPORT_COMPILE_COMMANDS`;
   - выбор `Release`, если `CMAKE_BUILD_TYPE` пуст;
   - цель `uninstall`. В подпроекте вместо неё появляется `pothos_uninstall`.
3. **Глобальные настройки → свойства целей.**
   - `CMAKE_CXX_STANDARD`/`_REQUIRED`/`_EXTENSIONS` удаляются. Стандарт уже приходит через `target_compile_features(... cxx_std_23)`; `CXX_EXTENSIONS OFF` ставится на цели ядра.
   - `option(CMAKE_SKIP_BUILD_RPATH…)`, `option(CMAKE_BUILD_WITH_INSTALL_RPATH…)`, `option(CMAKE_INSTALL_RPATH_USE_LINK_PATH…)`, `CMAKE_INSTALL_RPATH`, `CMAKE_MACOSX_RPATH` удаляются. `INSTALL_RPATH` (`$ORIGIN/../lib…`, `@loader_path/../lib…`) ставится на `Pothos` и `PothosUtil`.
3a. **`PothosConfig.cmake` при сборке ядра в дереве** (`POTHOS_IN_TREE_SOURCE_DIR`) не трогает `CMAKE_BUILD_TYPE`. Для внешних потребителей (`find_package`) поведение прежнее.
4. **`CMAKE_BINARY_DIR` → `PROJECT_BINARY_DIR`** во всех путях ядра: stage, test-home, consumer, test-env-guard.

## Review Focus

1. **Самостоятельная сборка не изменилась:**
   - список установленных файлов до и после совпадает (diff `install_manifest.txt` старой и новой сборки);
   - тесты CTest те же;
   - rpath у `PothosUtil` и `libPothos` прежний (`readelf -d`/`otool -l`).

   Шаг проверки в Task 2.
2. **Родитель с одноимёнными опциями и целями.** Родитель со своими `option(ENABLE_LIBRARY OFF)`, `option(BUILD_DOCS OFF)` и целью `uninstall`, а также с пустым `CMAKE_BUILD_TYPE`:
   - конфигурируется;
   - его значения не меняются;
   - `Pothos::Pothos` доступна.

   Тест `pothos.subproject` (Task 1).
3. **Тесты ядра не попадают в `ctest` родителя без `POTHOS_BUILD_TESTS=ON`.** С `ON` stage-каталог лежит в бинарном каталоге ядра, а не родителя. Тот же тест (Task 1).
4. **Глобальные переменные родителя не меняются:** `CMAKE_CXX_STANDARD`, `CMAKE_INSTALL_RPATH`, `CMAKE_EXPORT_COMPILE_COMMANDS`, `CMAKE_BUILD_TYPE` до и после `add_subdirectory` совпадают. Тот же тест (Task 1).
5. **Потребитель через `find_package`** (`pothos.consumer`) и самотесты из stage работают на всех платформах — CI.

---

### Task 1: Тест «ядро как подпроект» и исправления CMake

**Files:**
- Create: `tests/subproject/CMakeLists.txt` (родительский проект), `cmake/PothosSubprojectTest.cmake` (cmake -P: конфигурирует родителя дважды — без и с `-DPOTHOS_BUILD_TESTS=ON`; проверяет выводом `message` и `ctest -N`)
- Modify: `CMakeLists.txt`, `lib/CMakeLists.txt`, `apps/CMakeLists.txt`, `include/CMakeLists.txt`, `cmake/Modules/CMakeLists.txt`, `docs/CMakeLists.txt`, `cmake/Modules/PothosConfig.cmake`

**Interfaces:**
- `tests/subproject/CMakeLists.txt`: `project(Parent)`.
  - До подключения ядра: `option(ENABLE_LIBRARY "" OFF)`, `option(BUILD_DOCS "" OFF)`, `add_custom_target(uninstall)`. Также запоминает `CMAKE_CXX_STANDARD`, `CMAKE_INSTALL_RPATH`, `CMAKE_EXPORT_COMPILE_COMMANDS`, `CMAKE_BUILD_TYPE`.
  - Затем `add_subdirectory(${POTHOS_SOURCE} pothos)`.
  - После него печатает `PARENT_CHECK name=value` для каждого из этих значений, а также `HAS_TARGET=$<TARGET_EXISTS:Pothos::Pothos>` (через `if(TARGET)`).
- `pothos.subproject` (тест CTest, только на верхнем уровне при `POTHOS_BUILD_TESTS`), конфигурации в `${PROJECT_BINARY_DIR}/subproject[-tests]`.
  - Ожидается: значения родителя не изменились, `Pothos::Pothos` существует.
  - Без `POTHOS_BUILD_TESTS` `ctest -N` в каталоге родителя показывает 0 тестов.
  - С ним — тесты `pothos.*`, а stage-путь в их командах лежит под `pothos/`.

- [ ] **Step 1: Написать `tests/subproject/CMakeLists.txt`, `cmake/PothosSubprojectTest.cmake` и регистрацию `pothos.subproject`.**
- [ ] **Step 2: Run** `ctest -R pothos.subproject` → FAIL (опция `ENABLE_LIBRARY=OFF` родителя выключает ядро / `uninstall` конфликтует / тесты ядра в родителе).
- [ ] **Step 3: Реализовать решения 1–4.**
- [ ] **Step 4: Run** полный `ctest` → PASS (тестов 5).
- [ ] **Step 5: Commit** `build: the core works as a CMake subproject (POTHOS_ options, top-level only globals)`

---

### Task 2: Самостоятельная сборка не изменилась; документация

**Files:**
- Modify: `README.md` (опции сборки, подключение подпроектом), `CLAUDE.md` (опции, правило «никаких глобальных `CMAKE_*`/общих имён»), `Changelog.txt` (pothos2: переименованные/удалённые опции)

- [ ] **Step 1: Сравнить установку:** чистая сборка `pothos2` (без этой ветки) и этой ветки в `/tmp`-подобные каталоги в scratchpad → `install_manifest.txt` совпадают; `readelf -d build-*/stage/bin/PothosUtil | grep -E 'RUNPATH|RPATH'` и для `libPothos.so` — одинаковые; `ctest -N` — одинаковый список, кроме нового `pothos.subproject`. Расхождения — исправить или записать решение в журнал.
- [ ] **Step 2: Обновить документацию.**
- [ ] **Step 3: Чистые сборки GCC и Clang** → полный `ctest` PASS; удалить каталоги.
- [ ] **Step 4: Commit** `docs: CMake options and use as a subproject`

---

### Task 3: Итог этапа

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (6a выполнен; следующий — 6b), журнал `docs/superpowers/ledgers/2026-10-27-phase6a-cmake-subproject.md`

- [ ] **Step 1: Обновить roadmap и журнал. Commit** `docs: phase 6a complete — the core as a CMake subproject`
- [ ] **Step 2: Свежее ревью всей ветки** (фокус — Review Focus), исправления отдельными коммитами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase6-api-cmake`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- Публичный API (этап 6b): `SpinLock`/`SpinLockRW` → стандартные примитивы, `Templates.hpp` → `std::index_sequence`, `throw()` → `noexcept`, `typedef` → `using`, `std::span` рядом с существующими буферными методами; сигнатуры, которыми пользуются блоки, не ломаются (решение пользователя: «умеренно»).
- `PothosConfig.cmake` для внешних потребителей (`find_package`) по-прежнему выставляет `CMAKE_BUILD_TYPE=Release` по умолчанию — это поведение тулкитов, этап 7.
