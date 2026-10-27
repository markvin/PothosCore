# Этап 7: чистка репозитория — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** в PothosCore остаётся только ядро (`lib`, `include`, `apps`, `cmake`, `tests`, `third_party`, `docs`); все сабмодули тулкитов удалены, ядро самодостаточно и подключается соседними проектами через `find_package(Pothos)` как после установки, так и подпроектом.

**Architecture:** тулкиты уже живут в своих репозиториях pothosware (PothosBlocks, PothosFlow, …) — этап только убирает их сабмодули из ядра; перенос тулкитов на API pothos2 — отдельные этапы в их репозиториях. Две runtime-зависимости ядра от blocks (`/blocks/copier`, `/blocks/network_sink|source`) остаются плагинными путями, но дают понятную ошибку и документируются. Для сборки «ядро + тулкиты» подпроектами ядро пишет redirect-конфиг в `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`.

**Tech Stack:** git, CMake ≥ 3.25 (`CMAKE_FIND_PACKAGE_REDIRECTS_DIR`, 3.24+), C++23, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`; roadmap `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (строка этапа 7).

## Решения пользователя (2026-10-27)

1. Тулкиты: **только удалить** сабмодули (spuce, plotters, widgets, flow, soapy, blocks, python, audio, comms) и `sync_toolkits`. Форки и перенос тулкитов — не в этом этапе.
2. `/blocks/copier` и `/blocks/network_*`: **задокументировать** зависимость от тулкита blocks и дать понятное сообщение об ошибке, в ядро не переносить.
3. `debian/`: **удалить** (пакеты 0.7.1, `pothos-all` зависит от тулкитов; пакетирование pothos2 — позже).

## Global Constraints

- Не трогать общий `.git/modules` основного клона: сабмодули удаляются только из дерева и индекса ветки (`git rm`), каталоги `.git/modules/*` не чистятся.
- CMake ≥ 3.25, C++23; ничего не скачивается во время сборки.
- Публичный API не меняется (кроме текста исключений).
- CI: те же 4 job-а (linux-gcc, linux-clang с `POTHOS_WARNINGS_AS_ERRORS`, macos, windows).

## Review Focus

1. **Свежий клон без сабмодулей.** `git clone` + сборка + `ctest` проходят; нигде (CMake, CI, скрипты, документация) нет ссылок на удалённые каталоги. Проверка — Task 1, шаг grep и чистая сборка из `git archive`.
2. **Топология с пересечением доменов без blocks.** Ошибка — `Pothos::TopologyConnectError`, в тексте путь `/blocks/copier` и слово `PothosBlocks`; не `PluginRegistryError` без контекста. Тест `test_domain_crossing_needs_blocks` (Task 2).
3. **Сеть без blocks на удалённом хосте.** Та же форма ошибки для `/blocks/network_sink|source`. Код — Task 2 (один помощник на оба пути); отдельного теста нет (нужен второй хост), покрывается общим помощником.
4. **`find_package(Pothos)` после `add_subdirectory` ядра и в соседнем подкаталоге.** Находит in-tree конфигурацию, `Pothos::Pothos` и `POTHOS_UTIL_EXE` заданы, повторный `find_package` безвреден; в standalone-сборке redirect не пишется. Тест — расширение `pothos.subproject` (Task 3).
5. **Документация.** README без раздела «Toolkits as submodules», с указанием репозиториев тулкитов и runtime-зависимости от blocks; лицензии сторонних кодов (kissfft, Qwt, qtcolorpicker) больше не перечисляются, раз их кода в дереве нет. Task 4.

---

### Task 1: Удалить сабмодули, `sync_toolkits`, `debian/`

**Files:**
- Delete: `.gitmodules`, сабмодули `spuce plotters widgets flow soapy blocks python audio comms`, `sync_toolkits`, `debian/`
- Modify: всё, что найдёт grep ниже (ожидается: ничего в CMake/CI)

- [ ] **Step 1:** `git rm -r --cached`/`git rm` для 9 сабмодулей, `.gitmodules`, `sync_toolkits`, `debian`; каталоги сабмодулей в рабочем дереве удалить (`rm -rf <dir>` — только рабочие копии в этом worktree; `.git/modules` не трогать).
- [ ] **Step 2: Проверка ссылок:** `git ls-files | xargs grep -nE '(^|[^/])\b(spuce|plotters|widgets|flow|soapy|python|audio|comms)/|sync_toolkits|debian/'` (исключая `docs/superpowers/`, `Changelog.txt`) → Expected: пусто, кроме README (правится в Task 4).
- [ ] **Step 3: Чистая сборка из архива:** `git archive HEAD` в scratch-каталог, configure + build + `ctest` → Expected: 6/6.
- [ ] **Step 4: Commit** `chore: remove the toolkit submodules, sync_toolkits and the 0.7 debian packaging`

### Task 2: Понятная ошибка, когда для топологии нужен тулкит blocks

**Files:**
- Modify: `lib/Framework/TopologyDomainFlows.cpp:85`, `lib/Framework/TopologyNetworkFlows.cpp:40-50`, `include/Pothos/Framework/Topology.hpp` (doc-комментарий `commit()`)
- Create: `lib/Framework/TopologyToolkitBlocks.hpp` (внутренний), `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`

**Interfaces:**
- Produces: `Pothos::Proxy makeToolkitBlock(const Pothos::Proxy &registry, const std::string &path, ArgsType&&... args)` в `TopologyToolkitBlocks.hpp` (внутренний, шаблон): вызывает `registry.call(path, args...)`; если блока нет (`registry.call("doesBlockExist"...)` или перехват ошибки реестра), бросает `Pothos::TopologyConnectError("Pothos::Topology::commit()", "<path> is needed ... install the blocks toolkit (PothosBlocks)")`.

- [ ] **Step 1: Тест** `test_domain_crossing_needs_blocks` (`/framework/tests`): два блока теста — источник с выходом в домене `"test_domain_a"`, приёмник со входом в `"test_domain_b"`, оба возвращают `"ERROR"` из `getOutputBufferManager`/`getInputBufferManager` для чужого домена; в процессе самотеста `/blocks/copier` временно снят из реестра (`PluginRegistry::remove("/blocks/copier")`, если есть, с восстановлением). `topology.commit()` бросает `Pothos::TopologyConnectError`, `displayText()` содержит `"/blocks/copier"` и `"PothosBlocks"`.
- [ ] **Step 2: Run** → FAIL (бросается ошибка реестра плагинов без этого текста).
- [ ] **Step 3:** `makeToolkitBlock` и его вызов в обоих местах; doc-комментарий `Topology::commit()`: пересечение несовместимых доменов и топологии на нескольких хостах требуют тулкита blocks.
- [ ] **Step 4: Run** полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(topology): a clear error when the blocks toolkit is needed (copier, network flows)`

### Task 3: `find_package(Pothos)` для соседей ядра-подпроекта

**Files:**
- Modify: `CMakeLists.txt` (после `find_package(Pothos CONFIG REQUIRED)`), `tests/subproject/CMakeLists.txt`
- Create: `cmake/PothosRedirectConfig.cmake.in` (или запись через `file(WRITE)`)

**Interfaces:**
- Produces: при `NOT PROJECT_IS_TOP_LEVEL` файл `${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/pothos-config.cmake`: задаёт `POTHOS_IN_TREE_SOURCE_DIR` и `POTHOS_BUILD_APPS` значениями ядра и подключает `${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfig.cmake`; плюс `pothos-config-version.cmake` (версия из `POTHOS_ABI_VERSION`/`project(VERSION)`, `SameMajorVersion`).

- [ ] **Step 1: Тест:** в `tests/subproject/CMakeLists.txt` после `add_subdirectory(${POTHOS_SOURCE} pothos)` — `add_subdirectory(sibling)` (новый каталог `tests/subproject/sibling/`, проект-тулкит: `find_package(Pothos CONFIG REQUIRED)`, проверки `TARGET Pothos::Pothos`, `POTHOS_UTIL_EXE` непустой при `POTHOS_BUILD_APPS`, библиотека `add_library(sibling_module MODULE m.cpp)` линкуется с `Pothos::Pothos`); и `find_package(Pothos CONFIG REQUIRED)` в самом родителе. Проверка «нет новых непрефиксных cache-записей» продолжает проходить.
- [ ] **Step 2: Run** `ctest -R pothos.subproject` → FAIL (`Could not find a package configuration file provided by "Pothos"`).
- [ ] **Step 3:** запись redirect-файлов (только не на верхнем уровне).
- [ ] **Step 4: Run** полный `ctest` → PASS; standalone-сборка: redirect-файл не создаётся.
- [ ] **Step 5: Commit** `build: find_package(Pothos) finds the core added with add_subdirectory/FetchContent`

### Task 4: Документация, roadmap, журнал, ревью, PR

**Files:**
- Modify: `README.md` (без сабмодулей; где тулкиты: ссылки на репозитории pothosware; runtime-зависимость от blocks; сборка подпроектом и `find_package`; раздел лицензий — без kissfft/Qwt/qtcolorpicker), `Changelog.txt`, `CLAUDE.md` (layout без сабмодулей, заметки про spuce/qwt удалить), roadmap (этап 7 выполнен), журнал `docs/superpowers/ledgers/2026-10-27-phase7-repo-cleanup.md`

- [ ] **Step 1: Документация, roadmap, журнал. Commit** `docs: phase 7 complete — the repository holds only the core`
- [ ] **Step 2: Свежее ревью всей ветки** (фокус — Review Focus), исправления отдельными коммитами с тестами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase7-repo-cleanup`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- Перенос тулкитов на API pothos2 (Poco, libsimdpp, json) — в их репозиториях, отдельными этапами.
- Пакетирование pothos2 (deb/rpm) — позже.
- Подъём `POTHOS_ABI_VERSION` (раскладка портов изменилась в 6b) — к релизу pothos2.
