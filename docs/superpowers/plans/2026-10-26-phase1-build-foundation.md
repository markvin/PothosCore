# Этап 1: фундамент сборки — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ядро PothosCore конфигурируется современным CMake (в т.ч. 4.x), собирается в C++23 на Linux/macOS/Windows, самотесты запускаются через CTest и зелёные в GitHub Actions; Poco и muparserx временно остаются, но строго приватными.

**Architecture:** корневой `CMakeLists.txt` собирает только ядро (toolkits и spuce убраны). Poco и muparserx — in-tree сабмодули, подключаемые через `add_subdirectory` с `CMAKE_POLICY_VERSION_MINIMUM=3.5` и линкуемые `PRIVATE`. `json.hpp` вендорится в `third_party/`. Самотесты требуют установленной раскладки (`Pothos::init()` проверяет `bin/PothosUtil`, библиотеку, `include/`), поэтому CTest сначала делает `cmake --install` в `<build>/stage`, затем запускает `stage/bin/PothosUtil --self-tests` с изолированными пользовательскими каталогами. Пакет экспортируется как `Pothos::Pothos` без следов Poco.

**Tech Stack:** CMake ≥ 3.25 (проверено на 4.4.4), Ninja, GCC 13+/Clang 17+/AppleClang (Xcode 15+)/MSVC 17.8+, nlohmann/json 3.11.3 (single header), libsimdpp (сабмодуль), Poco 1.8.1 (временно), muparserx (временно), GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (дорожная карта: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`)

## Global Constraints

- Стандарт: C++23 (`cxx_std_23`, без расширений компилятора).
- `cmake_minimum_required(VERSION 3.25)`; проект должен конфигурироваться CMake 4.x без флагов от пользователя.
- Платформы: Linux, macOS (минимум 13.3, CI arm64), Windows (MSVC).
- Минимальные компиляторы: GCC 13+, Clang 17+, Xcode 15+, Visual Studio 2022 17.8+.
- Сборка не обращается к сети и не требует системных библиотек, кроме стандартных библиотек ОС.
- Никакой тип/заголовок внутренней зависимости (Poco, muparserx, json, libsimdpp) не попадает в `INTERFACE_*` свойства экспортируемого target-а и в `PothosConfig.cmake`.
- Модули (toolkits) не собираются и не трогаются.
- Самотесты, падающие из-за окружения CI (не из-за порта), можно отключать на конкретной платформе — с комментарием о причине.
- Пушить в `origin` (форк `markvin/PothosCore`) — только после явного согласия пользователя.

## Review Focus

1. **Потребитель установленного пакета без Poco:** внешний проект делает `find_package(Pothos CONFIG REQUIRED)` и линкует `Pothos::Pothos` — должен собраться, не видя заголовков Poco/json/muparserx. Покрыто тестом `pothos.consumer` (Task 4).
2. **Сборка без сети:** нигде нет `file(DOWNLOAD)`/`FetchContent`/`ExternalProject`. Покрыто CI-шагом grep (Task 6).
3. **Debug-постфикс библиотеки** (`libPothosd.so`): `Pothos::init()` ищет библиотеку по `$<TARGET_FILE_NAME:Pothos>` — должно работать и в Debug, и в Release. Покрыто матрицей CI Debug+Release (Task 6).
4. **Самотесты не трогают домашний каталог пользователя:** `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `APPDATA` указывают внутрь каталога сборки. Покрыто скриптом запуска (Task 2).
5. **Нестандартный `CMAKE_INSTALL_LIBDIR`** (`lib64`) — `Paths.cpp` строит пути относительно корня; staged install с `-DCMAKE_INSTALL_LIBDIR=lib64` должен проходить самотесты. Покрыто одной Linux-конфигурацией CI (Task 6).

---

### Task 1: Конфигурация на CMake 4 и сборка ядра в C++23

**Files:**
- Modify: `CMakeLists.txt` (весь файл: версия, стандарт, блоки Poco/spuce/muparserx/json/toolkits/uninstall)
- Modify: `lib/CMakeLists.txt:1-60` (поиск зависимостей), `:242-254` (линковка), `:259-269` (флаги)
- Modify: `apps/CMakeLists.txt`
- Modify: `cmake/Modules/PothosConfig.cmake:62-75` (in-tree ветка: `IN_TREE_LIBRARIES`)
- Create: `third_party/nlohmann/json.hpp` (nlohmann/json v3.11.3, `single_include/nlohmann/json.hpp`, MIT — лицензия внутри заголовка)
- Create: `third_party/CMakeLists.txt`
- Modify: `lib/Framework/TopologyMakeJSON.cpp`, `lib/Framework/TopologyDumpJSON.cpp`, `lib/Framework/WorkerActor.cpp`, `lib/Framework/TopologyStatsJSON.cpp`, `lib/Framework/ThreadPool.cpp`, `lib/Util/Builtin/*.cpp`, `apps/*.cpp` — всё, что включает `<json.hpp>` (найти: `grep -rln 'json.hpp' lib apps`)

**Interfaces:**
- Produces: CMake-target `pothos_json` (INTERFACE, не экспортируется, include-путь `third_party/`); include в коде — `#include <nlohmann/json.hpp>`.
- Produces: Pothos линкует `PRIVATE` Poco-таргеты `PocoFoundation PocoUtil PocoXML PocoNet` и `muparserx`; `PothosUtil` линкует `PRIVATE Pothos PocoFoundation PocoUtil PocoNet pothos_json`.
- Produces: опция `POTHOS_ENABLE_LIBNUMA` (по умолчанию `OFF`), включает текущую ветку `find_library(numa)` в `lib/CMakeLists.txt`.

- [ ] **Step 1: Зафиксировать текущий провал**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
Expected: FAIL — ошибка совместимости `cmake_minimum_required` (CMake 4 не поддерживает < 3.5) или `INTERFACE_INCLUDE_DIRECTORIES ... prefixed in the source directory` для `poco/*/include`.

- [ ] **Step 2: Переписать корневой `CMakeLists.txt`**

- Версию из `Changelog.txt` читать **до** `project()`; `project(Pothos VERSION ${POTHOS_LIBVER} LANGUAGES C CXX)`.
- `set(CMAKE_CXX_STANDARD 23)`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` (глобально — Poco тоже собирается в C++23, это проверено экспериментом).
- Poco: всегда in-tree (ветку внешнего Poco/`SetupPoco` удалить). Внутри `block()`: `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)`, текущий набор `ENABLE_* OFF` из `POCO_MIN_BUILD` + `ENABLE_JSON OFF`, `ENABLE_PAGECOMPILER OFF`, `ENABLE_PAGECOMPILER_FILE2PAGE OFF`, `ENABLE_REDIS OFF`; `add_subdirectory(poco)`. Убрать глобальный `include_directories(${Poco_INCLUDE_DIRS})`.
- muparserx: внутри `block()` с `CMAKE_POLICY_VERSION_MINIMUM 3.5`, `BUILD_SHARED_LIBS OFF`, `CMAKE_POSITION_INDEPENDENT_CODE ON`: `add_subdirectory(muparserx EXCLUDE_FROM_ALL)` — статическая, не устанавливается, пример muparserx не собирается.
- Удалить блоки spuce, json-download (вместе с `install(FILES json.hpp)`), `ENABLE_TOOLKITS` и foreach по модулям.
- `add_subdirectory(third_party)` до `lib`.
- uninstall: оставить только ветку собственного `uninstall`-таргета (без подмены Poco-скрипта).

- [ ] **Step 3: Обновить `lib/CMakeLists.txt` и `apps/CMakeLists.txt`**

- Удалить секции «locate Poco», «muparserx library», «json.hpp header» и условие `Poco_FOUND;muparserx_FOUND;JSON_HPP_INCLUDE_DIR` в `cmake_dependent_option(ENABLE_LIBRARY_LIB ...)`.
- `target_link_libraries(Pothos PRIVATE PocoFoundation PocoUtil PocoXML PocoNet muparserx pothos_json)`; include-путь muparserx — `PRIVATE ${PROJECT_SOURCE_DIR}/muparserx/parser`; убрать `PUBLIC ${Poco_INCLUDE_DIRS}`.
- `target_compile_features(Pothos PUBLIC cxx_std_23)`.
- libnuma — под `if(POTHOS_ENABLE_LIBNUMA)`, иначе текущий выбор `NumaInfoWindows/OSX/Other`.
- `apps/CMakeLists.txt`: линковка по Interfaces выше, убрать `${JSON_HPP_INCLUDE_DIR}`.
- `PothosConfig.cmake` (in-tree ветка): из `IN_TREE_LIBRARIES` убрать `MUPARSERX_IN_TREE`-часть (muparserx статическая); Poco-часть оставить (её DLL/so нужны для `PothosUtil.sh/.bat`), список — `PocoFoundation PocoUtil PocoXML PocoNet`.

- [ ] **Step 4: Перевести код на `<nlohmann/json.hpp>`**

Заменить include во всех файлах из списка; исправить расхождения API nlohmann 2.1.1 → 3.11.3, если компилятор их покажет (поведение JSON-вывода не менять).

- [ ] **Step 5: Собрать**

Run: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
Expected: конфигурация без ошибок (предупреждения о политиках из сабмодулей допустимы), сборка завершена, `FAILED` нет.

- [ ] **Step 6: Проверить исполняемый файл**

Run: `./build/apps/PothosUtil --success-code 200; echo $?`
Expected: `200`

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt lib/CMakeLists.txt apps/CMakeLists.txt cmake/Modules/PothosConfig.cmake third_party lib apps
git commit -m "build: configure with modern CMake, C++23, private in-tree Poco/muparserx, vendored json"
```

---

### Task 2: Самотесты через CTest (staged install)

**Files:**
- Create: `cmake/PothosRunSelfTests.cmake` (скрипт для `cmake -P`)
- Modify: `CMakeLists.txt` (секция тестов в конце)

**Interfaces:**
- Consumes: install-правила из Task 1.
- Produces: CTest-тест `pothos.stage` (`FIXTURES_SETUP pothos_stage`): `${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${CMAKE_BINARY_DIR}/stage --config $<CONFIG>`.
- Produces: CTest-тест `pothos.selftests` (`FIXTURES_REQUIRED pothos_stage`): `${CMAKE_COMMAND} -DPREFIX=${CMAKE_BINARY_DIR}/stage -DHOME_DIR=${CMAKE_BINARY_DIR}/test-home -DBINDIR=${CMAKE_INSTALL_BINDIR} -DLIBDIR=${CMAKE_INSTALL_LIBDIR} -P cmake/PothosRunSelfTests.cmake`; принимает необязательный `-DTEST_PATH=<путь>` → `--self-test1 <путь>` вместо `--self-tests`.
- Produces: переменная `POTHOS_SELFTEST_TIMEOUT` (по умолчанию 900 с) → свойство `TIMEOUT` теста.

- [ ] **Step 1: Убедиться, что тестов нет**

Run: `ctest --test-dir build -N`
Expected: `Total Tests: 0`

- [ ] **Step 2: Написать `cmake/PothosRunSelfTests.cmake`**

Скрипт: очищает и создаёт `HOME_DIR/data`, `HOME_DIR/config`; выставляет `ENV{POTHOS_ROOT}=PREFIX`, `ENV{XDG_DATA_HOME}`, `ENV{XDG_CONFIG_HOME}`, `ENV{APPDATA}=HOME_DIR/data`; на Windows добавляет `PREFIX/BINDIR` в начало `PATH`, на Linux `PREFIX/LIBDIR` в `LD_LIBRARY_PATH`, на macOS — в `DYLD_LIBRARY_PATH`; запускает `PREFIX/BINDIR/PothosUtil[.exe]` через `execute_process(... RESULT_VARIABLE rc)`, вывод — в консоль; `message(FATAL_ERROR ...)` при `rc` ≠ 0.

- [ ] **Step 3: Зарегистрировать тесты в `CMakeLists.txt`**

`enable_testing()` уже есть; добавить `pothos.stage` и `pothos.selftests` по Interfaces.

- [ ] **Step 4: Запустить**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `pothos.stage` PASS; `pothos.selftests` FAIL, в выводе `Failed 1 out of 88 tests` и `FAIL: /framework/tests/test_mmap_shared_buffer` (дефект до модернизации, чинится в Task 3). Домашний каталог не тронут: `ls ~/.local/share/Pothos ~/.config/Pothos` не показывает файлов новее запуска.

- [ ] **Step 5: Commit**

```bash
git add cmake/PothosRunSelfTests.cmake CMakeLists.txt
git commit -m "test: run plugin self-tests through CTest against a staged install"
```

---

### Task 3: Починить `test_mmap_shared_buffer`

**Files:**
- Investigate: `lib/Framework/Builtin/TestSharedBuffer.cpp:60-100`, `lib/Framework/MemoryMappedBufferContainer.cpp`, `lib/Framework/SharedBufferUnix.cpp`, `lib/Framework/SharedBufferWindows.cpp`
- Modify: файл, в котором окажется причина

**Interfaces:**
- Consumes: `ctest -R pothos.selftests` и `-DTEST_PATH` из Task 2.

Использовать superpowers:systematic-debugging. Симптом: `TestSharedBuffer.cpp:86`, `assert equal 4096 == 0`, `statement "(input.length) == (tempFile.getSize())"` — после записи через mmap-буфер файл имеет размер 0. Тест уже существует и падает — он и есть «failing test». Чинить код, а не ослаблять тест, если только не доказано, что тест проверяет неверное ожидание.

- [ ] **Step 1: Воспроизвести одиночный тест**

Run: `cmake -DPREFIX=build/stage -DHOME_DIR=build/test-home -DBINDIR=bin -DLIBDIR=lib -DTEST_PATH=/framework/tests/test_mmap_shared_buffer -P cmake/PothosRunSelfTests.cmake`
Expected: FAIL с `assert equal 4096 == 0`

- [ ] **Step 2: Найти первопричину и исправить**

- [ ] **Step 3: Проверить**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `pothos.selftests` PASS, в выводе `All 88 tests passed!`.

- [ ] **Step 4: Commit**

```bash
git commit -am "fix: <первопричина> in memory-mapped shared buffer"
```

---

### Task 4: Экспорт пакета `Pothos::Pothos` без Poco

**Files:**
- Modify: `lib/CMakeLists.txt` (секция install/export), `cmake/Modules/CMakeLists.txt`, `cmake/Modules/PothosConfig.cmake:150-184` (установленная ветка), `cmake/Modules/PothosUtil.cmake:102`, `lib/Pothos.in.pc`
- Delete: `cmake/Modules/FindPoco.cmake`, `cmake/Modules/SetupPoco.cmake`, `cmake/Modules/FindPkgMacros.cmake`
- Create: `tests/consumer/CMakeLists.txt`, `tests/consumer/main.cpp`, `cmake/PothosConsumerTest.cmake`
- Modify: `CMakeLists.txt` (тест `pothos.consumer`)

**Interfaces:**
- Produces: `add_library(Pothos::Pothos ALIAS Pothos)` в дереве; `install(EXPORT PothosExport NAMESPACE Pothos:: FILE PothosTargets.cmake DESTINATION ${POTHOS_CMAKE_DIRECTORY})`; установленный `PothosConfig.cmake` делает `include(${CMAKE_CURRENT_LIST_DIR}/PothosTargets.cmake)` и **не** включает `SetupPoco`, не вызывает `enable_language(C)`.
- Produces: `POTHOS_MODULE_UTIL` линкует `Pothos::Pothos`; старые переменные `Pothos_LIBRARIES` = `Pothos::Pothos`.
- Produces: CTest-тест `pothos.consumer` (`FIXTURES_REQUIRED pothos_stage`): `cmake -DPREFIX=... -DSRC=${PROJECT_SOURCE_DIR}/tests/consumer -DWORK=${CMAKE_BINARY_DIR}/consumer -DGENERATOR=${CMAKE_GENERATOR} -DCONFIG=$<CONFIG> -P cmake/PothosConsumerTest.cmake` — конфигурирует, собирает и запускает потребителя.

- [ ] **Step 1: Написать потребителя**

`tests/consumer/CMakeLists.txt`: `cmake_minimum_required(VERSION 3.25)`, `project(PothosConsumer CXX)`, `find_package(Pothos CONFIG REQUIRED)`, `add_executable(consumer main.cpp)`, `target_link_libraries(consumer PRIVATE Pothos::Pothos)`. Затем проверка утечки:

```cmake
foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_LINK_LIBRARIES INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(val Pothos::Pothos ${prop})
    if(val MATCHES "Poco|muparserx|nlohmann|simdpp")
        message(FATAL_ERROR "Pothos::Pothos leaks a private dependency via ${prop}: ${val}")
    endif()
endforeach()
```

`tests/consumer/main.cpp`: создаёт `Pothos::Object(42)`, проверяет `obj.convert<int>() == 42` и что `Pothos::System::getApiVersion()` не пуст; возвращает 0 при успехе.

- [ ] **Step 2: Написать `cmake/PothosConsumerTest.cmake` и зарегистрировать `pothos.consumer`**

Скрипт: `execute_process` configure (`-G GENERATOR`, `-DCMAKE_PREFIX_PATH=PREFIX`, `-DCMAKE_BUILD_TYPE=CONFIG`), build, запуск `consumer` с тем же окружением путей библиотек, что в `PothosRunSelfTests.cmake` (вынести общую функцию в `cmake/PothosTestEnv.cmake`); `FATAL_ERROR` на любом шаге с ненулевым кодом.

- [ ] **Step 3: Запустить — должно упасть**

Run: `cmake --build build && ctest --test-dir build -R pothos.consumer --output-on-failure`
Expected: FAIL — `Pothos::Pothos` не найден (экспорт без namespace) или `SetupPoco`/Poco требуется при `find_package`.

- [ ] **Step 4: Переделать экспорт по Interfaces**

Удалить три Poco-модуля и их `install(FILES ...)`; из `Pothos.in.pc` убрать Poco/muparserx из `Requires`/`Libs.private` (статический muparserx встроен в libPothos).

- [ ] **Step 5: Проверить**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: все три теста PASS.

- [ ] **Step 6: Commit**

```bash
git add -A cmake tests lib/CMakeLists.txt lib/Pothos.in.pc CMakeLists.txt
git commit -m "build: export Pothos::Pothos without private dependencies, add consumer test"
```

---

### Task 5: libsimdpp и гигиена флагов

**Files:**
- Modify: сабмодуль `libsimdpp` (обновить до текущего `master` апстрима `https://github.com/p12tic/libsimdpp`; на 2026-10-26 это `55860b50`, `v2.0-rc1-574`)
- Modify: `lib/CMakeLists.txt:259-283` (флаги), `:432-445` (SIMD)

**Interfaces:**
- Consumes: `simdpp_get_runnable_archs`, `simdpp_multiarch` из `libsimdpp/cmake/SimdppMultiarch.cmake`.

- [ ] **Step 1: Зафиксировать текущие предупреждения**

Run: `cmake --build build --clean-first 2>&1 | grep -c 'libsimdpp/.*warning'`
Expected: число > 0 (≈2100, в основном `-Wtemplate-id-cdtor`).

- [ ] **Step 2: Обновить сабмодуль, подключить заголовки как SYSTEM**

`target_include_directories(Pothos SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/libsimdpp)`. Адаптировать вызовы `simdpp_multiarch`, если API изменился.

- [ ] **Step 3: Перевести навязывание флагов потребителям в PRIVATE**

`-Wall -Wextra -Wnon-virtual-dtor` и MSVC `/MP` — `PRIVATE`. `NOMINMAX`, `/wd4251 /wd4275 /wd4503`, `-flat_namespace` (Apple) остаются `PUBLIC` (нужны потребителям заголовков/модулей). Удалить FreeBSD-ветку с `-stdlib=libc++` только если она мешает; иначе оставить.

- [ ] **Step 4: Проверить**

Run: `cmake --build build --clean-first 2>&1 | grep -c 'libsimdpp/.*warning'; ctest --test-dir build --output-on-failure`
Expected: `0`; все тесты PASS (SIMD-тесты `/util/tests/test_simd_dispatcher_utils_dump_header` и конвертации буферов — в составе самотестов).

- [ ] **Step 5: Commit**

```bash
git add libsimdpp lib/CMakeLists.txt
git commit -m "build: update libsimdpp, include it as system headers, keep warning flags private"
```

---

### Task 6: CI на GitHub Actions, удаление старого CI, документация

**Files:**
- Create: `.github/workflows/ci.yml`
- Delete: `.travis.yml`, `appveyor.yml`
- Modify: `CLAUDE.md` (раздел Build), `README.md` (раздел сборки, если он есть)

**Interfaces:**
- Consumes: тесты `pothos.stage`, `pothos.selftests`, `pothos.consumer`.

Матрица (имена runner-ов проверить на момент реализации — бери актуальные `*-latest`, если перечисленные сняты):

| job | runner | компилятор | конфигурация | доп. флаги |
|---|---|---|---|---|
| linux-gcc | ubuntu-24.04 | GCC 14 | Debug | — |
| linux-clang | ubuntu-24.04 | Clang 18 | Release | `-DCMAKE_INSTALL_LIBDIR=lib64` |
| macos | macos-latest (arm64) | AppleClang (Xcode ≥ 15) | Debug | `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3` |
| windows | windows-latest | MSVC (VS 2022) через `ilammy/msvc-dev-cmd` | Release | — |

- [ ] **Step 1: Написать workflow**

Триггеры: `push`, `pull_request`. Шаги: checkout без сабмодулей → `git submodule update --init poco muparserx libsimdpp` (без `--depth`: зафиксированные коммиты не на вершине) → шаг-проверка «без сети»: `! grep -rnE 'file\s*\(\s*DOWNLOAD|FetchContent|ExternalProject' CMakeLists.txt lib apps cmake third_party` → configure `-G Ninja` → build → `ctest --output-on-failure`.

Правило (моё решение): если job падает **внутри кода Poco** (не Pothos) на конкретной платформе, допустимо временно пометить этот job `continue-on-error: true` с комментарием, какой этап дорожной карты убирает эту часть Poco. Падения кода Pothos так не маскируются.

- [ ] **Step 2: Проверить workflow локально на синтаксис**

Run: `python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml'))" && echo ok`
Expected: `ok`

- [ ] **Step 3: Удалить `.travis.yml`, `appveyor.yml`; обновить `CLAUDE.md` (Build: команды configure/build/ctest из этого плана, список нужных сабмодулей ядра, заметка о staged install и `POTHOS_ROOT`)**

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/ci.yml CLAUDE.md README.md
git rm .travis.yml appveyor.yml
git commit -m "ci: GitHub Actions for Linux/macOS/Windows, drop Travis and AppVeyor"
```

- [ ] **Step 5: Спросить пользователя разрешение на push в `origin`; после push — дождаться результата workflow (`gh run watch`)**

Expected: все четыре job-а зелёные (или помечены `continue-on-error` строго по правилу выше — перечислить такие в отчёте).
