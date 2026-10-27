# SDD ledger — plan: docs/superpowers/plans/2026-10-27-phase4-network-asio.md
Plan approved by the user 2026-10-27 with decision 1 changed: Asio is a vendored copy in third_party/asio (no submodule).
Pre-flight: T1 Uri/HostPort/ioContext/resolveHost → T2 TcpStream/TcpServer ok; T1+T2 → T3 Remote ok; T1 parseHostPort/resolveHost/ipv6Supported + LoggerSinks → T4 syslog ok; T5 depends on T1–T4 removing all Poco Net. No conflicts.
Task 1: Ruling: added lib/Net/Authority.hpp (shared host[:port] parser for parseUri and parseHostPort); resolveHost returns IP literals without a lookup; io_context thread is detached on Windows at unload (same as the logger worker) — cost if wrong: none (internal)
Task 1: complete (commits c6577a4..dfd9485, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.94 sec)
Task 2: Ruling: added lib/Net/TcpStreamImpl.hpp (SocketBuf + Impl shared with TcpServer, keeps Asio out of TcpStream.hpp); SocketBuf flushes pending output before a blocking read (request/response safety); TcpServer swallows handler exceptions so one connection cannot stop the server — cost if wrong: none (internal)
Task 2: complete (commits dfd9485..a12ca07, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  13.89 sec)
Task 3: Ruling: lib/Net sources are also compiled into PothosUtil (POTHOS_NET_SOURCES, like the platform layer: libPothos exports no internal symbols) — the proxy server process has a second io_context thread — cost if wrong: one idle thread; RemoteServer now also reads the 'Host:' line to choose the loopback for an empty URI; requestSelfTermination falls back to std::_Exit on Windows when no termination event exists
Task 3: complete (commits a12ca07..065016a, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  14.15 sec)
Task 4: Ruling: added test_logger_syslog_forwarding (plan had only the restart test) — Logger::startSyslogForwarding to a Net::SyslogListener; RFC 3164 timestamps are taken as local time of the current year (mktime); RemoteServer forwarding records have pid 0 (PROCID '-'): the remote process pid is not known; TcpServer/SyslogListener close their sockets directly when the io_context is already stopped (process exit) — cost if wrong: none
Task 4: complete (commits 065016a..925ece5, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  14.20 sec)
Task 5: Ruling: also removed PocoNet from cmake/Modules/PothosConfig.cmake (in-tree library path list), not listed in the plan; RED grep found only the 3 link/list lines (sources were clean after Tasks 1-4); clean GCC and Clang builds 5/5, no warnings in lib/Net
Task 5: complete (commits 925ece5..48efb3f, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  13.61 sec)
User request (2026-10-27): lib/Platform and lib/Net as separate internal static targets (pothos_platform, pothos_net) linked into libPothos and PothosUtil instead of compiling their sources twice. Ruling: they are PIC + hidden visibility, compiled as clients of the Pothos API (no POTHOS_DLL_EXPORTS); on MSVC Pothos links with /IGNORE:4217,4286 (dllimport of locally defined symbols inside Pothos.dll) — unverified until Windows CI — cost if wrong: Windows link warnings or errors to fix in CI. Verified: each Platform/Net source has one object file; libPothos exports no Net/Platform functions (only std::thread template typeinfo/vtables, as before); ctest 5/5
Final review: opus reviewer, verdict "With fixes" (0 Critical, 4 Important, 11 Minor)
Final: fixed I-1 stale connection count (close path notified outside the mutex) — test_net_tcp_count_ordering added (race did not reproduce locally before the fix: no RED), counts now delivered under the server mutex; suite 5/5
Final: fixed I-2a localhost resolved to ::1 only, IPv4-only servers unreachable — test_net_tcp_localhost_to_ipv4_server RED → GREEN: resolveHostAll sorts IPv4 first (Poco order), connect tries every address
Final: fixed I-2b serialized DNS and no carry-over — per-host lookup on its own thread, in-flight lookups outlive a timeout and are reused; DNS and connect each get the full timeout again (as with Poco) — no deterministic test (slow DNS cannot be simulated), covered by test_net_resolve/test_net_connect_timeout
Final: fixed I-3 exceptions on the io_context thread end the process, busy accept loop, leaked count — test_net_tcp_callback_exception RED (terminate) → GREEN: run() loops with catch + log, callbacks are wrapped, the connection thread is created before it is counted, accept errors back off 100 ms (EMFILE loop has no test: needs fd exhaustion)
Final: fixed I-4 ~TcpServer waits forever for a stuck handler — test_net_tcp_server_stuck_handler RED (hang) → GREEN: waits ShutdownGrace (2 s), then detaches (Connection and Impl are shared)
Final: fixed M-1 (re-graded Important: UB from network input) syslog timestamps beyond the clock range — test_net_syslog_timestamp_range RED → GREEN: years outside 1970..2200 fall back to the receive time
Final: fixed with the above (same code): M-2 flush outside the server mutex, M-6 no concurrent fallback close while the io thread lives
Final: minor (deferred): M-3 finished connections are reaped only on the next accept
Final: minor (deferred): M-4 TcpServer with an empty host always binds :: (fails without IPv6; callers pass getWildcardAddr)
Final: minor (deferred): M-5 connect timeout path waits for the io_context thread without a bound
Final: minor (deferred): M-7 SyslogListener::port() reads the socket from the caller thread
Final: minor (deferred): M-8 whole datagram size not capped (long source; macOS maxdgram 9216)
Final: minor (deferred): M-9 SyslogSender resolves once at construction (Poco retried lazily)
Final: minor (deferred): M-10 read errors set eof/fail, not badbit (plan text says badbit)
Final: minor (deferred): M-11 RFC 3164 TAG may contain spaces; December messages received in January get the new year
