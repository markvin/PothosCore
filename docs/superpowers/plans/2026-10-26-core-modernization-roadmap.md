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

Порядок 2 ↔ 3 можно поменять: этап 2 независим и мал, поэтому идёт раньше.
