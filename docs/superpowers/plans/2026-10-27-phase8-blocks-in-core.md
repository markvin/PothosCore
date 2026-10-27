# Этап 8: blocks в дереве ядра — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** тулкит PothosBlocks живёт в `blocks/` дерева PothosCore, собирается вместе с ядром (опция `POTHOS_BUILD_BLOCKS`, ON) на API pothos2 без Poco и без своих копий сторонних библиотек; `/blocks/copier` и `/blocks/network_sink|source` поставляются с ядром.

**Architecture:** история PothosBlocks (upstream `pothosware/PothosBlocks` master `4aef23f`) вливается subtree-merge'ем в `blocks/`. Модули blocks собираются `POTHOS_MODULE_UTIL` внутри проекта ядра и линкуют внутренние цели ядра: `pothos_json` (nlohmann/json), `pothos_xsimd` (SIMD-ядра stream), `pothos_net` → `pothos_asio` (сетевые блоки). Эти цели по-прежнему не устанавливаются: модули — часть сборки ядра. Poco заменяется на `std::` (`std::format`, `std::byteswap`, `std::filesystem`, `<random>`, `<thread>`) и на API ядра (`Pothos::System::log`, `lib/Net`).

**Tech Stack:** git subtree merge, CMake ≥ 3.25, C++23, standalone Asio 1.38.2, xsimd 14.3.0, nlohmann/json 3.11.3, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`; решение этапа 7 (`docs/superpowers/plans/2026-10-27-phase7-repo-cleanup.md`, решение 2) этим этапом пересматривается для blocks.

## Решения пользователя (2026-10-27)

1. Не дублировать сторонние библиотеки в `third_party` тулкитов: **blocks переносится в дерево ядра** и использует `third_party` ядра.
2. Форк `markvin/PothosBlocks` и клон `/home/mark/Projects/PothosBlocks` не используются; их судьба — отдельный вопрос пользователю (ничего не удалять без ответа).

## Решения, которые план предлагает (утверждаются вместе с планом)

1. **История сохраняется**: subtree-merge (`git merge -s ours --allow-unrelated-histories` + `git read-tree --prefix=blocks/`), ~600 коммитов с авторством. PR в `pothos2` вливается merge-коммитом (как #7–#9), иначе история потеряется.
2. Одна опция `POTHOS_BUILD_BLOCKS` (ON; при `POTHOS_BUILD_APPS=OFF` модули собираются без JSON-доков). Поштучные `ENABLE_BLOCKS_*` убираются.
3. Сетевой и SIMD код blocks переписывается, а не оборачивается: Poco-совместимых прослоек не будет.
4. Формат данных на проводе сетевых блоков (заголовки SocketEndpoint, порядок байт) не меняется.

## Global Constraints

- Ничего не скачивается при сборке; CI-guard `No network access` расширяется на `blocks`.
- `pothos_json`, `pothos_xsimd`, `pothos_asio`, `pothos_net` не устанавливаются и не экспортируются; установленные публичные заголовки не включают их.
- Ни одного `#include <Poco/...>`, `Poco::`, `POCO_` в `blocks/` после этапа.
- Плагинные пути и имена блоков/тестов blocks не меняются (`/blocks/copier`, `/blocks/tests/test_*` и т.д.); модули ставятся в `lib/Pothos/modules${POTHOS_ABI_VERSION}/blocks`.
- `POTHOS_WARNINGS_AS_ERRORS` применяется к модулям blocks так же, как к ядру (linux-gcc, linux-clang).
- CI: те же 4 job-а (linux-gcc, linux-clang, macos, windows), все зелёные.
- Не трогать общий `.git/modules` основного клона; никакого bare `git stash`.

## Review Focus

1. **Сборка без apps или без blocks.** `-DPOTHOS_BUILD_BLOCKS=OFF` даёт прежнее ядро (ошибка этапа 7 про отсутствующий copier по-прежнему работает); `-DPOTHOS_BUILD_APPS=OFF` собирает blocks без `--doc-parse`. Проверка — Task 2, шаг конфигурации обоих вариантов.
2. **Сеть на IPv6 и порт 0.** `tcp://[::1]:0` и `udp://[::1]:0` с BIND получают реальный порт, который отдаёт `getActualPort`, и CONNECT к нему передаёт данные. Тест `test_network_blocks_ipv6` (Task 6), пропуск только когда `ipv6Supported()` из `lib/Net/Address.hpp` ложно.
3. **Обрыв соединения и закрытие топологии.** Остановка топологии с подключённым, но молчащим TCP-пиром не виснет (deactivate разблокирует ожидающее чтение). Тест `test_network_deactivate_unblocks` (Task 6) с таймаутом.
4. **SIMD-путь реально исполняется.** На x86_64 и arm64 тесты stream проходят и при SIMD-диспетчеризации, и на скалярном пути; диспетчер не выбирает архитектуру, которую CPU не поддерживает. Task 5: тест `test_simd_dispatch_matches_scalar` сравнивает результаты обоих путей на хвосте, не кратном ширине батча.
5. **Пересечение доменов внутри процесса.** Топология между портами несовместимых доменов теперь коммитится и передаёт данные через `/blocks/copier`. Тест `test_copier_domain_crossing` (Task 4).

---

### Task 1: Влить историю PothosBlocks в `blocks/`

**Files:**
- Create: `blocks/**` (дерево upstream `4aef23f`)
- Delete: `blocks/debian/`
- Modify: — (корневой `CMakeLists.txt` ещё не подключает `blocks/`)

- [ ] **Step 1:** `git fetch https://github.com/pothosware/PothosBlocks master` и проверить, что `FETCH_HEAD` = `4aef23f…`.
- [ ] **Step 2:** subtree-merge:
  ```bash
  git merge -s ours --no-commit --allow-unrelated-histories FETCH_HEAD
  git read-tree --prefix=blocks/ -u FETCH_HEAD
  git commit -m "blocks: import PothosBlocks history (pothosware/PothosBlocks@4aef23f)"
  ```
- [ ] **Step 3:** `git rm -r blocks/debian` и коммит `blocks: drop debian packaging`.
- [ ] **Step 4: Проверка.** `git log --oneline -- blocks/stream/Copier.cpp | wc -l` > 1; `cmake --build build && ctest --test-dir build` → прежний результат (6/6), `blocks/` в сборке не участвует.

### Task 2: Сборка blocks в ядре, модуль packet

**Files:**
- Create: `blocks/CMakeLists.txt` (переписать), `blocks/packet/CMakeLists.txt` (переписать)
- Modify: `CMakeLists.txt` (опция, `add_subdirectory(blocks)`, ctest), `cmake/PothosRunSelfTests.cmake`, `.github/workflows/ci.yml` (guard: `+ blocks`), `blocks/packet/*.cpp` (`<json.hpp>` → `<nlohmann/json.hpp>`)

**Interfaces:**
- Produces: функция `pothos_blocks_module(TARGET <name> SOURCES <...> [LIBRARIES <...>] [TESTS <test_name>...])` в `blocks/CMakeLists.txt`: вызывает `POTHOS_MODULE_UTIL(... DESTINATION blocks [ENABLE_DOCS при POTHOS_BUILD_APPS])`, линкует `pothos_json`, добавляет `blocks/` в include-пути, применяет `pothos_warnings_as_errors`, дописывает `/blocks/tests/<test_name>` в глобальное свойство `POTHOS_BLOCKS_TESTS`. Все следующие задачи регистрируют модули только через неё.
- Produces: `PothosRunSelfTests.cmake` принимает `-DTEST_PATHS=<;-список>` и запускает `--self-test1` для каждого пути, падая на первом неуспешном; ctest `pothos.blocks.selftests` (FIXTURES_REQUIRED `pothos_stage`) передаёт ему `POTHOS_BLOCKS_TESTS`.

- [ ] **Step 1: Тест.** В корневом `CMakeLists.txt` зарегистрировать `pothos.blocks.selftests` с `TEST_PATHS=/blocks/tests/test_packet_blocks;/blocks/tests/test_packet_to_stream;/blocks/tests/test_stream_to_packet` (временно списком; после Step 3 — из свойства).
- [ ] **Step 2: RED.** `ctest --test-dir build -R pothos.blocks` → FAIL: `--self-test1 /blocks/tests/test_packet_blocks` не находит плагин.
- [ ] **Step 3:** опция `option(POTHOS_BUILD_BLOCKS "Build the blocks toolkit (blocks/)" ON)`, `blocks/CMakeLists.txt` без `project()`/`find_package` с `pothos_blocks_module`, модуль `PacketBlocks` с `TESTS` трёх тестов выше; ctest берёт список из свойства.
- [ ] **Step 4: GREEN.** `ctest --test-dir build --output-on-failure` → все тесты, включая `pothos.blocks.selftests`, проходят.
- [ ] **Step 5: Варианты конфигурации.** `cmake -S . -B build-noblocks -DPOTHOS_BUILD_BLOCKS=OFF` собирается, ctest без `pothos.blocks.*`, `test_domain_crossing_needs_blocks` проходит; `cmake -S . -B build-noapps -DPOTHOS_BUILD_APPS=OFF` собирается.
- [ ] **Step 6: Коммит** `blocks: build in the core tree, packet module`.

### Task 3: Модули event, serialize, testers без Poco

**Files:**
- Modify: `blocks/{event,serialize,testers}/CMakeLists.txt` (через `pothos_blocks_module`), `blocks/event/{Evaluator,MessagePrinter}.cpp`, `blocks/serialize/{Serializer,Deserializer}.cpp`, `blocks/testers/{Abort,CollectorSink,TestConstantSource}.cpp`, `<json.hpp>` во всех тестах этих модулей

Замены (общие и для Task 4–6): `Poco::format` / `NumberFormatter` → `std::format`; `Poco::ByteOrder` → `std::byteswap` + `std::endian`; `Poco::Logger` → `Pothos::System::log` (`include/Pothos/System/Logger.hpp`, тот же source-name); `Poco::Thread::sleep` → `std::this_thread::sleep_for`; `Poco::Random`/`RandomStream` → `std::mt19937` / `std::random_device`; `SingletonHolder` → static-локальная переменная функции; `Poco/Types.h`, `Platform.h`, `Foundation.h` → `<cstdint>` или удалить.

- [ ] **Step 1: RED.** Добавить модули `EventBlocks`, `SerializeBlocks`, `TesterBlocks` в сборку с их `TESTS` (все `POTHOS_TEST_BLOCK` из этих каталогов, см. grep `POTHOS_TEST_BLOCK` в них); сборка падает на `Poco/*.h`.
- [ ] **Step 2:** заменить Poco по таблице выше.
- [ ] **Step 3: GREEN.** `cmake --build build && ctest --test-dir build -R 'pothos.blocks|pothos.selftests' --output-on-failure` → PASS; `grep -rn Poco blocks/{event,serialize,testers}` пусто.
- [ ] **Step 4: Коммит** `blocks: port event, serialize and testers off Poco`.

### Task 4: Модули file и stream (скалярный путь), copier

**Files:**
- Modify: `blocks/{file,stream}/CMakeLists.txt`, `blocks/file/{BinaryFileSource,MemoryMappedBufferContainer,TestBinaryFileBlocks,TextFileSink}.cpp`, `blocks/stream/{Clamp,Round,TestFirstN}.cpp`, `<json.hpp>` в тестах
- Create: `blocks/stream/TestCopier.cpp`

Замены сверх Task 3: `Poco::File` → `std::filesystem`; `Poco::TemporaryFile` → уникальный путь в `std::filesystem::temp_directory_path()` с удалением в RAII-guard теста. В этой задаче stream собирается без `SIMD/` (как upstream без xsimd).

- [ ] **Step 1: Тест `test_copier_domain_crossing`** в `blocks/stream/TestCopier.cpp`: два блока с выходным/входным портами доменов `copier_test_a`/`copier_test_b` (каждый бросает `PortDomainError` на чужой домен, как в `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`), между ними 1000 элементов `int`; `topology.commit()` не бросает, `topology.waitInactive()`, принятый буфер равен отправленному.
- [ ] **Step 2: RED.** Модули `FileBlocks`, `StreamBlocks` в сборке с их `TESTS` + `test_copier_domain_crossing` → сборка падает на Poco.
- [ ] **Step 3:** заменить Poco.
- [ ] **Step 4: GREEN.** ctest как в Task 3 → PASS (включая core-тест `test_domain_crossing_needs_blocks`, который сам снимает copier из реестра).
- [ ] **Step 5: Коммит** `blocks: port file and stream off Poco, in-process domain crossing works`.

### Task 5: SIMD-ядра stream на xsimd 14

**Files:**
- Modify: `blocks/stream/CMakeLists.txt` (`PothosGenerateSIMDSources(... SIMD/StreamBlocks.json ...)`, `LIBRARIES pothos_xsimd`, `POTHOS_XSIMD`), `blocks/stream/SIMD/{Clamp,IsX,MinMax,Round}.cpp`, `blocks/stream/{Clamp,IsX,MinMax,Round}.cpp` (диспетчер)
- Create: `blocks/stream/TestSIMDDispatch.cpp`

**Interfaces:**
- Consumes: `PothosGenerateSIMDSources(<var> <json> <inputs...>)` из `cmake/Modules/PothosConfigSIMD.cmake` (подключить `include(${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfigSIMD.cmake)`), `Pothos/Util/XSIMDTraits.hpp`.

- [ ] **Step 1: Тест `test_simd_dispatch_matches_scalar`**: для `clamp`, `minmax`, `round`, `isnan/isinf` на `float` и `double`, длина `4*batch+3`, значения со знаками, ±inf, NaN — результат SIMD-функции, выбранной диспетчером, совпадает со скалярной (побитово для isX, равенство для остальных).
- [ ] **Step 2: RED.** Подключить SIMD-генерацию → сборка падает на старом API (`xsimd::simd_type`/`set_simd`/`batch_bool<T,N>`).
- [ ] **Step 3:** перевести SIMD-ядра на xsimd 14: `xsimd::batch<T, Arch>`, `xsimd::batch_bool<T, Arch>`, `load_unaligned`/`store_unaligned`, `xsimd::select`, `xsimd::isnan`; архитектуры из `StreamBlocks.json`, которых нет в xsimd 14, убрать из JSON.
- [ ] **Step 4: GREEN.** ctest → PASS на x86_64 локально; `ninja -t targets all | grep StreamBlocks_SIMD` показывает сгенерированные файлы.
- [ ] **Step 5: Коммит** `blocks: stream SIMD kernels on xsimd 14`.

### Task 6: Сетевые блоки на Asio

**Files:**
- Modify: `blocks/network/CMakeLists.txt` (`LIBRARIES pothos_net`), `blocks/network/{SocketEndpoint.hpp,SocketEndpoint.cpp,DatagramIO.cpp,NetworkSource.cpp,NetworkSink.cpp,TestNetworkBlocks.cpp}`

**Interfaces:**
- Consumes: `lib/Net/Uri.hpp` (разбор `tcp://host:port`, `udp://…`), `lib/Net/Address.hpp` (`parseHostPort`, `resolveHost`, `isWildcard`, `ipv6Supported`), `pothos_asio`.
- Produces: тот же публичный для модуля интерфейс `PothosPacketSocketEndpoint` из `SocketEndpoint.hpp` (конструктор `(uri, opt)`, `openComms`, `closeComms`, `getActualPort`, `send`/`recv` с таймаутом); реализация — собственный `asio::io_context` на endpoint, таймауты через `run_for`.

- [ ] **Step 1: Тесты.** `test_network_blocks_ipv6` — копия `test_network_blocks` на `[::1]`, пропуск с сообщением, когда `ipv6Supported()` ложно; `test_network_deactivate_unblocks` — sink BIND, сырой TCP-клиент подключается и молчит, `topology.commit()` затем деструктор топологии завершаются за ≤ 5 с (watchdog-поток как в `TestReentrantPlugins.cpp`).
- [ ] **Step 2: RED.** Модуль `NetworkBlocks` в сборке с `TESTS test_network_blocks test_network_blocks_ipv6 test_network_deactivate_unblocks test_network_topology` → сборка падает на `Poco/Net/*`.
- [ ] **Step 3:** переписать `SocketEndpoint.cpp` и `DatagramIO.cpp` на Asio (tcp: acceptor/socket; udp: socket с connect/bind, MTU как раньше); формат заголовков и порядок байт без изменений.
- [ ] **Step 4: GREEN.** ctest → PASS, включая `test_network_topology` (топология через процессы с `/blocks/network_*`); `grep -rn Poco blocks` пусто.
- [ ] **Step 5: Коммит** `blocks: network blocks on Asio`.

### Task 7: Ядро и документация после переноса

**Files:**
- Modify: `lib/Framework/TopologyToolkitBlocks.hpp` (текст ошибки), `lib/Framework/Builtin/TestTopologyToolkitBlocks.cpp`, `include/Pothos/Framework/Topology.hpp` (doc `commit()`), `README.md`, `CLAUDE.md`, `Changelog.txt`, `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`, `blocks/README.md`
- Keep: `blocks/Changelog.txt` (история релизов PothosBlocks)

- [ ] **Step 1: Тест.** В `test_domain_crossing_needs_blocks` заменить ожидание `"PothosBlocks"` на `"POTHOS_BUILD_BLOCKS"` (остальные ожидания — `/blocks/copier`, `in this process` — без изменений).
- [ ] **Step 2: RED.** ctest `pothos.selftests` → FAIL на новом ожидании.
- [ ] **Step 3:** сообщение: `"... needs the block <path> from the blocks module, which is not installed <location> (build the core with POTHOS_BUILD_BLOCKS=ON)"`; doc `commit()` и README: blocks входят в ядро, опция `POTHOS_BUILD_BLOCKS`; PothosBlocks убрать из списка внешних тулкитов, раздел о зависимости от тулкита переписать; Changelog/roadmap — строка этапа 8; лицензия blocks — BSL-1.0, как у ядра.
- [ ] **Step 4: GREEN.** Полный `ctest --test-dir build --output-on-failure` → PASS; `grep -rn "PothosBlocks" README.md CLAUDE.md include lib` — только историческая ссылка на upstream в `blocks/README.md`.
- [ ] **Step 5: Коммит** `core: blocks ship with the core, update docs and error text`.
