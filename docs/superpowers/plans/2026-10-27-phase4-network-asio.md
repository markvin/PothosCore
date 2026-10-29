# Phase 4: network on Asio — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remote (the client, the server, the proxy server of `PothosUtil`), the forwarding and reception of logs over syslog/UDP, and `Util/Network` run on standalone Asio. Poco Net and `Poco::URI` are not used any more, and the build does not build the Poco Net library.

**Architecture:** An internal module `lib/Net/` (namespace `Pothos::Net`, the headers are not installed). It has these parts: a shared `io_context` with a background thread; the parse of URIs and addresses; a blocking `TcpStream` (`std::iostream` over a socket); a `TcpServer` (asynchronous accept, one thread for each connection, the connection handler does not know about Remote); and a syslog codec RFC 5424/3164 with a UDP sender and a listener. Remote keeps the protocol (`Archive` over `std::iostream`). Only the transport changes. Asio is a copy of the headers in `third_party/asio` (decision of the user). It is connected as the PRIVATE INTERFACE target `pothos_asio`.

**Tech Stack:** C++23, standalone Asio 1.38.2 (`ASIO_STANDALONE`, `ASIO_NO_DEPRECATED`), CMake/CTest, the self-tests `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (the decisions about Remote, syslog, the future web UI, the anti-goals), roadmap `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`.

## Global Constraints

- C++23. Linux GCC/Clang, macOS, Windows MSVC: all four CI jobs are green. The Remote tests (`/proxy/remote/tests`) are on for all platforms.
- Asio is not visible in `include/Pothos/**`. The consumer does not need it through `PothosConfig.cmake` (the test `pothos.consumer`: add `asio` to the regex).
- The build downloads nothing (Asio is in the tree). The system libraries are only those of the OS (on Windows `ws2_32`, `mswsock`).
- "The forwarding of logs over UDP stays compatible with syslog (rsyslog/journald read the messages; the listener accepts standard syslog)" (spec).
- "The internal network layer on Asio is designed so that you can add an HTTP/WebSocket server on top of it with no rework (a shared io_context/threads, a connection abstraction that does not depend on the Remote protocol)" (spec). The phase does not implement HTTP/WebSocket.
- You can change the Remote protocol, but you do not need to: the format of `Archive` and the public `RemoteClient`/`RemoteServer`/`RemoteHandler`/`Util/Network.hpp` do not change.
- The Poco ratchet: delete a line of `cmake/poco-allowlist.txt` as soon as the file does not use Poco any more. There are no new lines.
- The check of each task: `cmake --build build && ctest --test-dir build --output-on-failure` → all PASS. One self-test — the command from `CLAUDE.md` with `-DTEST_PATH=...`.
- Push to `origin` only with the consent of the user.

## Decisions (for approval)

1. **Asio is a copy in `third_party/asio`** (decision of the user, 2026-10-27): `include/` and `LICENSE_1_0.txt` from the tag `asio-1-38-2`. The version and the source are in `third_party/asio/README.md`.
2. **Execution model:** One shared `io_context` for each process with one background thread (`Net::ioContext()`). It starts lazily and stops at the unload of the library. It runs the accept, the syslog reception, and the timeouts of connect/resolve. The read and write of Remote are blocking, with one thread for each connection (as the Poco `TCPServer`): `RemoteHandler` is synchronous.
3. **Syslog format (send):** RFC 5424 over UDP, facility `user`:
   `<PRI>1 2026-10-27T12:00:00.123456Z HOST Pothos PID - [pothos@32473 source="..." level="trace" tid="..."] TEXT`.
   The source and the exact Pothos level are in the structured data (MSGID/APP-NAME do not allow spaces). `32473` is the enterprise number for documentation (RFC 5612). Levels → severity: fatal→alert(1), critical→crit(2), error→err(3), warning→4, notice→5, information→info(6), debug and trace→debug(7). A message longer than 60000 bytes is cut.
4. **Syslog reception:** RFC 5424 (with `pothos@32473` — the exact source/level/pid/tid; without it, source = APP-NAME) and RFC 3164 (`<PRI>Mmm dd hh:mm:ss HOST TAG[PID]: TEXT`; source = TAG). The listener ignores damaged datagrams. The listener is dual-stack (`::`, `v6_only=false`). If IPv6 is not available, it uses `0.0.0.0` (this closes the old FIXME "always binds to IPv4").
5. **Syslog address** — `host[:port]` (IPv6 in brackets), the default port is 514. `startSyslogListener()` still returns the port number as a string.

## Review Focus

1. **A wildcard server and an IPv4 client:** A server on `tcp://[::]` accepts a connection on `127.0.0.1`. On a machine without IPv6 everything works through `0.0.0.0`. The test `test_net_tcp_dual_stack` (Task 2).
2. **A connection break in the middle of a message:** The client closes the socket during a request. The handler thread ends, the server serves the other clients, and `--require-active` ends the process after the last connection closes. The tests `test_net_tcp_server_peer_abort` (Task 2) and `test_process_self_termination` (Task 3).
3. **Timeouts:** A connect to an address that does not accept, and a resolve of a name that does not exist, do not hang longer than the timeout (+1 s). The tests `test_net_connect_timeout` (Task 2), `test_net_resolve` (Task 1).
4. **Foreign and damaged syslog:** The lines of rsyslog/journald (RFC 3164 and 5424 without our SD), empty and cut lines, lines with `]`/`"`/`\` inside the SD, and non-UTF-8 lines give no crash. The code ignores what it cannot parse. The test `test_net_syslog_parse_foreign` (Task 4).
5. **Stop:** `stopSyslogListener` → a second `startSyslogListener`. The exit of a process with an active server/listener/forwarding does not hang. The destructor of `TcpServer` with open connections. The tests `test_net_tcp_server_shutdown` (Task 2), `test_logger_syslog_restart` (Task 4).

---

### Task 1: Asio in the tree, URI and addresses

**Files:**
- Create: `third_party/asio/` (`include/`, `LICENSE_1_0.txt`, `README.md` with the version), `lib/Net/Uri.hpp, `lib/Net/Uri.cpp`, `lib/Net/Address.hpp`, `lib/Net/Address.cpp`, `lib/Net/IoContext.hpp`, `lib/Net/IoContext.cpp`, `lib/Net/Builtin/TestNetAddress.cpp`
- Modify: `third_party/CMakeLists.txt` (the target `pothos_asio`), `lib/CMakeLists.txt` (the sources of `Net/`, `pothos_asio` PRIVATE), `lib/Util/Network.cpp`, `lib/Framework/TopologyNetworkFlows.cpp`, `cmake/poco-allowlist.txt` (−2 lines), `tests/consumer/CMakeLists.txt` (the regex `Poco|nlohmann|simdpp|asio`), `README.md` and `CLAUDE.md` (Asio in the list of vendored code)

**Interfaces (namespace `Pothos::Net`):**
- `pothos_asio`: INTERFACE, `SYSTEM` include `third_party/asio/include`, the definitions `ASIO_STANDALONE ASIO_NO_DEPRECATED`, on Windows `_WIN32_WINNT=0x0A00` and `ws2_32 mswsock`.
- `struct Uri { std::string scheme; std::string host; std::uint16_t port = 0; };` — `host` has no brackets, and `port == 0` means not set.
- `Uri parseUri(std::string_view s);` — `scheme://host[:port][/]`; IPv6 only in brackets; an error is a `Pothos::InvalidArgumentException` with the source string.
- `std::string toString(const Uri &uri);` — brackets for a host with `:`, the port only if it is not 0.
- `struct HostPort { std::string host; std::uint16_t port; };` `HostPort parseHostPort(std::string_view s, std::uint16_t defaultPort);` `std::string formatHostPort(std::string_view host, std::uint16_t port);`
- `bool isWildcard(std::string_view host);` (`""`, `0.0.0.0`, `::`) and `std::string loopbackFor(std::string_view wildcardHost);` (`::`→`::1`, otherwise `127.0.0.1`).
- `asio::io_context &ioContext();` — the shared context, a background thread with a work guard. A static guard object stops the thread and joins it at the unload.
- `bool ipv6Supported();` — a UDP socket on `::` opens and binds.
- `asio::ip::address resolveHost(const std::string &host, std::chrono::microseconds timeout);` — an asynchronous resolve on `ioContext()`, a wait with a timeout; `Pothos::TimeoutException` / `Pothos::IOException` (the text of the OS error).

- [ ] **Step 1: The copy of Asio and the CMake target:** the headers of the tag `asio-1-38-2` in `third_party/asio/include`; `pothos_asio` in `third_party/CMakeLists.txt`; README, CLAUDE.md, the regex of the consumer.
- [ ] **Step 2: The tests `TestNetAddress.cpp`** (`/net/tests`):
  - `test_net_uri`: `tcp://localhost:16415` → `{tcp, localhost, 16415}`; `tcp://[::1]:80` → host `::1`; `tcp://[::]` → port 0; `toString({tcp,"::1",80}) == "tcp://[::1]:80"`; `toString({tcp,"10.0.0.1",0}) == "tcp://10.0.0.1"`; errors: `localhost:1`, `tcp://`, `tcp://::1:80`, `tcp://h:70000`, `tcp://h:x`, `tcp://h:1/path`.
  - `test_net_host_port`: `parseHostPort("[::1]:514", 1)` → `{::1, 514}`; `"logs.local"` → the default port; `"1.2.3.4:9"`; the errors are as for the URI; `formatHostPort("::1", 5) == "[::1]:5"`.
  - `test_net_resolve`: `resolveHost("127.0.0.1", 1s)` — loopback; `"localhost"` — loopback (v4 or v6); `"no-such-host.invalid"` → `Pothos::Exception` not later than the timeout + 1 s.
  - `test_net_wildcard_addr`: `Pothos::Util::getWildcardAddr("1")` — `[::]:1` or `0.0.0.0:1` depending on `ipv6Supported()`.
- [ ] **Step 3: Run** → FAIL (there is no `lib/Net`).
- [ ] **Step 4: Implement** `Uri`, `Address`, `IoContext`. Move `Util/Network.cpp` to `ipv6Supported()` and `TopologyNetworkFlows.cpp` to `Uri`/`toString`. Remove both lines from the allowlist.
- [ ] **Step 5: Run** the self-tests `/net/tests/*` and the full `ctest` → PASS.
- [ ] **Step 6: Commit** `feat(net): vendored Asio, URI and address helpers, shared io_context`

---

### Task 2: `TcpStream` and `TcpServer`

**Files:**
- Create: `lib/Net/TcpStream.hpp`, `lib/Net/TcpStream.cpp`, `lib/Net/TcpServer.hpp`, `lib/Net/TcpServer.cpp`, `lib/Net/Builtin/TestNetTcp.cpp`
- Modify: `lib/CMakeLists.txt`

**Interfaces:**
- Consumes: `ioContext()`, `resolveHost`, `HostPort` (Task 1).
- `class TcpStream : public std::iostream` — 64 KiB buffers in each direction, `TCP_NODELAY`. An I/O error sets `badbit`, as for file streams (no exceptions from `<<`/`>>`).
  - `static std::unique_ptr<TcpStream> connect(const std::string &host, std::uint16_t port, std::chrono::microseconds timeout);` — resolve + connect to the first available address. The timeout covers everything. `Pothos::TimeoutException`/`Pothos::IOException`.
  - `std::string peerHost() const;` (no brackets and no port), `void close();` — it safely interrupts a blocking read in another thread.
- `class TcpServer` — a connection abstraction with no knowledge of Remote:
  - `using Handler = std::function<void(TcpStream &)>;`
  - `TcpServer(const std::string &host, std::uint16_t port, Handler handler, std::function<void(std::size_t active)> onCountChange = {});` — bind (for `::`, `v6_only=false`), listen, an asynchronous accept on `ioContext()`. Each connection has its own thread that calls `handler`. `onCountChange` is called after each open/close.
  - `std::string host() const;` `std::uint16_t port() const;` (the real port when `port == 0`).
  - `~TcpServer()` — stops the accept, closes the open connections, joins their threads.

- [ ] **Step 1: The tests `TestNetTcp.cpp`** (`/net/tests`):
  - `test_net_tcp_echo`: a server on `127.0.0.1:0` with an echo handler (`getline` → a reply). The client sends 3 lines and reads them. `peerHost()` in the handler is `127.0.0.1`.
  - `test_net_tcp_large_payload`: 16 MiB of random bytes in both directions (the handler returns the sum and the length) — they match.
  - `test_net_tcp_dual_stack`: a server on `::` (if `ipv6Supported()`, otherwise `0.0.0.0`). The clients on `127.0.0.1` and (with IPv6) `::1` connect.
  - `test_net_tcp_concurrent`: 16 clients in parallel with 100 requests each. `onCountChange` sees a maximum ≥ 2 and the last value 0.
  - `test_net_tcp_server_peer_abort`: the client writes half of a line and closes. The handler gets EOF/`badbit` and returns. The next client is served.
  - `test_net_tcp_server_shutdown`: the handler is blocked in a read. The destructor of the server ends in < 2 s.
  - `test_net_connect_timeout`: a connect to a port where nobody listens (`127.0.0.1:<closed>`) → `Pothos::IOException` fast. A connect to `10.255.255.1:9` with a timeout of 200 ms → an exception not later than 1.2 s.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** (`std::streambuf` over `asio::ip::tcp::socket`: `underflow` — `read_some`, `overflow`/`sync` — `write`; connect — `async_connect` with a wait on `std::future` and a cancel at the timeout).
- [ ] **Step 4: Run** the self-tests `/net/tests/test_net_tcp_*` and the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(net): blocking TCP iostream and thread-per-connection TCP server`

---

### Task 3: Remote on `lib/Net`

**Files:**
- Modify: `lib/Remote/Client.cpp`, `lib/Remote/Server.cpp` (URI and the rewrite of the wildcard address; syslog is in Task 4), `apps/PothosUtilProxyServer.cpp`, `lib/Platform/Process.hpp`, `lib/Platform/ProcessUnix.cpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/Builtin/TestProcess.cpp`, `cmake/poco-allowlist.txt` (−`lib/Remote/Client.cpp`), `lib/Remote/Builtin/TestRemote.cpp`

**Interfaces:**
- Consumes: `parseUri`, `toString`, `isWildcard`, `loopbackFor`, `TcpStream::connect`, `TcpServer` (Tasks 1–2).
- `void Pothos::Platform::requestSelfTermination(void);` — Unix: `kill(getpid(), SIGTERM)`. Windows: `SetEvent` on the event `terminationEventName(GetCurrentProcessId())`, otherwise `std::exit(1)`. The proxy server needs it until phase 5 (`waitForTerminationRequest` stays in Poco::Util).
- `RemoteClient`: `getIoStream()` returns `TcpStream`. The table of node ids keeps the address as a string. The default port is `getLocatorPort()`. The errors are `RemoteClientError` with the URI.
- `PothosUtilProxyServer`: `TcpServer(host, port, handler, onCountChange)`, the print of `Host: …`/`Port: …` as at present (`RemoteServer` reads them). `--require-active` → `requestSelfTermination()` when the counter is 0. `Poco::Net` and `Poco::Process` leave the file (`Poco::Util` stays — phase 5).

- [ ] **Step 1: The tests:**
  - `TestProcess.cpp`: `test_process_self_termination` — a child `PothosUtil --proxy-server=tcp://127.0.0.1:0 --require-active`: a client connects and disconnects → the process ends itself within 5 s.
  - `TestRemote.cpp` (the existing `test_inception`, `test_remote`, `test_server`, `test_multithread_safe` stay) + `test_remote_client_errors`: `RemoteClient("udp://localhost")`, `RemoteClient("tcp://[::1")` → `RemoteClientError`; `RemoteClient("tcp://127.0.0.1:<closed port>")` → `RemoteClientError` fast; `test_remote_ipv4_to_wildcard`: `RemoteServer("tcp://"+getWildcardAddr())`, and a client on `tcp://127.0.0.1:<port>` works.
- [ ] **Step 2: Run** → FAIL (the new tests).
- [ ] **Step 3: Implement**; remove `lib/Remote/Client.cpp` from the allowlist.
- [ ] **Step 4: Run** `/proxy/remote/tests/*`, `/platform/tests/test_process_self_termination` and the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(remote): client, server and proxy server on the Asio network layer`

---

### Task 4: syslog on Asio

**Files:**
- Create: `lib/Net/Syslog.hpp`, `lib/Net/Syslog.cpp`, `lib/Net/Builtin/TestNetSyslog.cpp`
- Modify: `lib/System/Logger.cpp` (the listener and the forwarding), `lib/Remote/Server.cpp` (`startSyslogForwarding`), `lib/System/Builtin/TestLogger.cpp`, `cmake/poco-allowlist.txt` (−`lib/System/Logger.cpp`, −`lib/Remote/Server.cpp`), `lib/CMakeLists.txt`

**Interfaces:**
- Consumes: `ioContext()`, `parseHostPort`, `resolveHost`, `ipv6Supported()` (Task 1); `System::Detail::LogRecord`, `deliver`, `setLogSink`, `guessLevel` (`lib/System/LoggerSinks.hpp`).
- `std::string formatSyslog(const System::Detail::LogRecord &r, std::string_view hostname);` — by decision 3.
- `std::optional<System::Detail::LogRecord> parseSyslog(std::string_view datagram);` — by decision 4. A time without a zone (3164) is the local time of the current year.
- `class SyslogSender { explicit SyslogSender(const std::string &addr); void send(const System::Detail::LogRecord &r); };` — the address follows decision 5, the resolve runs one time, and send errors are ignored (as in Poco).
- `class SyslogListener { explicit SyslogListener(std::function<void(System::Detail::LogRecord)> onRecord); std::uint16_t port() const; ~SyslogListener(); };` — by decision 4, port 0 (a free one), the reception runs on `ioContext()`.

- [ ] **Step 1: The tests:**
  - `test_net_syslog_roundtrip`: a record (levels trace…fatal, source `"My Source"`, a text with spaces, quotes, `]`, `\`, UTF-8, `\n`) → `formatSyslog` → `parseSyslog` → the same level/source/text/pid/tid, and the time with an accuracy of microseconds.
  - `test_net_syslog_format`: the line starts with `<12>1 ` for warning and contains `[pothos@32473 source="My Source" level="warning"`; the length is ≤ 60000 + the header for a text of 100000 bytes.
  - `test_net_syslog_parse_foreign`: `<34>Oct 11 22:14:15 mymachine su[123]: 'su root' failed` → critical, source `su`, pid 123; `<165>1 2003-10-11T22:14:15.003Z host app 77 ID47 - hello` → notice, source `app`; `<13>1 - - - - - -` → an empty text; garbage (`""`, `"<"`, `"<999>x"`, `"<13>1 2003"`, an SD without `]`) → `nullopt` with no crash.
  - `test_net_syslog_udp`: `SyslogListener` + `SyslogSender("127.0.0.1:<port>")` and (with IPv6) `"[::1]:<port>"` → the record arrives within 2 s.
  - `TestLogger.cpp`: `test_logger_syslog_restart` — `startSyslogListener` → `startSyslogForwarding("127.0.0.1:"+port)` → the record is visible in the intercepting sink; `stopSyslogListener`, a second start — the new port works.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement**; `Logger.cpp` and `Server.cpp` without Poco; both lines out of the allowlist.
- [ ] **Step 4: Run** `/net/tests/test_net_syslog_*`, `/system/tests/test_logger_*`, `/proxy/remote/tests/*`, the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(net): RFC 5424 syslog forwarding and listener on Asio`

---

### Task 5: Poco Net is off

**Files:**
- Modify: `CMakeLists.txt` (`ENABLE_NET OFF`, `install(TARGETS Foundation Util XML)`), `lib/CMakeLists.txt` and `apps/CMakeLists.txt` (without `PocoNet`), `README.md`, `CLAUDE.md` (the section about `lib/Net/`: the module, `ioContext`, the rule "Asio only in `lib/Net` and in `.cpp` files, not in `include/`"), `Changelog.txt` (pothos2: network on Asio, the syslog format RFC 5424, the dual-stack listener)

- [ ] **Step 1: RED:** `grep -rn "Poco/Net\|Poco::Net\|Poco::URI\|Poco/URI" lib apps include` → the result (record the number; it must be 0 after Tasks 1–4, otherwise record what remains).
- [ ] **Step 2: Turn Net off** in the Poco build and remove the linking.
- [ ] **Step 3: A clean configuration** `build-clean` (GCC) and `build-clang` (Clang) → the full `ctest` PASS. There are no new warnings in `lib/Net/`. Delete the directories.
- [ ] **Step 4: Commit** `build: drop Poco Net, the network layer is Asio`

---

### Task 6: Result of the phase

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (phase 4 is done, the next one is phase 5), the ledger `docs/superpowers/ledgers/2026-10-27-phase4-network-asio.md`

- [ ] **Step 1: Update the roadmap and the ledger. Commit** `docs: phase 4 complete — network on Asio`
- [ ] **Step 2: A fresh review of the whole branch** (one reviewer, the focus is Review Focus). Make the fixes in separate commits with tests.
- [ ] **Step 3: With the consent of the user, push the branch `phase4-network-asio`, make a PR to `pothos2`, run the CI on all four jobs.**

## Out of the phase

- `Poco::Util` (`PothosUtil.cpp/.hpp`, `PothosUtilProxyServer.cpp` — only `waitForTerminationRequest`, `ConfLoader`, `ModuleSafeLoad`) — phase 5.
- An HTTP/WebSocket server is a future goal. `TcpServer`/`ioContext()` are the extension point.
- The network blocks (`blocks/network`) are outside the core build. They use their own code.
