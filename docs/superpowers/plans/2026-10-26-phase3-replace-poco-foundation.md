# Этап 3: замена Poco Foundation — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** ядро не использует Poco Foundation/XML нигде, кроме кода, который ещё держится на Poco Net/Util (Remote, `Util/Network.cpp`, syslog-часть логгера, `apps/PothosUtil*`); появляются публичный logging API на `std::format` и внутренний платформенный слой.

**Architecture:** Замена идёт «храповиком»: CTest-тест `pothos.poco_allowlist` разрешает Poco только файлам из `cmake/poco-allowlist.txt` и падает, если файл из списка Poco больше не использует — каждая задача обязана сокращать список. Стандарт C++23 закрывает строки/числа/формат/файлы/random/regex/byteorder; ОС-специфичное (переменные окружения, сведения о системе, загрузка библиотек, процессы и пайпы) уходит во внутренний слой `lib/Platform/` (namespace `Pothos::Platform`, заголовки не устанавливаются). Логирование — собственное ядро с приёмниками console/color/file/null; UDP-syslog пока проходит через адаптеры к Poco Net (заменяются на этапе 4).

**Tech Stack:** C++23 (`std::format`, `std::filesystem`, `<random>`, `<regex>`, `std::byteswap`), POSIX (`posix_spawnp`, `pipe`, `dlopen`, `uname`), Win32 (`CreateProcessW`, `CreatePipe`, `LoadLibraryW`), CMake/CTest, существующий фреймворк самотестов `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (дорожная карта: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`; этап 3 выполняется до этапа 2 по решению пользователя)

## Global Constraints

- C++23, без расширений компилятора; Linux, macOS (≥ 13.3), Windows (MSVC) — CI на всех трёх должен остаться зелёным.
- Никаких новых внешних зависимостей; код ОС — только системные API.
- Внутренние заголовки (`lib/Platform/*`, вспомогательные `lib/**/*.hpp`) не устанавливаются и не включаются из `include/Pothos/**`.
- Публичный API можно менять (совместимость не требуется), архитектура блоков/портов/топологий/Object/Callable/Proxy сохраняется.
- Namespace — открытый вопрос (спека, раздел «Ветки и версия»): новый код живёт в `Pothos`/`Pothos::System`/`Pothos::Platform`, имя `Pothos` не размножается в строковых литералах и путях без нужды.
- Переменные окружения логгера сохраняются: `POTHOS_LOG_LEVEL` (по умолчанию `information`), `POTHOS_LOG_CHANNEL` (`color`; также `console`, `file`, `null`), `POTHOS_LOG_FILE` (`pothos.log`), `POTHOS_LOG_FORMAT` (`%Y-%m-%d %H:%M:%S %s: %t`), `POTHOS_LOG_TIMEZONE` (`local` | `UTC`); новая — `POTHOS_LOG_ASYNC` (`1` по умолчанию, `0` — синхронно).
- Quill не используется (решение пользователя); логгер — собственный, с внутренним интерфейсом приёмника, позволяющим подключить Quill позже без изменения публичного API.
- Пересылка логов по UDP остаётся совместимой с syslog (на этом этапе — через Poco Net).
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все тесты PASS, самотестов N из N.
- Push в `origin` — только с согласия пользователя.

## Review Focus

1. **Аргументы процессов с пробелами и кавычками** (пути модулей, путь к `PothosUtil` вида `C:\Program Files\...`) должны доходить до дочернего процесса без изменений на всех ОС. Тест `test_windows_command_line_quoting` (Task 6) проверяет алгоритм квотирования как чистую функцию на всех платформах.
2. **Дочерний процесс пишет больше, чем вмещает буфер пайпа (> 64 КиБ)**, а родитель читает только stdout — не должно быть дедлока. Тест `test_process_large_output` (Task 6).
3. **Логирование из многих потоков одновременно** — без потери и склейки сообщений, без падений. Тест `test_logger_concurrent` (Task 7).
4. **Сообщение лога содержит фигурные скобки** (JSON, пользовательский текст) — не должно бросать `std::format_error`. Тест `test_logger_braces_in_message` (Task 7).
5. **Неверное значение `POTHOS_LOG_LEVEL`** (`bogus`) — запуск не падает, используется `information`, выводится одно предупреждение. Тест `test_logger_bad_level_env` (Task 7).

---

### Task 1: Храповик `pothos.poco_allowlist`

**Files:**
- Create: `cmake/PothosCheckPoco.cmake`, `cmake/poco-allowlist.txt`
- Modify: `CMakeLists.txt` (секция тестов)

**Interfaces:**
- Produces: CTest-тест `pothos.poco_allowlist` = `cmake -DSRC=${PROJECT_SOURCE_DIR} -DALLOWLIST=${PROJECT_SOURCE_DIR}/cmake/poco-allowlist.txt -P cmake/PothosCheckPoco.cmake`.
- Produces: формат `poco-allowlist.txt` — один путь от корня репозитория на строку, `#`-комментарии, пустые строки игнорируются. **Каждая следующая задача удаляет из него строки файлов, которые перестали использовать Poco.**

Правила скрипта: сканируются `include/`, `lib/`, `apps/` (`*.cpp`, `*.hpp`, `*.in.cpp`); файл «использует Poco», если содержит `#include <Poco/` / `#include "Poco/` или `Poco::`/`POCO_`/`poco_`. Ошибка, если (а) файл использует Poco и не в списке; (б) файл в списке, но Poco не использует или не существует; (в) любой файл под `include/` использует Poco (даже из списка). Сообщение перечисляет все нарушения.

- [ ] **Step 1: Написать скрипт и пустой список, зарегистрировать тест**
- [ ] **Step 2: Запустить — должен упасть**

Run: `ctest --test-dir build -R poco_allowlist --output-on-failure`
Expected: FAIL, перечислено ~88 файлов (ровно те, что дают `grep -rlE '#include\s*[<"]Poco/|Poco::|POCO_|poco_' include lib apps`), в т.ч. `include/Pothos/Exception.hpp` как нарушение правила (в).

- [ ] **Step 3: Убрать Poco из `include/Pothos/Exception.hpp`** — удалить строку `catch(const Poco::Exception &ex)...` из `POTHOS_EXCEPTION_CATCH` (Poco::Exception наследует `std::exception`, его ловит следующая ветка). Заполнить список текущими файлами `lib/`/`apps/`.
- [ ] **Step 4: Run** `cmake --build build && ctest --test-dir build --output-on-failure` → Expected: все PASS (5 тестов).
- [ ] **Step 5: Commit** `test: ratchet test that confines Poco to an allowlist`

---

### Task 2: Строки и числа

**Files:**
- Create: `lib/Util/StringUtils.hpp`, `lib/Util/Builtin/TestStringUtils.cpp`
- Modify: все файлы с `Poco/StringTokenizer.h`, `Poco/String.h`, `Poco/NumberFormatter.h`, `Poco/NumberParser.h`, `Poco/Types.h`, `Poco/HashMap.h`, `Poco/Hash.h`, `Poco/Random.h`, `Poco/RandomStream.h`, `Poco/ByteOrder.h`, `Poco/RegularExpression.h`, `Poco/Bugcheck.h` (список: `grep -rlE 'Poco/(StringTokenizer|String|NumberFormatter|NumberParser|Types|HashMap|Hash|Random|RandomStream|ByteOrder|RegularExpression|Bugcheck)\.h' lib apps`); `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (`lib/Util/StringUtils.hpp`, namespace `Pothos::Util::Detail`, header-only):
  - `std::string_view trim(std::string_view s);` — пробельные по `std::isspace` с обеих сторон.
  - `std::string toLower(std::string_view s);`, `std::string toUpper(std::string_view s);` — ASCII.
  - `bool iequals(std::string_view a, std::string_view b);` — ASCII без учёта регистра.
  - `std::vector<std::string> split(std::string_view s, std::string_view separators, bool trimTokens = true, bool skipEmpty = true);` — эквивалент `Poco::StringTokenizer(s, separators, TOK_TRIM|TOK_IGNORE_EMPTY)`; разделитель — любой символ из `separators`.
  - `std::string replaceAll(std::string s, std::string_view from, std::string_view to);`

Замены без обёрток: `NumberFormatter` → `std::format`/`std::to_string`; `NumberParser` → `std::from_chars` (ошибка разбора → `Pothos::SyntaxException`, если вызывающий код ожидал исключение); `Poco::Int64`… → `<cstdint>`; `HashMap` → `std::unordered_map`; `Poco::hash` → `std::hash`; `Random`/`RandomStream` → `std::mt19937_64` + `std::random_device`; `ByteOrder` → `std::byteswap` + `std::endian`; `RegularExpression` → `std::regex` (ECMAScript); `poco_assert`/`poco_bugcheck` → `assert`/`throw Pothos::AssertionViolationException`.

- [ ] **Step 1: Тесты `TestStringUtils.cpp`** (`POTHOS_TEST_BLOCK("/util/tests", ...)`, добавить в `lib/CMakeLists.txt`):
  - `test_string_trim`: `trim("  a b \t\n") == "a b"`, `trim("") == ""`, `trim("   ") == ""`.
  - `test_string_case`: `toLower("AbC1") == "abc1"`, `toUpper("abc") == "ABC"`, `iequals("Port:", "port:")`, `!iequals("a", "ab")`.
  - `test_string_split`: `split("Port: 1234", " ")` == `{"Port:", "1234"}`; `split(" a , ,b ", ",")` == `{"a", "b"}`; `split("a,,b", ",", true, false)` == `{"a", "", "b"}`; `split("", ",")` пуст.
  - `test_string_replace_all`: `replaceAll("a.b.c", ".", "::") == "a::b::c"`, `replaceAll("aaa", "a", "aa") == "aaaaaa"` (не зацикливается).
- [ ] **Step 2: Run** `ctest --test-dir build -R selftests` → Expected: FAIL на сборке (`StringUtils.hpp` не существует).
- [ ] **Step 3: Реализовать `StringUtils.hpp`; заменить перечисленные Poco-заголовки и вызовы; удалить освободившиеся файлы из `cmake/poco-allowlist.txt`.**
- [ ] **Step 4: Run** полный `ctest` → Expected: все PASS; `grep -rlE 'Poco/(StringTokenizer|String|NumberFormatter|NumberParser|Types|HashMap|Hash|Random|RandomStream|ByteOrder|RegularExpression|Bugcheck)\.h' lib apps` → пусто.
- [ ] **Step 5: Commit** `refactor: replace Poco string/number utilities with the standard library`

---

### Task 3: `Poco::format` → `std::format`

**Files:**
- Modify: все файлы с `Poco/Format.h` (`grep -rl 'Poco/Format.h' lib apps`, 37 файлов); `cmake/poco-allowlist.txt`

**Interfaces:** нет новых.

Правила перевода спецификаторов: `%s %d %u %z %?d %?i %b` → `{}`; `%x` → `{:x}`; `%0Nd` → `{:0N}`; `%.Nf` → `{:.Nf}`; `%f` → `{:f}`; `%e` → `{:e}`; `%%` → `%`; позиционные `%[n]s` → `{n}`. Литеральные `{`/`}` в исходной строке экранировать как `{{`/`}}`. Если формат-строка не литерал (собирается в рантайме) — `std::vformat` + `std::make_format_args`, либо конкатенация.

- [ ] **Step 1: RED** — `ctest -R poco_allowlist` проходит, а `grep -rc 'Poco::format' lib apps | grep -v ':0' | wc -l` даёт число > 0: зафиксировать его в отчёте задачи.
- [ ] **Step 2: Перевести все вызовы; удалить освободившиеся файлы из списка.**
- [ ] **Step 3: Run** полный `ctest` → Expected: все PASS; `grep -r 'Poco::format\|Poco/Format.h' lib apps` → пусто. Сборка без новых предупреждений `-Wformat`.
- [ ] **Step 4: Commit** `refactor: Poco::format -> std::format`

---

### Task 4: Файловая система

**Files:**
- Modify: файлы с `Poco/Path.h`, `Poco/File.h`, `Poco/TemporaryFile.h`, `Poco/Glob.h` (`grep -rlE 'Poco/(Path|File|TemporaryFile|Glob)\.h' lib apps`), `lib/ConfLoader/FileRealPath.cpp`, `lib/System/Paths.in.cpp`, `lib/Init.cpp`, `lib/Framework/Builtin/TestSharedBuffer.cpp`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces: публичные функции `Pothos::System::get*Path()` продолжают возвращать `std::string` в формате `std::filesystem::path::string()` (на Windows — с `\`); тип возврата на `std::filesystem::path` меняется на этапе 6, не сейчас.
- Produces (`lib/Util/TempFile.hpp`, namespace `Pothos::Util::Detail`): `class TempFile { public: TempFile(); ~TempFile(); const std::filesystem::path &path() const; }` — уникальное имя в `std::filesystem::temp_directory_path()`, файл создаётся в конструкторе, удаляется (ошибки игнорируются) в деструкторе; некопируемый.

Замены: `Poco::Path`/`File` → `std::filesystem` (`exists`, `is_directory`, `create_directories`, `file_size`, `absolute`, `canonical`/`weakly_canonical`); проверка «исполняемый» в `Init.cpp` → `std::filesystem::status(p).permissions() & perms::owner_exec` на POSIX, на Windows — существование файла; `Glob` → обход `directory_iterator` + сравнение расширения/шаблона (уточнить по месту использования); `Poco::Path::home()` → `HOME`/`USERPROFILE` через Task 5 (до Task 5 — временно `std::getenv`).

- [ ] **Step 1: Тест** `lib/Util/Builtin/TestTempFile.cpp`: `test_temp_file` — путь существует после конструктора, лежит в `temp_directory_path()`, два объекта дают разные пути, после деструктора файла нет.
- [ ] **Step 2: Run** → FAIL на сборке.
- [ ] **Step 3: Реализовать `TempFile`, перевести файлы, удалить строки из списка.**
- [ ] **Step 4: Run** полный `ctest` → все PASS; `grep -rlE 'Poco/(Path|File|TemporaryFile|Glob)\.h' lib apps` → пусто.
- [ ] **Step 5: Commit** `refactor: Poco::Path/File/TemporaryFile/Glob -> std::filesystem`

---

### Task 5: Платформенный слой — окружение и сведения о системе

**Files:**
- Create: `lib/Platform/Environment.hpp`, `lib/Platform/EnvironmentUnix.cpp`, `lib/Platform/EnvironmentWindows.cpp`, `lib/Platform/Builtin/TestEnvironment.cpp`
- Modify: файлы с `Poco/Environment.h`, `Poco/Platform.h`, `Poco/Foundation.h` (`grep -rlE 'Poco/(Environment|Platform|Foundation)\.h' lib apps`), `lib/System/HostInfo.cpp`, `lib/Util/UID.cpp`, `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (namespace `Pothos::Platform`):
  - `std::optional<std::string> getEnv(const std::string &name);`
  - `void setEnv(const std::string &name, const std::string &value);` — на Windows через `_putenv_s` (видно и `std::getenv`, и дочерним процессам).
  - `void unsetEnv(const std::string &name);`
  - `std::string osName();` (`Linux`/`Darwin`/`Windows NT` — как у `uname -s` и Poco), `std::string osVersion();`, `std::string osArchitecture();`, `std::string nodeName();`, `unsigned processorCount();` (`std::thread::hardware_concurrency()`, минимум 1), `int currentProcessId();`
- Макросы платформы: `POCO_OS_FAMILY_WINDOWS` → `_WIN32`, `POCO_OS_FAMILY_UNIX` → `!_WIN32`, `POCO_OS == POCO_OS_MAC_OS_X` → `__APPLE__`.

- [ ] **Step 1: Тесты** (`/platform/tests`): `test_env_roundtrip` — имя `POTHOS_TEST_ENV_` + `currentProcessId()`: `setEnv(n,"a b=c")` → `getEnv(n) == "a b=c"`, `unsetEnv(n)` → `getEnv(n) == std::nullopt`; `test_env_missing` — `getEnv("POTHOS_SURELY_UNSET_VARIABLE") == std::nullopt`; `test_host_info` — `osName()`, `nodeName()`, `osArchitecture()` не пусты, `processorCount() >= 1`, `currentProcessId() > 0`.
- [ ] **Step 2: Run** → FAIL на сборке.
- [ ] **Step 3: Реализовать (POSIX: `getenv/setenv/unsetenv`, `uname`; Windows: `_dupenv_s`/`_putenv_s`, `RtlGetVersion` или `GetVersionEx`, `GetComputerNameW`, `GetNativeSystemInfo`), перевести файлы, список.**
- [ ] **Step 4: Run** полный `ctest` → все PASS; `grep -rlE 'Poco/(Environment|Platform|Foundation)\.h|Poco::Environment|POCO_OS' lib apps` → только файлы, оставшиеся в allowlist по другим причинам.
- [ ] **Step 5: Commit** `feat: internal platform layer for environment and host info`

---

### Task 6: Платформенный слой — библиотеки, процессы, пайпы

**Files:**
- Create: `lib/Platform/DynamicLibrary.hpp`, `lib/Platform/DynamicLibraryUnix.cpp`, `lib/Platform/DynamicLibraryWindows.cpp`, `lib/Platform/Process.hpp`, `lib/Platform/ProcessUnix.cpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/CommandLine.cpp`, `lib/Platform/Builtin/TestDynamicLibrary.cpp`, `lib/Platform/Builtin/TestProcess.cpp`
- Modify: файлы с `Poco/SharedLibrary.h`, `Poco/Process.h`, `Poco/Pipe.h`, `Poco/PipeStream.h` (`lib/Plugin/Module.cpp`, `lib/Plugin/Loader.in.cpp`, `lib/Plugin/ModuleSafeLoad.cpp`, `lib/ConfLoader/*`, `lib/Util/Builtin/GccClangCompilerSupport.in.cpp`, `lib/Util/Builtin/MsvcCompilerSupport.in.cpp`, `lib/Remote/Server.cpp`, `lib/Remote/Builtin/TestRemote.cpp`, `apps/PothosUtilSelfTests.cpp`); `lib/CMakeLists.txt`; `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces `DynamicLibrary` (некопируемый, перемещаемый):
  - `explicit DynamicLibrary(const std::filesystem::path &path);` — бросает `Pothos::LibraryLoadException` с путём и текстом `dlerror()`/`FormatMessageW`; на Windows — `LoadLibraryW` (UTF-8 → UTF-16).
  - `void *symbol(const std::string &name) const;` — `nullptr`, если нет; `~DynamicLibrary()` выгружает.
  - Суффикс модулей не входит в класс: остаётся текущий `MODULE_EXT` из `lib/CMakeLists.txt` (`.so` на Unix, включая macOS, `.dll` на Windows).
- Produces `Pipe` (общий handle, копирование разделяет один канал, как у Poco):
  - `Pipe();` — анонимный пайп; `std::size_t read(char *buf, std::size_t len);` (0 = EOF), `std::size_t write(const char *buf, std::size_t len);`, `void closeRead();`, `void closeWrite();`, `void close();`
  - `class PipeInputStream : public std::istream { explicit PipeInputStream(Pipe); };`, `class PipeOutputStream : public std::ostream { explicit PipeOutputStream(Pipe); };`
- Produces `Process`:
  - `struct ProcessOptions { std::vector<std::string> args; std::map<std::string, std::string> env; Pipe *in = nullptr; Pipe *out = nullptr; Pipe *err = nullptr; };` — `env` дополняет унаследованное окружение; `nullptr` — поток наследуется.
  - `static Process launch(const std::filesystem::path &exe, const ProcessOptions &opts);` — бросает `Pothos::SystemException` с путём и ошибкой ОС, если запуск не удался (а не «пустой процесс», как у Poco); после запуска родитель закрывает свои копии дочерних концов пайпов.
  - `int id() const;`, `int wait();` (код выхода; при завершении сигналом — `128 + signo`), `std::optional<int> tryWait();`, `void requestTermination();` (SIGTERM; Windows — `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT)` для процесса, запущенного с `CREATE_NEW_PROCESS_GROUP`, иначе `TerminateProcess`), `void kill();`
  - деструктор не убивает процесс (как Poco `ProcessHandle`).
- Produces `std::wstring buildWindowsCommandLine(const std::filesystem::path &exe, const std::vector<std::string> &args);` в `CommandLine.cpp` — компилируется на всех ОС (для теста), правила квотирования как у `CommandLineToArgvW` (пробелы/табы → кавычки; `"` → `\"`; обратные слэши перед `"` удваиваются).
- Ручное квотирование в вызывающем коде (`"\""+path+"\""` в `ModuleSafeLoad.cpp`) удаляется — квотирует `Process`.

- [ ] **Step 1: Тесты** (`/platform/tests`):
  - `test_dynamic_library_load`: таблица по ОС — Linux `libc.so.6`/`getpid`, macOS `/usr/lib/libSystem.B.dylib`/`getpid`, Windows `kernel32.dll`/`GetCurrentProcessId`; `symbol(name) != nullptr`, `symbol("pothos_no_such_symbol") == nullptr`.
  - `test_dynamic_library_missing`: `POTHOS_TEST_THROWS(DynamicLibrary("/no/such/lib.so"), Pothos::LibraryLoadException)`; текст исключения содержит путь.
  - `test_windows_command_line_quoting` (на всех ОС): `exe=C:\Program Files\P\PothosUtil.exe`, args `{"--load-module", "C:\my mods\a.dll", "say \"hi\"", "trail\\", ""}` → ровно `L"\"C:\\Program Files\\P\\PothosUtil.exe\" --load-module \"C:\\my mods\\a.dll\" \"say \\\"hi\\\"\" trail\\ \"\""`.
  - `test_pipe_roundtrip`: `Pipe p; PipeOutputStream(p) << "hello\nworld\n" << std::flush; p.closeWrite();` → `PipeInputStream(p)` читает строки `hello`, `world`, затем EOF.
  - `test_process_exit_code`: `launch(System::getPothosUtilExecutablePath(), {.args={"--success-code","7"}}).wait() == 7`.
  - `test_process_capture_stdout`: `PothosUtil --help` с `out`-пайпом → вывод содержит `self-tests`.
  - `test_process_large_output` (Review Focus 2): ни одна команда PothosUtil не даёт > 64 КиБ (максимум `--plugin-tree` ≈ 28 КиБ), поэтому в том же файле — вспомогательный блок `POTHOS_TEST_BLOCK("/platform/tests", helper_large_output)`: если `getEnv("POTHOS_TEST_LARGE_OUTPUT") == "1"`, пишет в `std::cout` 4096 строк по 63 символа `x` + `\n` (262144 байта), иначе ничего не делает. Тест запускает `PothosUtil --self-test1 /platform/tests/helper_large_output` с `env = {{"POTHOS_TEST_LARGE_OUTPUT","1"}}` и `out`-пайпом, читает stdout до EOF, проверяет, что строк `x…x` ровно 4096, и `wait() == 0` (заодно проверяет передачу `env`).
  - `test_process_launch_failure`: `launch("/no/such/exe", {})` → `POTHOS_TEST_THROWS(..., Pothos::SystemException)`.
- [ ] **Step 2: Run** → FAIL на сборке.
- [ ] **Step 3: Реализовать (POSIX: `posix_spawn` + `posix_spawn_file_actions`, `pipe2(O_CLOEXEC)`/`pipe`+`FD_CLOEXEC`, `waitpid`, `dlopen(RTLD_NOW|RTLD_GLOBAL)` — флаги как у Poco SharedLibrary по умолчанию проверить в `poco/Foundation/src/SharedLibrary_UNIX.cpp` и сохранить; Windows: `CreatePipe` с наследуемыми концами только для дочерних сторон, `CreateProcessW` + `STARTF_USESTDHANDLES`, `WaitForSingleObject`/`GetExitCodeProcess`), перевести вызывающий код, список.**
- [ ] **Step 4: Run** полный `ctest` → все PASS (включая Remote-тесты и `ModuleSafeLoad` через загрузку модулей); `grep -rlE 'Poco/(SharedLibrary|Process|Pipe|PipeStream)\.h' lib apps` → пусто.
- [ ] **Step 5: Commit** `feat: internal platform layer for dynamic libraries, processes and pipes`

---

### Task 7: Публичный logging API

**Files:**
- Modify: `include/Pothos/System/Logger.hpp`, `lib/System/Logger.cpp`
- Create: `lib/System/LoggerSinks.hpp`, `lib/System/LoggerSinks.cpp` (console/color/file/null, форматтер), `lib/System/Builtin/TestLogger.cpp`
- Modify: все файлы с `poco_(fatal|critical|error|warning|notice|information|debug|trace)`, `Poco::Logger`, `Poco/Logger.h`, `Poco/Message.h`, каналами Poco (`grep -rlE 'poco_(fatal|critical|error|warning|notice|information|debug|trace)|Poco::Logger|Poco/(Logger|Message|.*Channel|PatternFormatter|AutoPtr)\.h' lib apps`); `cmake/poco-allowlist.txt`

**Interfaces:**
- Produces (публично, `include/Pothos/System/Logger.hpp`, namespace `Pothos::System`):
  - `enum class LogLevel : int { Fatal = 1, Critical, Error, Warning, Notice, Information, Debug, Trace };` (числа совпадают с syslog-приоритетами Poco — нужны адаптерам syslog).
  - В класс `Logger` (существующие статические методы сохраняются): `static void log(LogLevel level, const std::string &source, const std::string &message);` (сообщение — готовый текст, не формат-строка), `static bool isEnabled(LogLevel level, const std::string &source);`, `static void setLevel(LogLevel level, const std::string &source = "");` (`""` — корень), `static LogLevel getLevel(const std::string &source = "");`, `static std::optional<LogLevel> parseLevel(std::string_view name);` (`fatal`…`trace`, без учёта регистра).
  - Шаблоны: `template <typename... Args> void log(LogLevel level, const std::string &source, std::format_string<Args...> fmt, Args &&...args);` — форматирует только если `isEnabled`.
  - Макросы: `POTHOS_LOG_FATAL(source, ...)`, `POTHOS_LOG_CRITICAL`, `POTHOS_LOG_ERROR`, `POTHOS_LOG_WARNING`, `POTHOS_LOG_NOTICE`, `POTHOS_LOG_INFORMATION`, `POTHOS_LOG_DEBUG`, `POTHOS_LOG_TRACE` → `::Pothos::System::log(LogLevel::X, source, __VA_ARGS__)`.
  - Уровень источника: точное имя → иначе корень. Порог по умолчанию — из `POTHOS_LOG_LEVEL`.
- Produces (внутренне, `lib/System/LoggerSinks.hpp`, namespace `Pothos::System::Detail`): `struct LogRecord { LogLevel level; std::string source; std::string text; std::chrono::system_clock::time_point time; unsigned long threadId; int pid; };` `using LogSink = std::function<void(const LogRecord &)>;` `void setLogSink(LogSink sink);` (заменяет текущий приёмник; для тестов и syslog-адаптеров), `std::string formatLogRecord(const LogRecord &r, std::string_view pattern, bool utc);` — поддержать токены `%Y %m %d %H %M %S %i`(мс) `%s`(source) `%t`(text) `%p`(имя уровня) `%q`(первая буква уровня) `%P`(pid) `%T`(thread id) `%%`; неизвестный токен выводится как есть.
  - **Асинхронный режим** (Quill не используется — решение пользователя; интерфейс `LogSink` оставляет возможность подключить его позже как приёмник): `Logger::log` кладёт `LogRecord` в очередь (мьютекс + `std::deque` + `condition_variable`), фоновый поток отдаёт записи приёмнику по порядку. Включён по умолчанию; `POTHOS_LOG_ASYNC=0` — синхронный режим. Правила: `Fatal` и `Critical` пишутся синхронно — `log()` сначала дожидается опустошения очереди, затем пишет сам, затем сбрасывает приёмник; `static void Logger::flush();` (публичный) ждёт, пока очередь опустеет; `void Detail::shutdownLogging();` останавливает поток с опустошением очереди и вызывается при выгрузке библиотеки (статический объект в `Logger.cpp`) — после неё `log()` пишет синхронно, а не падает (защита от логирования из деструкторов статических объектов плагинов); `setLogSink` выполняется после `flush()`. Поток стартует лениво при первой записи (а не при загрузке библиотеки) — это сохраняет безопасность `fork()` для режима `--daemon` этапа 5.
  - Пересылка в syslog (`startSyslogForwarding`) — приёмник, превращающий `LogRecord` в `Poco::Message` и отдающий в `Poco::Net::RemoteSyslogChannel` (в одном `.cpp`, остаётся в allowlist до этапа 4); приём (`startSyslogListener`) — `Poco::Net::RemoteSyslogListener` + маленький `Poco::Channel`, вызывающий `Logger::log`. `lib/Remote/Server.cpp::pipeToSyslogWorker` пишет через `Logger::log`.

- [ ] **Step 1: Тесты** (`/system/tests`, приёмник подменяется через `setLogSink`, в конце теста восстанавливается):
  - `test_logger_levels`: `setLevel(Warning, "t.lvl")`; `POTHOS_LOG_INFORMATION("t.lvl", "x")` не доходит до приёмника; `POTHOS_LOG_ERROR("t.lvl", "v={}", 42)` доходит с `text == "v=42"`, `level == LogLevel::Error`, `source == "t.lvl"`.
  - `test_logger_format_pattern`: `formatLogRecord({Error, "src", "msg", <2026-01-02 03:04:05.006 UTC>, ...}, "%Y-%m-%d %H:%M:%S.%i %p %s: %t", true) == "2026-01-02 03:04:05.006 Error src: msg"`.
  - `test_logger_braces_in_message` (RF 4): `Logger::log(Error, "t", "{raw}")` и `POTHOS_LOG_ERROR("t", "{}", "{\"a\":1}")` → тексты `{raw}` и `{"a":1}`, без исключений.
  - `test_logger_concurrent` (RF 3): 8 потоков × 1000 `POTHOS_LOG_ERROR("t.mt", "{} {}", thread, i)` → приёмник (с мьютексом) получил 8000 записей, все тексты уникальны.
  - `test_logger_async_order`: в асинхронном режиме 1000 `POTHOS_LOG_ERROR("t.order", "{}", i)` из одного потока → после `Logger::flush()` приёмник получил 1000 записей в порядке `0..999`.
  - `test_logger_fatal_sync`: приёмник с искусственной задержкой 1 мс на запись; 100 записей `Error`, затем `POTHOS_LOG_FATAL("t.fatal", "stop")` → к моменту возврата из `log` приёмник уже получил все 101 запись, последней — `stop` (без вызова `flush()`).
  - `test_logger_after_shutdown`: `Detail::shutdownLogging()`, затем `POTHOS_LOG_ERROR("t.down", "late")` → запись доходит до приёмника синхронно, без исключений и падения; затем асинхронный режим восстанавливается для остальных тестов (`Detail::restartLogging()` — тестовая функция в том же внутреннем заголовке).
  - `test_logger_bad_level_env` (RF 5): `parseLevel("bogus") == std::nullopt`, `parseLevel("DEBUG") == LogLevel::Debug`; и функция инициализации из окружения при `POTHOS_LOG_LEVEL=bogus` выставляет `Information` и логирует одно предупреждение (вынести её как `void Detail::configureFromEnvironment();` и вызвать в тесте с подменённым окружением через `Platform::setEnv`, восстановив после).
- [ ] **Step 2: Run** → FAIL на сборке.
- [ ] **Step 3: Реализовать ядро и приёмники; перевести все вызовы `poco_*`/`Poco::Logger` на `POTHOS_LOG_*`; syslog-адаптеры; список.**
- [ ] **Step 4: Run** полный `ctest` → все PASS; вручную: `POTHOS_LOG_LEVEL=debug build/stage/bin/PothosUtil --system-info` (с `POTHOS_ROOT=build/stage`) выводит цветные строки в формате по умолчанию; `grep -rlE 'poco_(error|debug|information|warning)|Poco::Logger' lib apps` → только syslog-адаптер в `lib/System/Logger.cpp`.
- [ ] **Step 5: Commit** `feat: public logging API on std::format; drop Poco::Logger`

---

### Task 8: DOT-разметка без Poco XML

**Files:**
- Modify: `lib/Framework/TopologyDotMarkup.cpp`, `lib/CMakeLists.txt` (убрать `PocoXML` из линковки), `CMakeLists.txt` (убрать `XML` из `install(TARGETS ...)`, `PothosConfig.cmake` in-tree список); `cmake/poco-allowlist.txt`
- Create: `lib/Framework/Builtin/TestDotMarkup.cpp`

**Interfaces:**
- Produces (внутри `TopologyDotMarkup.cpp`): `static std::string htmlEscape(std::string_view s);` — `&`→`&amp;`, `<`→`&lt;`, `>`→`&gt;`, `"`→`&quot;`, `'`→`&#39;`. HTML-метки DOT строятся конкатенацией с экранированием всех пользовательских строк (имена блоков, портов, значения).

- [ ] **Step 1: Тест** `test_dot_markup_escaping` (`/framework/tests`): топология с блоком, чьё имя (`setName`) = `a<b>&"c"`; `topology.toDotMarkup()` содержит `a&lt;b&gt;&amp;&quot;c&quot;` и не содержит `a<b>`; разметка начинается с `digraph`. Плюс: вывод текущей реализации для простой топологии (снять до изменений и сохранить как ожидаемую строку в тесте) совпадает с новым побайтно, кроме допустимых различий в пробелах — зафиксировать в тесте нормализацию пробелов.
- [ ] **Step 2: Run** → тест экранирования PASS или FAIL на текущем коде — зафиксировать; тест эталона PASS на текущем коде (снят с него).
- [ ] **Step 3: Переписать генерацию, убрать PocoXML из сборки и установки, список.**
- [ ] **Step 4: Run** полный `ctest` → все PASS; `ldd build/stage/lib/libPothos*.so | grep -c PocoXML` → `0`.
- [ ] **Step 5: Commit** `refactor: generate DOT markup without Poco XML`

---

### Task 9: Итог этапа

**Files:**
- Modify: `cmake/poco-allowlist.txt` (финальная сверка), `CLAUDE.md`, `README.md` (раздел Dependencies), `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (статус этапа 3)

- [ ] **Step 1: Проверить, что в allowlist остались только файлы, которым Poco нужен из-за Net/Util:** каждый оставшийся файл включает `Poco/Net/` или `Poco/Util/` (или является syslog-адаптером логгера); иначе — вернуться к соответствующей задаче. Run: `ctest --test-dir build -R poco_allowlist` → PASS.
- [ ] **Step 2: Обновить документацию** (CLAUDE.md: logging API, `lib/Platform/`, храповик allowlist; roadmap: этап 3 выполнен, этап 2 следующий).
- [ ] **Step 3: Run** полный `ctest` → все PASS.
- [ ] **Step 4: Commit** `docs: phase 3 complete — Poco confined to Net/Util users`
- [ ] **Step 5: С согласия пользователя — push в `origin` (ветка этапа → PR в `pothos2`), дождаться CI на всех четырёх job-ах.**
