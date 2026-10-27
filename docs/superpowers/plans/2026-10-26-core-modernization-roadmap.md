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
| 5 | PothosUtil без Poco::Util и удаление Poco | свой разбор аргументов, завершение по сигналам, `--daemon`/`--pidfile`/`--umask`, служба Windows; сабмодуль poco удалён | пишется после этапа 4 |
| 6 | Модернизация публичного API | проход по `include/Pothos/**` (std::span/optional/стандартные примитивы), чистка предупреждений | пишется после этапа 5 |

Порядок 2 ↔ 3 можно поменять. **Решение пользователя (2026-10-26): этап 3
выполняется раньше этапа 2.**

## Статус

- Этап 1 — выполнен (ветка `pothos2`, CI зелёный на Linux GCC/Clang,
  Windows MSVC, macOS (Intel-раннер)).
- Этап 3 — выполнен и слит в `pothos2` (PR #1, merge `e749d7c5`): Poco
  остаётся в 10 файлах (только Net и Util), `cmake/poco-allowlist.txt`.
- Журналы исполнения (все решения «Ruling:» и отложенные мелкие замечания):
  `docs/superpowers/ledgers/`.
- Этап 2 — выполнен на ветке `phase2-expression-evaluator` (план
  `2026-10-27-phase2-expression-evaluator.md`): собственный вычислитель
  `lib/Util/Expr/`, muparserx удалён. PR в `pothos2` — после ревью.
- Следующий — этап 4 (сеть на Asio).

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
