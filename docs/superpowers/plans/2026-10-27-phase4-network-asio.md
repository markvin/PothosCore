# Этап 4: сеть на Asio — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remote (клиент, сервер, прокси-сервер `PothosUtil`), пересылка и приём логов по syslog/UDP и `Util/Network` работают на standalone Asio; Poco Net и `Poco::URI` больше не используются, библиотека Poco Net не собирается.

**Architecture:** Внутренний модуль `lib/Net/` (namespace `Pothos::Net`, заголовки не устанавливаются) — общий `io_context` с фоновым потоком, разбор URI и адресов, блокирующий `TcpStream` (`std::iostream` поверх сокета) и `TcpServer` (асинхронный accept, поток на соединение, обработчик соединения не знает о Remote), syslog-кодек RFC 5424/3164 с UDP-отправителем и слушателем. Remote сохраняет протокол (`Archive` поверх `std::iostream`), меняется только транспорт. Asio — git-сабмодуль, подключается как PRIVATE INTERFACE-цель `pothos_asio`.

**Tech Stack:** C++23, standalone Asio 1.38.2 (`ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`), CMake/CTest, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (решения про Remote, syslog, будущий web-UI, анти-цели), roadmap `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`.

## Global Constraints

- C++23; Linux GCC/Clang, macOS, Windows MSVC — все четыре CI-job зелёные; Remote-тесты (`/proxy/remote/tests`) включены на всех платформах.
- Asio не виден в `include/Pothos/**` и не требуется потребителю через `PothosConfig.cmake` (тест `pothos.consumer`: регулярка дополняется `asio`).
- Сборка не скачивает ничего, кроме `git submodule update`; системные библиотеки — только ОС (на Windows `ws2_32`, `mswsock`).
- «Пересылка логов по UDP остаётся совместимой с syslog (сообщения читаются rsyslog/journald; listener принимает стандартный syslog)» (спека).
- «Внутренний сетевой слой на Asio проектируется так, чтобы поверх него можно было добавить HTTP/WebSocket-сервер без переделки (общий io_context/потоки, абстракция соединения, не завязанная на протокол Remote)» (спека). HTTP/WebSocket не реализуется.
- Протокол Remote менять можно, но не нужно: формат `Archive` и публичные `RemoteClient`/`RemoteServer`/`RemoteHandler`/`Util/Network.hpp` не меняются.
- Храповик Poco: строка `cmake/poco-allowlist.txt` удаляется, как только файл перестаёт использовать Poco; новых строк нет.
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS. Одиночный самотест — команда из `CLAUDE.md` с `-DTEST_PATH=...`.
- Push в `origin` — только с согласия пользователя.

## Решения (на утверждение)

1. **Asio — сабмодуль** `asio` в корне (как `poco`, `libsimdpp`), тег `asio-1-38-2`; заголовки из `asio/asio/include`. Альтернатива — копия в `third_party/` (~500 файлов в истории).
2. **Модель исполнения:** один общий `io_context` на процесс с одним фоновым потоком (`Net::ioContext()`), запуск лениво, остановка при выгрузке библиотеки. На нём — accept, приём syslog, тайм-ауты connect/resolve. Чтение/запись Remote — блокирующие, поток на соединение (как у Poco `TCPServer`): `RemoteHandler` синхронный.
3. **Формат syslog (отправка):** RFC 5424 поверх UDP, facility `user`:
   `<PRI>1 2026-10-27T12:00:00.123456Z HOST Pothos PID - [pothos@32473 source="..." level="trace" tid="..."] TEXT`.
   Источник и точный уровень Pothos — в structured data (MSGID/APP-NAME не допускают пробелов); `32473` — номер предприятия для документации (RFC 5612). Уровни → severity: fatal→alert(1), critical→crit(2), error→err(3), warning→4, notice→5, information→info(6), debug и trace→debug(7). Сообщение длиннее 60000 байт обрезается.
4. **Приём syslog:** RFC 5424 (с `pothos@32473` — точные source/level/pid/tid; без него — source = APP-NAME) и RFC 3164 (`<PRI>Mmm dd hh:mm:ss HOST TAG[PID]: TEXT`; source = TAG). Повреждённые датаграммы игнорируются. Слушатель — dual-stack (`::`, `v6_only=false`), при отсутствии IPv6 — `0.0.0.0` (закрывает старый FIXME «always binds to IPv4»).
5. **Адрес syslog** — `host[:port]` (IPv6 в скобках), порт по умолчанию 514; `startSyslogListener()` по-прежнему возвращает номер порта строкой.

## Review Focus

1. **Wildcard-сервер и IPv4-клиент:** сервер на `tcp://[::]` принимает соединение на `127.0.0.1`; на машине без IPv6 всё работает через `0.0.0.0`. Тест `test_net_tcp_dual_stack` (Task 2).
2. **Обрыв соединения посреди сообщения:** клиент закрывает сокет во время запроса — поток обработчика завершается, сервер обслуживает остальных, `--require-active` завершает процесс после закрытия последнего соединения. Тесты `test_net_tcp_server_peer_abort` (Task 2) и `test_process_self_termination` (Task 3).
3. **Тайм-ауты:** connect к непринимающему адресу и resolve несуществующего имени не висят дольше тайм-аута (+1 с). Тесты `test_net_connect_timeout` (Task 2), `test_net_resolve` (Task 1).
4. **Чужой и испорченный syslog:** строки rsyslog/journald (RFC 3164 и 5424 без наших SD), пустые, обрезанные, с `]`/`"`/`\` внутри SD, не-UTF-8 — без падений; неразбираемое игнорируется. Тест `test_net_syslog_parse_foreign` (Task 4).
5. **Остановка:** `stopSyslogListener` → повторный `startSyslogListener`; выход процесса с активными сервером/слушателем/пересылкой не зависает; деструктор `TcpServer` при открытых соединениях. Тесты `test_net_tcp_server_shutdown` (Task 2), `test_logger_syslog_restart` (Task 4).

---

### Task 1: Asio в дереве, URI и адреса

**Files:**
- Create: `.gitmodules` запись `asio` (URL `https://github.com/chriskohlhoff/asio.git`, тег `asio-1-38-2`); `lib/Net/Uri.hpp`, `lib/Net/Uri.cpp`, `lib/Net/Address.hpp`, `lib/Net/Address.cpp`, `lib/Net/IoContext.hpp`, `lib/Net/IoContext.cpp`, `lib/Net/Builtin/TestNetAddress.cpp`
- Modify: `third_party/CMakeLists.txt` (цель `pothos_asio`), `lib/CMakeLists.txt` (исходники `Net/`, `pothos_asio` PRIVATE), `lib/Util/Network.cpp`, `lib/Framework/TopologyNetworkFlows.cpp`, `cmake/poco-allowlist.txt` (−2 строки), `tests/consumer/CMakeLists.txt` (регулярка `Poco|nlohmann|simdpp|asio`), `.github/workflows/ci.yml` и `README.md`, `CLAUDE.md` (`git submodule update --init poco libsimdpp asio`)

**Interfaces (namespace `Pothos::Net`):**
- `pothos_asio`: INTERFACE, `SYSTEM` include `asio/asio/include`, определения `ASIO_STANDALONE ASIO_NO_DEPRECATED`, на Windows `_WIN32_WINNT=0x0A00` и `ws2_32 mswsock`; FATAL_ERROR, если сабмодуль не инициализирован.
- `struct Uri { std::string scheme; std::string host; std::uint16_t port = 0; };` — `host` без скобок, `port == 0` — не задан.
- `Uri parseUri(std::string_view s);` — `scheme://host[:port][/]`; IPv6 только в скобках; ошибка — `Pothos::InvalidArgumentException` с исходной строкой.
- `std::string toString(const Uri &uri);` — скобки для хоста с `:`, порт только если не 0.
- `struct HostPort { std::string host; std::uint16_t port; };` `HostPort parseHostPort(std::string_view s, std::uint16_t defaultPort);` `std::string formatHostPort(std::string_view host, std::uint16_t port);`
- `bool isWildcard(std::string_view host);` (`""`, `0.0.0.0`, `::`) и `std::string loopbackFor(std::string_view wildcardHost);` (`::`→`::1`, иначе `127.0.0.1`).
- `asio::io_context &ioContext();` — общий контекст, фоновый поток с work guard; поток останавливается и присоединяется статическим guard-объектом при выгрузке.
- `bool ipv6Supported();` — UDP-сокет на `::` открывается и привязывается.
- `asio::ip::address resolveHost(const std::string &host, std::chrono::microseconds timeout);` — асинхронный resolve на `ioContext()`, ожидание с тайм-аутом; `Pothos::TimeoutException` / `Pothos::IOException` (текст ошибки ОС).

- [ ] **Step 1: Сабмодуль и цель CMake:** `git submodule add https://github.com/chriskohlhoff/asio.git asio`, checkout тега; `pothos_asio` в `third_party/CMakeLists.txt`; CI, README, CLAUDE.md, регулярка consumer.
- [ ] **Step 2: Тесты `TestNetAddress.cpp`** (`/net/tests`):
  - `test_net_uri`: `tcp://localhost:16415` → `{tcp, localhost, 16415}`; `tcp://[::1]:80` → host `::1`; `tcp://[::]` → port 0; `toString({tcp,"::1",80}) == "tcp://[::1]:80"`; `toString({tcp,"10.0.0.1",0}) == "tcp://10.0.0.1"`; ошибки: `localhost:1`, `tcp://`, `tcp://::1:80`, `tcp://h:70000`, `tcp://h:x`, `tcp://h:1/path`.
  - `test_net_host_port`: `parseHostPort("[::1]:514", 1)` → `{::1, 514}`; `"logs.local"` → порт по умолчанию; `"1.2.3.4:9"`; ошибки как у URI; `formatHostPort("::1", 5) == "[::1]:5"`.
  - `test_net_resolve`: `resolveHost("127.0.0.1", 1s)` — loopback; `"localhost"` — loopback (v4 или v6); `"no-such-host.invalid"` → `Pothos::Exception` не позже чем через тайм-аут + 1 с.
  - `test_net_wildcard_addr`: `Pothos::Util::getWildcardAddr("1")` — `[::]:1` или `0.0.0.0:1` в зависимости от `ipv6Supported()`.
- [ ] **Step 3: Run** → FAIL (нет `lib/Net`).
- [ ] **Step 4: Реализовать** `Uri`, `Address`, `IoContext`; перевести `Util/Network.cpp` на `ipv6Supported()` и `TopologyNetworkFlows.cpp` на `Uri`/`toString`; убрать обе строки из allowlist.
- [ ] **Step 5: Run** самотесты `/net/tests/*` и полный `ctest` → PASS.
- [ ] **Step 6: Commit** `feat(net): vendored Asio, URI and address helpers, shared io_context`

---

### Task 2: `TcpStream` и `TcpServer`

**Files:**
- Create: `lib/Net/TcpStream.hpp`, `lib/Net/TcpStream.cpp`, `lib/Net/TcpServer.hpp`, `lib/Net/TcpServer.cpp`, `lib/Net/Builtin/TestNetTcp.cpp`
- Modify: `lib/CMakeLists.txt`

**Interfaces:**
- Consumes: `ioContext()`, `resolveHost`, `HostPort` (Task 1).
- `class TcpStream : public std::iostream` — буферы 64 KiB в каждую сторону, `TCP_NODELAY`; ошибка ввода-вывода → `badbit`, как у файловых потоков (никаких исключений из `<<`/`>>`).
  - `static std::unique_ptr<TcpStream> connect(const std::string &host, std::uint16_t port, std::chrono::microseconds timeout);` — resolve + connect к первому доступному адресу; тайм-аут на всё; `Pothos::TimeoutException`/`Pothos::IOException`.
  - `std::string peerHost() const;` (без скобок и порта), `void close();` — потокобезопасно прерывает блокирующее чтение в другом потоке.
- `class TcpServer` — абстракция соединения без знания о Remote:
  - `using Handler = std::function<void(TcpStream &)>;`
  - `TcpServer(const std::string &host, std::uint16_t port, Handler handler, std::function<void(std::size_t active)> onCountChange = {});` — bind (для `::` — `v6_only=false`), listen, асинхронный accept на `ioContext()`; каждое соединение — свой поток, вызывающий `handler`; `onCountChange` вызывается после каждого открытия/закрытия.
  - `std::string host() const;` `std::uint16_t port() const;` (фактический порт при `port == 0`).
  - `~TcpServer()` — прекращает accept, закрывает открытые соединения, присоединяет их потоки.

- [ ] **Step 1: Тесты `TestNetTcp.cpp`** (`/net/tests`):
  - `test_net_tcp_echo`: сервер на `127.0.0.1:0` с эхо-обработчиком (`getline` → ответ); клиент шлёт 3 строки и читает их; `peerHost()` у обработчика — `127.0.0.1`.
  - `test_net_tcp_large_payload`: 16 MiB случайных байтов в обе стороны (обработчик возвращает сумму и длину) — совпадение.
  - `test_net_tcp_dual_stack`: сервер на `::` (если `ipv6Supported()`, иначе `0.0.0.0`), клиенты на `127.0.0.1` и (при IPv6) `::1` соединяются.
  - `test_net_tcp_concurrent`: 16 клиентов параллельно по 100 запросов; `onCountChange` видит максимум ≥ 2 и последнее значение 0.
  - `test_net_tcp_server_peer_abort`: клиент пишет половину строки и закрывается — обработчик получает EOF/`badbit` и возвращается; следующий клиент обслуживается.
  - `test_net_tcp_server_shutdown`: обработчик блокирован в чтении; деструктор сервера завершается < 2 с.
  - `test_net_connect_timeout`: connect к порту, где никто не слушает (`127.0.0.1:<закрытый>`), → `Pothos::IOException` быстро; к `10.255.255.1:9` с тайм-аутом 200 мс → исключение не позже чем через 1.2 с.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать** (`std::streambuf` над `asio::ip::tcp::socket`: `underflow` — `read_some`, `overflow`/`sync` — `write`; connect — `async_connect` с ожиданием `std::future` и отменой по тайм-ауту).
- [ ] **Step 4: Run** самотесты `/net/tests/test_net_tcp_*` и полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(net): blocking TCP iostream and thread-per-connection TCP server`

---

### Task 3: Remote на `lib/Net`

**Files:**
- Modify: `lib/Remote/Client.cpp`, `lib/Remote/Server.cpp` (URI и переписывание wildcard-адреса; syslog — в Task 4), `apps/PothosUtilProxyServer.cpp`, `lib/Platform/Process.hpp`, `lib/Platform/ProcessUnix.cpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/Builtin/TestProcess.cpp`, `cmake/poco-allowlist.txt` (−`lib/Remote/Client.cpp`), `lib/Remote/Builtin/TestRemote.cpp`

**Interfaces:**
- Consumes: `parseUri`, `toString`, `isWildcard`, `loopbackFor`, `TcpStream::connect`, `TcpServer` (Tasks 1–2).
- `void Pothos::Platform::requestSelfTermination(void);` — Unix: `kill(getpid(), SIGTERM)`; Windows: `SetEvent` на событии `terminationEventName(GetCurrentProcessId())`, иначе `std::exit(1)`. Нужна прокси-серверу до этапа 5 (`waitForTerminationRequest` остаётся в Poco::Util).
- `RemoteClient`: `getIoStream()` возвращает `TcpStream`; таблица node id хранит адрес строкой; порт по умолчанию — `getLocatorPort()`; ошибки — `RemoteClientError` с URI.
- `PothosUtilProxyServer`: `TcpServer(host, port, handler, onCountChange)`, печать `Host: …`/`Port: …` как сейчас (их читает `RemoteServer`); `--require-active` → `requestSelfTermination()` при счётчике 0; `Poco::Net` и `Poco::Process` из файла уходят (остаётся `Poco::Util` — этап 5).

- [ ] **Step 1: Тесты:**
  - `TestProcess.cpp`: `test_process_self_termination` — дочерний `PothosUtil --proxy-server=tcp://127.0.0.1:0 --require-active`: клиент соединяется и отключается → процесс завершается сам в течение 5 с.
  - `TestRemote.cpp` (существующие `test_inception`, `test_remote`, `test_server`, `test_multithread_safe` сохраняются) + `test_remote_client_errors`: `RemoteClient("udp://localhost")`, `RemoteClient("tcp://[::1")` → `RemoteClientError`; `RemoteClient("tcp://127.0.0.1:<закрытый порт>")` → `RemoteClientError` быстро; `test_remote_ipv4_to_wildcard`: `RemoteServer("tcp://"+getWildcardAddr())`, клиент на `tcp://127.0.0.1:<port>` работает.
- [ ] **Step 2: Run** → FAIL (новые тесты).
- [ ] **Step 3: Реализовать**; убрать `lib/Remote/Client.cpp` из allowlist.
- [ ] **Step 4: Run** `/proxy/remote/tests/*`, `/platform/tests/test_process_self_termination` и полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(remote): client, server and proxy server on the Asio network layer`

---

### Task 4: syslog на Asio

**Files:**
- Create: `lib/Net/Syslog.hpp`, `lib/Net/Syslog.cpp`, `lib/Net/Builtin/TestNetSyslog.cpp`
- Modify: `lib/System/Logger.cpp` (листенер и пересылка), `lib/Remote/Server.cpp` (`startSyslogForwarding`), `lib/System/Builtin/TestLogger.cpp`, `cmake/poco-allowlist.txt` (−`lib/System/Logger.cpp`, −`lib/Remote/Server.cpp`), `lib/CMakeLists.txt`

**Interfaces:**
- Consumes: `ioContext()`, `parseHostPort`, `resolveHost`, `ipv6Supported()` (Task 1); `System::Detail::LogRecord`, `deliver`, `setLogSink`, `guessLevel` (`lib/System/LoggerSinks.hpp`).
- `std::string formatSyslog(const System::Detail::LogRecord &r, std::string_view hostname);` — по решению 3.
- `std::optional<System::Detail::LogRecord> parseSyslog(std::string_view datagram);` — по решению 4; время без зоны (3164) — локальное текущего года.
- `class SyslogSender { explicit SyslogSender(const std::string &addr); void send(const System::Detail::LogRecord &r); };` — адрес по решению 5, resolve один раз, ошибки отправки игнорируются (как у Poco).
- `class SyslogListener { explicit SyslogListener(std::function<void(System::Detail::LogRecord)> onRecord); std::uint16_t port() const; ~SyslogListener(); };` — по решению 4, порт 0 (свободный), приём на `ioContext()`.

- [ ] **Step 1: Тесты:**
  - `test_net_syslog_roundtrip`: запись (уровни trace…fatal, source `"My Source"`, текст с пробелами, кавычками, `]`, `\`, UTF-8, `\n`) → `formatSyslog` → `parseSyslog` → те же level/source/text/pid/tid, время с точностью до микросекунд.
  - `test_net_syslog_format`: строка начинается с `<12>1 ` для warning, содержит `[pothos@32473 source="My Source" level="warning"`; длина ≤ 60000 + заголовок для текста 100000 байт.
  - `test_net_syslog_parse_foreign`: `<34>Oct 11 22:14:15 mymachine su[123]: 'su root' failed` → critical, source `su`, pid 123; `<165>1 2003-10-11T22:14:15.003Z host app 77 ID47 - hello` → notice, source `app`; `<13>1 - - - - - -` → пустой текст; мусор (`""`, `"<"`, `"<999>x"`, `"<13>1 2003"`, SD без `]`) → `nullopt` без падения.
  - `test_net_syslog_udp`: `SyslogListener` + `SyslogSender("127.0.0.1:<port>")` и (при IPv6) `"[::1]:<port>"` → запись доходит за 2 с.
  - `TestLogger.cpp`: `test_logger_syslog_restart` — `startSyslogListener` → `startSyslogForwarding("127.0.0.1:"+port)` → запись видна в перехвачивающем приёмнике; `stopSyslogListener`, повторный старт — новый порт работает.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать**; `Logger.cpp` и `Server.cpp` без Poco; обе строки из allowlist.
- [ ] **Step 4: Run** `/net/tests/test_net_syslog_*`, `/system/tests/test_logger_*`, `/proxy/remote/tests/*`, полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(net): RFC 5424 syslog forwarding and listener on Asio`

---

### Task 5: Poco Net выключен

**Files:**
- Modify: `CMakeLists.txt` (`ENABLE_NET OFF`, `install(TARGETS Foundation Util XML)`), `lib/CMakeLists.txt` и `apps/CMakeLists.txt` (без `PocoNet`), `README.md`, `CLAUDE.md` (раздел про `lib/Net/`: модуль, `ioContext`, правило «Asio только в `lib/Net` и `.cpp`, не в `include/`»), `Changelog.txt` (pothos2: сеть на Asio, формат syslog RFC 5424, слушатель dual-stack)

- [ ] **Step 1: RED:** `grep -rn "Poco/Net\|Poco::Net\|Poco::URI\|Poco/URI" lib apps include` → найденное (зафиксировать число; ожидается 0 после Tasks 1–4, иначе — что осталось).
- [ ] **Step 2: Выключить Net** в сборке Poco и убрать линковку.
- [ ] **Step 3: Чистая конфигурация** `build-clean` (GCC) и `build-clang` (Clang) → полный `ctest` PASS; без новых предупреждений в `lib/Net/`; удалить каталоги.
- [ ] **Step 4: Commit** `build: drop Poco Net, the network layer is Asio`

---

### Task 6: Итог этапа

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (этап 4 выполнен, следующий — этап 5), журнал `docs/superpowers/ledgers/2026-10-27-phase4-network-asio.md`

- [ ] **Step 1: Обновить roadmap и журнал. Commit** `docs: phase 4 complete — network on Asio`
- [ ] **Step 2: Свежее ревью всей ветки** (один ревьюер, фокус — Review Focus), исправления отдельными коммитами с тестами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase4-network-asio`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- `Poco::Util` (`PothosUtil.cpp/.hpp`, `PothosUtilProxyServer.cpp` — только `waitForTerminationRequest`, `ConfLoader`, `ModuleSafeLoad`) — этап 5.
- HTTP/WebSocket-сервер — будущая цель; `TcpServer`/`ioContext()` — точка расширения.
- Сетевые блоки (`blocks/network`) вне сборки ядра; используют свой код.
