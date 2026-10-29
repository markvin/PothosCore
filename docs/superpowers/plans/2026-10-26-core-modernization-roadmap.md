# Модернизация ядра — дорожная карта

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md`

Работа разбита на этапы; у каждого свой план, после каждого ядро собирается
и `PothosUtil --self-tests` зелёные в CI на Linux/macOS/Windows. Poco
убирается постепенно — промежуточные этапы держат его как **приватную**
in-tree зависимость (спека это разрешает).

| # | Этап | Результат | План |
|---|------|-----------|------|
| 1 | Фундамент сборки | CMake ≥ 3.25 / работает на 4.x, C++23, json в дереве, Poco и muparserx приватные, самотесты через CTest, экспорт `Pothos::Pothos` без Poco, CI на GitHub Actions | `2026-10-26-phase1-build-foundation.md` |
| 2 | Свой вычислитель выражений | `EvalEnvironment` без muparserx, сабмодуль muparserx удалён | пишется после этапа 1 |
| 3 | Замена Poco Foundation | `std::format`/`filesystem`/…, внутренний платформенный слой (DynamicLibrary, Process+Pipe, Environment), публичный logging API; Poco остаётся только в Net/Util | пишется после этапа 2 |
| 4 | Сеть на Asio | Asio в дереве, Remote (клиент/сервер/прокси), syslog-совместимые forward/listener, `Util/Network`; сетевой слой готов к будущему HTTP/WebSocket (web-UI) | пишется после этапа 3 |
| 5 | PothosUtil без Poco::Util и удаление Poco | свой разбор аргументов, завершение по сигналам, `--daemon`/`--pidfile`/`--umask`, служба Windows; сабмодуль poco удалён | `2026-10-27-phase5-remove-poco.md` |
| 5a | libsimdpp → xsimd | xsimd копией в `third_party/xsimd`: определение CPU и `SIMDConvert` на xsimd, `PothosConfigSIMD.cmake` без `SimdppMultiarch`, сабмодуль libsimdpp удалён; имена архитектур (`sse2`, `avx2`…) — сохранить или менять вместе с тулкитами | пишется после этапа 5 |
| 6 | Модернизация публичного API | проход по `include/Pothos/**` (std::span/optional/стандартные примитивы), чистка предупреждений; CMake-гигиена для подключения ядра подпроектом (решение пользователя, 2026-10-27, до этапа 7): опции с префиксом `POTHOS_` вместо `ENABLE_LIBRARY*` (и будущих `ENABLE_PYTHON`/`ENABLE_BLOCKS*`), тесты/документация/`feature_summary`/глобальные `CMAKE_*` только при `PROJECT_IS_TOP_LEVEL`, `PROJECT_BINARY_DIR` вместо `CMAKE_BINARY_DIR` (сейчас `pothos.stage` в подпроекте установил бы весь родительский проект), стандарт и rpath — свойствами целей, CI-проверка «ядро через `add_subdirectory`» с одноимёнными опциями родителя | пишется после этапа 5 |
| 7 | Чистка репозитория (решения пользователя, 2026-10-27) | в PothosCore остаётся только ядро (`lib`, `include`, `apps`); все сабмодули и их каталоги удаляются (ядро самодостаточно); `blocks`, `python`, `flow`, `widgets`, `soapy`, `plotters`, `comms`, `audio` переезжают в отдельные репозитории (`blocks` и `python` — тоже, уточнения пользователя 2026-10-27; раньше планировались в дерево ядра); README.md и Changelog.txt корректируются. Учесть для сборки «ядро + тулкиты» подпроектами: `find_package(Pothos)` у родителя/соседей после `add_subdirectory(ядра)` сейчас не находит конфигурацию (`Pothos_DIR` — обычная переменная в области ядра; замечание ревью 6a) | пишется после этапа 6 |
| 8 | blocks в дереве ядра (решение пользователя, 2026-10-27) | история PothosBlocks в `blocks/`, сборка с ядром (`POTHOS_BUILD_BLOCKS`) на `third_party` ядра (json, xsimd, Asio через `lib/Net`), без Poco; copier и сетевые блоки поставляются с ядром | `2026-10-27-phase8-blocks-in-core.md` |
| 9 | Зависимости через find_package, blocks снова отдельно (решение пользователя, 2026-10-28; отменяет этап 8) | `third_party/` удалён: nlohmann_json, Asio, xsimd через `find_package` (системные пакеты или Conan 2, `conanfile.txt`; CMake не знает о Conan); blocks — форк `markvin/PothosBlocks` (ветка `pothos2`, свой `conanfile.txt`), только публичный API ядра (`Pothos::Util::parseUri/resolveHost`, публичный `XSIMDCopy.hpp`); CI: 4 job'а через Conan + `linux-system` (Arch) | `2026-10-28-phase9-conan-deps.md` |
| 10 | comms на ядре pothos2 (решения пользователя, 2026-10-28) | форк `markvin/PothosComms` (ветка `pothos2`): сборка против установленного ядра (`find_package`, nlohmann_json, xsimd; системные пакеты или Conan 2), без Poco; `math/SIMD` на `XSIMDCopy.hpp` и API xsimd 14; spuce заменён своей внутренней библиотекой `comms_filter_design` (`filter/design/`, порт spuce `ad08df7`, эталонные данные из spuce); самотесты требуют PothosBlocks в префиксе ядра; CI: 4 job'а через Conan + `linux-system` (Arch) | `2026-10-28-phase10-comms.md` |
| 11 | PothosBase — агрегирующая сборка (решения пользователя, 2026-10-28) | репозиторий `markvin/PothosBase` (ветка `main`): core, blocks, comms — сабмодули на `pothos2`, одно дерево CMake (`add_subdirectory`), `POTHOS_BASE_<NAME>_DIR` подставляет локальный клон; тесты `base.stage`/`base.modules`/`base.selftests` из одного префикса; `build/pothos-env.sh` для отладки; в тулкитах `CMP0144` NEW + `NO_PACKAGE_ROOT_PATH`; CI: Linux (Conan) + `linux-system` (Arch) | `2026-10-28-phase11-pothosbase.md` |
| 12 | widgets на ядре pothos2 (решения пользователя, 2026-10-29) | форк `markvin/PothosWidgets` (ветка `pothos2`): сборка против установленного ядра (CMake 3.25, `CMP0144` NEW), только Qt 6 (≥ 6.5) из системы или установщика Qt, не из Conan; без Qt 6 configure останавливается с понятной ошибкой; install RPATH на каталог Qt вне системных каталогов; смоук-тест 11 виджетов на платформе `offscreen` (без дисплея); CI: 4 job'а (Conan для ядра + `install-qt-action`) + `linux-system` (Arch, `qt6-base`); widgets — сабмодуль PothosBase | `2026-10-29-phase12-widgets.md` |
| 13 | soapy на ядре pothos2 (решения пользователя, 2026-10-28 и 2026-10-29) | форк `markvin/PothosSoapy` (ветка `pothos2`): сборка против установленного ядра (CMake 3.25, `CMP0144` NEW), без Poco (лог SoapySDR — в логгер Pothos); SoapySDR ≥ 0.8 из системы или из исходников upstream, не из Conan; без SoapySDR configure останавливается с понятной ошибкой; с SoapySDR < 0.9 цели soapy игнорируют `#warning` о `<ciso646>` (GCC ≥ 15); install RPATH на каталог SoapySDR вне системных каталогов; самотесты без железа через свой тестовый драйвер SoapySDR (`pothos_soapy_test`, регистрируется только внутри теста); ручной тест `soapy.device` с реальным устройством; CI: 4 job'а (Conan + SoapySDR `soapy-sdr-0.9.0` из исходников, `actions/cache`) + `linux-system` (Arch, `soapysdr` 0.8.1); soapy — сабмодуль PothosBase | `2026-10-29-phase13-soapy.md` |

Порядок 2 ↔ 3 можно поменять. **Решение пользователя (2026-10-26): этап 3
выполняется раньше этапа 2.**

## Статус

- Этап 1 — выполнен (ветка `pothos2`, CI зелёный на Linux GCC/Clang,
  Windows MSVC, macOS (Intel-раннер)).
- Этап 3 — выполнен и слит в `pothos2` (PR #1, merge `e749d7c5`): Poco
  остаётся в 10 файлах (только Net и Util), `cmake/poco-allowlist.txt`.
- Журналы исполнения (все решения «Ruling:» и отложенные мелкие замечания):
  `docs/superpowers/ledgers/`.
- Этап 2 — выполнен и слит в `pothos2` (PR #2, merge `bd369e9b`): собственный
  вычислитель `lib/Util/Expr/`, muparserx удалён.
- Этап 4 — выполнен и слит в `pothos2` (PR #3, merge `34ea844e`): сетевой слой
  `lib/Net/` на standalone Asio (`third_party/asio`), Remote и syslog без Poco,
  Poco Net не собирается.
- Этап 5 — выполнен и слит в `pothos2` (PR #4, merge `d3ecdda5`): Poco удалён
  полностью; PothosUtil на собственной `pothos_app` (`lib/App/`).
- Этап 5a — выполнен на ветке `phase5a-xsimd` (план `2026-10-27-phase5a-xsimd.md`):
  xsimd 14.3.0 в `third_party/xsimd`, определение CPU и `SIMDConvert` на xsimd,
  свой диспетчер, сабмодуль libsimdpp удалён — у ядра нет сабмодулей.
  PR в `pothos2` — после ревью.
- Этап 5a — слит в `pothos2` (PR #6, merge `f3db2df8`).
- Этап 6 (решения пользователя, 2026-10-27): делится на **6a** — CMake-гигиена
  для подключения ядра подпроектом (план `2026-10-27-phase6a-cmake-subproject.md`)
  и **6b** — публичный API «умеренно» (самописное → стандартное без смены
  абстракций; `std::span` дополнительными методами; сигнатуры, которыми
  пользуются блоки, не ломаются). Пространство имён `Pothos::` сохраняется.
- Этап 6a — выполнен на ветке `phase6-api-cmake`: ядро работает как CMake-подпроект
  (опции `POTHOS_*`, глобальное — только на верхнем уровне, тест `pothos.subproject`).
  Слит в `pothos2` (PR #7, merge `e9ebc24d`).
- Этап 6b — выполнен на ветке `phase6b-public-api` (план
  `2026-10-27-phase6b-public-api.md`): стандартные блокировки (`SpinLockRW`
  удалён, `SpinLock` ждёт), алиасы `std::` в `Util/Templates.hpp`, `using`
  вместо `typedef`, `BufferChunk::asSpan`, строки в `POTHOS_TEST_*`,
  ядро без предупреждений (`POTHOS_WARNINGS_AS_ERRORS` в Linux CI), тест
  `pothos.api_compat`. Этап 6 выполнен; следующий — этап 7 (чистка репозитория).
  Известное вне этапа: гонки TSan в `ThreadEnvironment::_waitModeEnabled`
  (обычный `bool`) и в тестах `TestTopology.cpp`.
- Этап 6b — слит в `pothos2` (PR #8, merge `0d8e51e1`).
- Этап 7 (решения пользователя, 2026-10-27: только удалить сабмодули; copier и
  network-блоки остаются в PothosBlocks — понятная ошибка и документация;
  `debian/` удалить) — выполнен на ветке `phase7-repo-cleanup` (план
  `2026-10-27-phase7-repo-cleanup.md`): в репозитории только ядро, сабмодулей
  нет, `find_package(Pothos)` находит ядро-подпроект (redirect в
  `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`). Все этапы roadmap выполнены; дальше —
  перенос тулкитов на API pothos2 в их репозиториях.
- Этап 8 (решение пользователя, 2026-10-27: не дублировать `third_party` в
  тулкитах, blocks — в дерево ядра) — выполнен на ветке
  `phase8-blocks-in-core` (план `2026-10-27-phase8-blocks-in-core.md`).
  Попутно исправлены: safe load модулей (SIGPIPE), гонка чтения/записи
  `TcpStream` в удалённых вызовах, `XSIMDTraits` под xsimd 14, генератор
  SIMD-диспетчеров. Из прочих тулкитов xsimd нужен только Comms.
- Этап 5a (решение пользователя, 2026-10-27): libsimdpp заменяется на xsimd
  (копия в `third_party/xsimd`, не Highway — тулкиты уже используют xsimd).

- Этап 9 (решения пользователя, 2026-10-28) — выполнен на ветке
  `phase9-conan-deps` (спецификация `2026-10-28-phase9-conan-deps-design.md`,
  план `2026-10-28-phase9-conan-deps.md`): этап 8 отменён, blocks — форк
  `markvin/PothosBlocks` (ветка `pothos2`, история upstream `4aef23f` + коммиты
  этапов 8–9); `third_party/` удалён, зависимости через `find_package`
  (система или Conan 2). CI зелёный: ядро 5/5, blocks 4/4. Дальше: comms
  по той же схеме (этап 10).

- Этап 10 (решения пользователя, 2026-10-28) — выполнен в форке
  `markvin/PothosComms` (спецификация `2026-10-28-phase10-comms-design.md`,
  план `2026-10-28-phase10-comms.md`, PR #1 `phase10-comms` → `pothos2`):
  comms собирается против установленного ядра без Poco и без spuce. Своя
  библиотека расчёта фильтров `filter/design/` даёт те же отводы, что spuce
  `ad08df7` (эталонные данные, генератор в `filter/design/tools/`). Изменения
  поведения: `IIRFilter.setTaps` с 1 отводом и `IIRDesigner` с неизвестным
  типом или полосой дают исключение; границы Remez, где у spuce было UB, —
  ошибку. CI зелёный: 5/5, по 60 самотестов. Ядро не менялось.

- Этап 11 (решения пользователя, 2026-10-28) — выполнен в репозитории
  `markvin/PothosBase` (спецификация `2026-10-28-phase11-pothosbase-design.md`,
  план `2026-10-28-phase11-pothosbase.md`, PR #1 `add-comms` → `main`): core,
  blocks и comms — сабмодули на концах `pothos2`, одно дерево CMake. В blocks
  (PR #1) и comms (PR #2) `CMP0144` NEW и `find_package(Pothos ...
  NO_PACKAGE_ROOT_PATH)`: тулкит не ищет ядро по `POTHOS_ROOT`. CI зелёный:
  2/2 (Conan и Arch), 316 самотестов из одного префикса, 0 предупреждений.
  Ядро не менялось. Дальше: следующие тулкиты по одному (widgets → soapy →
  python → plotters → flow), каждый — в PothosBase после переноса.

- Этап 12 (решения пользователя, 2026-10-29) — выполнен в форке
  `markvin/PothosWidgets` (спецификация `2026-10-29-phase12-widgets-design.md`,
  план `2026-10-29-phase12-widgets.md`, PR #1 `phase12-widgets` → `pothos2`)
  и в PothosBase (PR #2 `add-widgets` → `main`). Только Qt 6 (≥ 6.5); исправление
  Qt 6 в `indicator.cpp` (цифры в списке вместо `findChildren`). Смоук-тест
  11 виджетов на `offscreen` падает на предупреждениях Qt о неудачном вызове
  (`QMetaObject::invokeMethod`, `Cannot queue arguments`). CI widgets
  зелёный: 5/5, по 13 самотестов; Qt 6.5.3 на Linux и macOS, Qt 6.8.3 на
  Windows (заголовки Qt 6.5.3 не собираются с текущим MSVC, решение
  пользователя). PothosBase: 329 самотестов из одного префикса. Ядро не
  менялось. Дальше: soapy.

- Этап 13 (решения пользователя, 2026-10-28 и 2026-10-29) — выполнен в форке
  `markvin/PothosSoapy` (спецификация `2026-10-29-phase13-soapy-design.md`,
  план `2026-10-29-phase13-soapy.md`, PR #1 `phase13-soapy` → `pothos2`,
  merge `94e1d7f`) и в PothosBase (PR #3 `add-soapy` → `main`). Poco убран;
  CI собирает SoapySDR `soapy-sdr-0.9.0` из upstream (в 0.9 нет `<ciso646>`),
  `linux-system` — Arch SoapySDR 0.8.1 с обходом `#warning`. Самотесты без
  железа: свой драйвер `pothos_soapy_test` в модуле `SoapySupport`
  (регистрация только на время теста, Find отвечает только на свой
  `driver=`); 11 самотестов (10 в `/soapy/tests` и
  `/soapy/device_tests/test_source`). Ручная проверка `soapy.device` на
  Ettus USRP B210 (SoapyUHD): 950 640 отсчётов за 1 с при 1 MSps (только
  приём). Исправлено при переносе: недостающие `#include` (`<exception>` в
  `SoapyBlock.hpp`, сборка с libc++ на macOS). CI soapy зелёный: 5/5.
  PothosBase: 340 самотестов из одного префикса. Ядро не менялось.
  Дальше: python.

## Решения для следующих этапов

Решения пользователя (2026-10-28), принятые до начала этих этапов:

- plotters (spuce `design_window` в Periodogram и Spectrogram): своя
  маленькая копия кода окон в plotters. Замена spuce в comms остаётся
  внутренней библиотекой comms и не устанавливается.
- soapy: SoapySDR нет в ConanCenter. CI собирает SoapySDR из исходников
  upstream `pothosware/SoapySDR` (зафиксированный тег или коммит) отдельным
  шагом до сборки тулкита и ставит его в префикс зависимостей
  (`actions/cache` по версии). Остальные зависимости — через Conan. Job
  `linux-system` берёт `extra/soapysdr`. CMake тулкита: только
  `find_package(SoapySDR CONFIG)`. Рецепт для ConanCenter — позже,
  необязательно. Локальный клон `/home/mark/Projects/SoapySDR` смотрит в
  корпоративное зеркало: CI его не использует. Выполнено в этапе 13
  (тег `soapy-sdr-0.9.0`, решение пользователя 2026-10-29).
- PothosBase — оформлен этапом 11 (см. таблицу и статус).
- PothosBase, следующие тулкиты: добавлять после их переноса, по одному
  (widgets — этап 12, soapy — этап 13; дальше python → plotters → flow). Проверки `base.modules` и
  `base.selftests` предполагают каталог модулей `/<toolkit>/` и тесты в
  `/<toolkit>/tests/`; для python (`proxy/environment`) и flow нужна
  настройка для тулкита (отложенное замечание ревью этапа 11).

## Заметки для плана этапа 2 (собраны при разведке)

- `Topology::make()` (`lib/Framework/TopologyMakeJSON.cpp:16-36`) и
  `ProxyBlockEval` (`lib/Util/Builtin/BlockEval.cpp:85`) вычисляют
  `arg.dump()` для любых не-строковых JSON-значений → синтаксис обязан быть
  надмножеством JSON: `true`/`false`/`null` в нижнем регистре, числа
  `-3`, `1e-05`, массивы, объекты с ключами-строками.
- Строковый аргумент, который не вычислился, становится литеральной строкой
  (`TopologyMakeJSON.cpp:21-31`, документировано в `Topology.hpp:~95`) →
  неизвестный идентификатор (`int32`, `hello`) **обязан** бросать исключение,
  а не давать значение.
- Пример из документации `Topology.hpp`: globals `{"name":"baz","value":"-fiz"}`
  — унарный минус над константой.
- Текущие хаки, которые уходят: `splitExpr` для списков/словарей
  (`EvalEnvironmentListParsers.cpp`), кодирование словаря в матрицу с меткой
  `__map__`, временные константы `__tmp__N`, `MupOctalValReader`.
