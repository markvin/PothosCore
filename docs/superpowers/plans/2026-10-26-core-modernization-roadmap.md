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
| 7 | Чистка репозитория (решение пользователя, 2026-10-27) | в PothosCore остаются только `blocks` (базовые блоки и их тестирование), `apps`, `python`; остальные сабмодули и их каталоги удаляются (ядро самодостаточно); `flow`, `widgets`, `soapy`, `plotters`, `comms`, `audio` переезжают в отдельные репозитории; README.md и Changelog.txt корректируются. `blocks` и `python` переезжают в дерево ядра и входят в его сборку (решение пользователя, 2026-10-27) | пишется после этапа 6 |

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
- Этап 5 — план `2026-10-27-phase5-remove-poco.md`, ветка
  `phase5-pothosutil-without-poco`.
- Этап 5a (решение пользователя, 2026-10-27): libsimdpp заменяется на xsimd
  (копия в `third_party/xsimd`, не Highway — тулкиты уже используют xsimd).

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
