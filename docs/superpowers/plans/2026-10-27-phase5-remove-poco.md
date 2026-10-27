# Этап 5: PothosUtil без Poco::Util, удаление Poco — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ядро и `PothosUtil` не используют Poco; сабмодуль `poco`, его сборка, храповик allowlist и все упоминания удалены. Все режимы `PothosUtil` сохраняются, включая сервер, демон Unix и службу Windows.

**Architecture:**
- **Файлы свойств.** Свой читатель и писатель формата Poco `PropertyFileConfiguration` (`lib/Util/PropertyFile.*`) для `.conf`-файлов и кеша безопасной загрузки модулей.
- **Новая внутренняя статическая библиотека `pothos_app`** (`lib/App/`, через `pothos_internal_library`). Её линкуют `PothosUtil` и libPothos (последняя — ради самотестов). В ней:
  - разбор командной строки и справка (`Options`);
  - режим сервера: ожидание запроса на завершение, демон и pid-файл на Unix, служба Windows (`Server`).
- **`PothosUtilBase`** больше не наследует `ServerApplication`. `config()` возвращает собственный разобранный `CommandLine` с теми же методами (`has`, `getString`, `getUInt`, …), поэтому обработчики команд почти не меняются.

**Tech Stack:** C++23, POSIX (`sigwait`, `fork`/`setsid`), Win32 (`SetConsoleCtrlHandler`, Service Control Manager), CMake/CTest, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (раздел «Решения»: Poco удаляется полностью, режимы `PothosUtil` сохраняются в полном объёме, синтаксис командной строки можно менять). Roadmap: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md`.

## Global Constraints

- C++23; Linux GCC/Clang, macOS, Windows MSVC — все четыре CI-job зелёные.
- «Poco удаляется полностью»; ничего из `pothos_app` и `lib/Util/PropertyFile` не попадает в `include/Pothos/**` и не устанавливается.
- «Режимы `PothosUtil` сохраняются в полном объёме»:
  - корректное завершение по SIGTERM / SIGINT / Ctrl-C (аналог `waitForTerminationRequest`);
  - Unix: `--daemon`, `--pidfile`, `--umask`;
  - Windows: регистрация, снятие и запуск как службы Windows; имена опций можно привести к общему стилю.
- «Синтаксис командной строки `PothosUtil` можно менять … вызывающий код в ядре обновляется соответственно».
  - Обязательно работают вызовы ядра и CMake-модулей:
    - `--require-active --proxy-server=URI` (`lib/Remote/Server.cpp`);
    - `--load-module PATH --success-code N` (`lib/Plugin/ModuleSafeLoad.cpp`, значения отдельными аргументами);
    - `--self-tests[=PATH]`, `--self-test1=PATH` (`cmake/PothosRunSelfTests.cmake`);
    - `--doc-parse FILES… --output FILE`, `--generate-simd-dispatchers=FILE --simd-arches=LIST --output=FILE` (`cmake/Modules/*.cmake`).
- Формат `.conf` и кеша загрузчика модулей не меняется: существующие файлы читаются так же, как их читал Poco.
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS.
- Push в `origin` — только с согласия пользователя.

## Решения (на утверждение)

1. **Командная строка.**
   - Опция с обязательным значением принимает и `--opt=value`, и `--opt value`; необязательное значение задаётся только через `=`. `-h` — то же, что `--help`.
   - Неизвестная опция или недопустимое значение — сообщение и код 64, как сейчас `EXIT_USAGE`.
   - Сначала разбирается вся строка, затем команды выполняются в порядке появления. В отличие от Poco, порядок опций больше не важен: `--self-tests --num-trials=3` тоже работает. `--help` отменяет остальные команды.
2. **Режим сервера.** Команды, которые ждут завершения (`--proxy-server`, `--run-topology` без длительности), ждут:
   - на Unix — SIGINT, SIGTERM, SIGQUIT (как Poco);
   - на Windows — Ctrl-C, Ctrl-Break, закрытия консоли и именованного события завершения.

   Событие переименовывается из `POCOTRM<pid>` в `PothosUtilTerminate<pid>` (8 hex-цифр). `Platform::Process::requestTermination` и `requestSelfTermination` обновляются вместе с ним.
3. **Unix-демон — как у Poco:**
   - `--daemon` выполняет `fork`, затем `setsid`, umask по умолчанию 027 и перенаправляет stdin, stdout, stderr в `/dev/null`;
   - `--umask=OCT` задаёт umask;
   - `--pidfile=PATH` записывает pid и удаляет файл при выходе.
4. **Служба Windows — опции в общем стиле:** `--register-service[=NAME]` (по умолчанию `PothosUtil`), `--unregister-service[=NAME]`, `--display-name=…`, `--description=…`, `--startup=auto|manual`.
   - В отличие от Poco, регистрация сохраняет остальные опции командной строки в пути запуска службы: `"PothosUtil.exe" --run-service=NAME <опции>`. Служба, запущенная без аргументов, ничего бы не делала.
   - Скрытая опция `--run-service` запускает диспетчер Service Control Manager; остановка службы — это запрос на завершение.
5. **`lib/App/` — внутренняя статическая библиотека `pothos_app`.** Код командной строки и сервера можно проверять самотестами, а не только вручную.

## Review Focus

1. **Вызовы `PothosUtil` из ядра и CMake** (список в Global Constraints) разбираются в те же опции и значения, что раньше. Тест `test_app_core_command_lines` (Task 2).
2. **Завершение сервера:** SIGTERM и SIGINT прокси-серверу и `RemoteServer::~RemoteServer` (через `requestTermination`) завершают процесс с кодом 0 за ≤ 5 с. Тест `test_app_proxy_server_terminates` (Task 3).
3. **Демон:**
   - `--daemon --pidfile=F --proxy-server=tcp://127.0.0.1:0` — родитель выходит с кодом 0, pid-файл содержит живой pid;
   - SIGTERM этому pid завершает его и удаляет файл;
   - `--umask` применяется.

   Тест `test_app_daemon_pidfile` (Task 3, только Unix).
4. **Совместимость формата свойств:** комментарии `#`/`!`, `key = value` и `key: value`, продолжение строки `\`, экранирования, пробелы вокруг, CRLF, пустые значения, повторный ключ (побеждает последний) — так же, как у Poco. Тест `test_property_file_poco_compat` (Task 1).
5. **Служба Windows:**
   - регистрация → запуск через SCM → служба работает → остановка → снятие с регистрации;
   - без прав администратора — понятная ошибка, а не падение.

   Тест `test_app_windows_service` (Task 4; на CI с правами администратора выполняется, иначе пропускается с сообщением).

---

### Task 1: Файлы свойств без Poco

**Files:**
- Create: `lib/Util/PropertyFile.hpp`, `lib/Util/PropertyFile.cpp`, `lib/Util/Builtin/TestPropertyFile.cpp`
- Modify: `lib/ConfLoader/ConfLoader.cpp`, `lib/Plugin/ModuleSafeLoad.cpp`, `lib/CMakeLists.txt`, `cmake/poco-allowlist.txt` (−2 строки)

**Interfaces (namespace `Pothos::Util::Detail`):**
- `using PropertyMap = std::map<std::string, std::string>;`
- `PropertyMap parseProperties(std::istream &is);` — грамматика `PropertyFileConfiguration::parseLine`/`readChar` из Poco: пробелы в начале пропускаются, `#`/`!` — комментарий, ключ — до `=`/`:`/конца строки (с обрезкой пробелов), значение — с экранированиями `\t \r \n \f`, `\<любой>` → символ, `\` в конце строки — продолжение; значение обрезается.
- `std::string formatProperties(const PropertyMap &map);` — как `PropertyFileConfiguration::save`: `key: value\n`, экранируются `\t \r \n \f \\`.
- `PropertyMap loadPropertyFile(const std::filesystem::path &path);` / `void savePropertyFile(const std::filesystem::path &path, const PropertyMap &map);` — ошибки → `Pothos::FileException` с путём.

- [ ] **Step 1: Тесты `TestPropertyFile.cpp`** (`/util/tests`):
  - `test_property_file_poco_compat`: текст с `# c`, `! c`, `  a = 1  `, `b: two words`, `c=`, `d`, `e = x\tz` (буквально `\t`), `f = line1 \` + перевод строки + `  line2`, `g = \\`, `CRLF` в конце строк, `a = 3` (повтор) → `{a:"3", b:"two words", c:"", d:"", e:"x<TAB>z", f:"line1   line2" (как у Poco: пробелы продолжения сохраняются), g:"\\"}`.
  - `test_property_file_roundtrip`: карта со значениями, содержащими `\n`, `\t`, `\\`, `:` и `=` → `formatProperties` → `parseProperties` → та же карта.
  - `test_property_file_io`: `savePropertyFile` во временный файл (`Util/TempFile.hpp`) → `loadPropertyFile` → та же карта; `loadPropertyFile("нет такого")` → `Pothos::FileException`.
- [ ] **Step 2: Run** → FAIL (нет заголовка).
- [ ] **Step 3: Реализовать**; перевести `ConfLoader.cpp` (`loadPropertyFile`, ошибка — `Pothos::Exception("ConfLoader(path)", …)` как сейчас) и `ModuleSafeLoad.cpp` (кеш через `loadPropertyFile`/`savePropertyFile`, отсутствие ключа — `map.find`); убрать обе строки из allowlist.
- [ ] **Step 4: Run** самотесты и полный `ctest` → PASS (загрузка модулей и `.conf` проходит через `pothos.selftests`).
- [ ] **Step 5: Commit** `feat(util): Poco-compatible property files without Poco`

---

### Task 2: Разбор командной строки (`pothos_app`)

**Files:**
- Create: `lib/App/Options.hpp`, `lib/App/Options.cpp`, `lib/App/Builtin/TestOptions.cpp`
- Modify: `lib/CMakeLists.txt` (`pothos_internal_library(pothos_app …)`, линковка в `Pothos`), `apps/CMakeLists.txt` (линковка `pothos_app`), `CLAUDE.md` (раздел про внутренние библиотеки)

**Interfaces (namespace `Pothos::App`):**
- `enum class Arg { None, Required, Optional };`
- `struct OptionSpec { std::string name; char shortName = 0; Arg arg = Arg::None; std::string argName; std::string help; bool repeatable = false; bool hidden = false; };`
- `class CommandLine` — результат разбора:
  - `const std::vector<std::pair<std::string, std::string>> &given() const;` — опции в порядке появления;
  - `const std::vector<std::string> &positional() const;`
  - `bool has(std::string_view name) const;` и `bool hasOption(std::string_view name) const;` (синоним для старого кода);
  - `std::string getString(std::string_view name, std::string def = {}) const;` (последнее значение);
  - `std::vector<std::string> getAll(std::string_view name) const;`
  - `int getInt(…, int def) const;`, `unsigned getUInt(…, unsigned def) const;`, `double getDouble(…, double def) const;` — ошибка преобразования → `Pothos::InvalidArgumentException` с именем опции.
- `CommandLine parseCommandLine(const std::vector<OptionSpec> &specs, const std::vector<std::string> &args);` — по решению 1; `--` завершает опции; ошибки → `Pothos::InvalidArgumentException` («unknown option '--x'», «option '--x' requires a value», «option '--x' takes no value», «option '--x' given more than once»).
- `std::string formatHelp(const std::vector<OptionSpec> &specs, std::string_view usage, std::string_view header);` — `--name[=ARG]` / `-h, --help`, описание с переносом строк; скрытые опции не выводятся.

- [ ] **Step 1: Тесты `TestOptions.cpp`** (`/app/tests`):
  - `test_app_parse_forms`: `--a=1 --b 2 --c=x=y -h --flag pos1 -- --not-an-option` → given `[(a,1),(b,2),(c,x=y),(help,""),(flag,"")]`, positional `[pos1, --not-an-option]`; необязательное значение `--opt` без `=` → пустое, следующий аргумент — позиционный.
  - `test_app_parse_errors`: неизвестная опция, `--flag=1` для `Arg::None`, `--req` в конце строки, повтор неповторяемой, `getUInt("n")` для `"x"` и `"-1"` → `Pothos::InvalidArgumentException`; `--var=a:1 --var=b:2` (repeatable) → `getAll("var") == {"a:1","b:2"}`.
  - `test_app_core_command_lines`: таблица опций `PothosUtil` (Task 3 экспортирует её функцией `pothosUtilOptions()` из `apps/PothosUtilOptions.cpp`; до Task 3 тест использует копию нужных опций) разбирает строки из Global Constraints: `--require-active --proxy-server=tcp://[::]:0`, `--load-module /p/m.so --success-code 200`, `--self-tests`, `--self-tests=/util`, `--self-test1=/util/tests/x`, `--doc-parse a.cpp b.cpp --output out.json`, `--generate-simd-dispatchers=in.json --simd-arches=sse2,avx2 --output=h.hpp` — с ожидаемыми значениями.
  - `test_app_help`: `formatHelp` содержит `--proxy-server[=URI]`, `-h, --help`, не содержит скрытую опцию.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать.**
- [ ] **Step 4: Run** `/app/tests/*`, полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(app): command line parser for PothosUtil`

---

### Task 3: `PothosUtil` без `Poco::Util`; ожидание завершения и демон

**Files:**
- Create: `lib/App/Server.hpp`, `lib/App/ServerUnix.cpp`, `lib/App/ServerWindows.cpp` (ожидание завершения; в Task 4 сюда добавляется служба), `lib/App/Builtin/TestServer.cpp`, `apps/PothosUtilOptions.cpp` (таблица опций)
- Modify: `apps/PothosUtil.hpp`, `apps/PothosUtil.cpp`, `apps/PothosUtil*.cpp` (только `config()`/`waitForTerminationRequest` при необходимости), `lib/Platform/Process.hpp`, `lib/Platform/ProcessWindows.cpp`, `lib/Platform/Builtin/TestProcess.cpp` (имя события), `cmake/poco-allowlist.txt` (−`apps/PothosUtil.cpp`, −`apps/PothosUtil.hpp`)

**Interfaces:**
- Consumes: `parseCommandLine`, `formatHelp`, `CommandLine` (Task 2).
- `void Pothos::App::waitForTerminationRequest(void);` — по решению 2.
- Unix: `void Pothos::App::becomeDaemon(std::optional<unsigned> umaskValue);` и `class Pothos::App::PidFile { explicit PidFile(std::filesystem::path); ~PidFile(); };` (запись при создании, удаление при уничтожении); `--daemon`/`--umask`/`--pidfile` на Windows → ошибка «not supported on Windows».
- `std::wstring Pothos::Platform::terminationEventName(int pid)` → `L"PothosUtilTerminate" + 8 hex`.
- `PothosUtilBase` (без базового класса): `const Pothos::App::CommandLine &config() const;`, `void waitForTerminationRequest();` (вызывает `App::`), прежние методы команд; `std::vector<Pothos::App::OptionSpec> pothosUtilOptions();` — те же опции и тексты справки, что сейчас, плюс серверные опции решений 3–4.
- `main`: разбор → `--help` (или нет аргументов) → справка, код 0; иначе серверные опции (демон, pid-файл) → команды по порядку; исключение команды → текст в stderr, код 64; иначе `--success-code` (0 по умолчанию).

- [ ] **Step 1: Тесты:**
  - `TestServer.cpp` (`/app/tests`):
    - `test_app_proxy_server_terminates` — запуск `PothosUtil --proxy-server=tcp://127.0.0.1:0` с чтением `Port:` и `requestTermination()` → код 0 за ≤ 5 с; на Unix ещё и прямой `kill(pid, SIGINT)` → код 0.
    - `test_app_daemon_pidfile` (Unix): `PothosUtil --daemon --umask=077 --pidfile=<tmp> --proxy-server=tcp://127.0.0.1:0` → родитель выходит с кодом 0; pid-файл появляется за ≤ 5 с с живым pid; `kill(pid, SIGTERM)` → процесс исчезает, файл удалён за ≤ 5 с.
  - `TestProcess.cpp`: `test_termination_event_name` → `L"PothosUtilTerminate0000ABCD"`.
  - Через `Process::launch`: `test_app_pothosutil_cli` — `PothosUtil --help` → код 0, stdout содержит `--self-tests`; `PothosUtil --no-such-option` → код 64, stderr содержит `--no-such-option`; `PothosUtil --var=bad` → код 64; `PothosUtil --load-module <несуществующий> --success-code 7` → не 0 и не 7.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать** `Server*`, `PothosUtilOptions.cpp`, переписать `PothosUtil.cpp/.hpp` (лог-конфигурацию больше не нужно восстанавливать после Poco), обновить имя события; убрать две строки allowlist.
- [ ] **Step 4: Run** `/app/tests/*`, `/platform/tests/*`, `/proxy/remote/tests/*`, полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(app): PothosUtil without Poco::Util — own options, termination wait, daemon and pid file`

---

### Task 4: Служба Windows

**Files:**
- Modify: `lib/App/Server.hpp`, `lib/App/ServerWindows.cpp`, `lib/App/Builtin/TestServer.cpp`, `apps/PothosUtil.cpp`

**Interfaces (только `_WIN32`):**
- `struct ServiceOptions { std::string name = "PothosUtil", displayName, description, startup = "manual"; std::vector<std::string> args; };`
- `void registerService(const ServiceOptions &opts);` — `CreateServiceW` с путём `"<exe>" --run-service=NAME <args>` (кавычки — `Platform::buildWindowsCommandLine`), описание — `ChangeServiceConfig2W`; ошибки → `Pothos::SystemException` с текстом Win32 (в т.ч. «Access is denied» без прав администратора).
- `void unregisterService(const std::string &name);`
- `int runAsService(const std::string &name, std::function<int()> main);` — `StartServiceCtrlDispatcherW`; `ServiceMain` сообщает `SERVICE_RUNNING`, запускает `main` в потоке; `SERVICE_CONTROL_STOP`/`SHUTDOWN` → событие завершения (его ждёт `waitForTerminationRequest`); код `main` → `SERVICE_STOPPED`.
- `PothosUtil`: `--register-service`/`--unregister-service` выполняются вместо команд (остальные опции, кроме служебных, сохраняются в `args`); `--run-service=NAME` → `runAsService(NAME, <обычный запуск команд>)`.

- [ ] **Step 1: Тест `test_app_windows_service`** (Windows; при отсутствии прав администратора — `std::cout` «skipped: not elevated» и выход): имя `PothosUtilTest<pid>`, `registerService({name, …, args={"--proxy-server=tcp://127.0.0.1:0"}})` → `StartServiceW` → состояние `SERVICE_RUNNING` за ≤ 10 с → `ControlService(STOP)` → `SERVICE_STOPPED` за ≤ 10 с → `unregisterService` → `OpenServiceW` не находит службу; при ошибке — снятие регистрации в любом случае. На других платформах тест печатает «skipped: Windows only».
- [ ] **Step 2: Run** (локально — только компиляция и пропуск на Linux; RED проверяется на Windows CI — зафиксировать в журнале).
- [ ] **Step 3: Реализовать.**
- [ ] **Step 4: Run** полный `ctest` → PASS (Linux); Windows — CI.
- [ ] **Step 5: Commit** `feat(app): run PothosUtil as a Windows service`

---

### Task 5: Удаление Poco

**Files:**
- Modify: `.gitmodules` и индекс (`git rm poco`), `CMakeLists.txt` (блок сборки Poco, `POCO_IN_TREE`, установка библиотек Poco, uninstall-скрипт Poco), `cmake/Modules/PothosConfig.cmake` (`POCO_IN_TREE`), `lib/CMakeLists.txt` и `apps/CMakeLists.txt` (линковка `PocoFoundation PocoUtil`), `tests/consumer/CMakeLists.txt` (регулярка без `Poco`), `.github/workflows/ci.yml` (`git submodule update --init libsimdpp`), `debian/control` (`libpoco-dev`), `README.md`, `CLAUDE.md` (храповик → «Poco удалён»), `Changelog.txt` (pothos2: Poco удалён, новая командная строка `PothosUtil`, опции службы Windows), комментарии `lib/Platform` про Poco
- Delete: `cmake/poco-allowlist.txt`, `cmake/PothosCheckPoco.cmake` и регистрация теста `pothos.poco_allowlist`

- [ ] **Step 1: RED:** `grep -rniI 'poco' --exclude-dir={build,.cache,docs,poco,flow,blocks,comms,third_party,.git} .` → найденное (зафиксировать число; исторические строки `Changelog.txt` допустимы).
- [ ] **Step 2: Удалить** (`git submodule deinit -f poco && git rm poco`; `.git/modules/poco` не трогать).
- [ ] **Step 3: Чистые конфигурации** `build-clean` (GCC) и `build-clang` → полный `ctest` PASS (тестов становится 4: без `pothos.poco_allowlist`); повторный grep → только история в `Changelog.txt`; удалить каталоги.
- [ ] **Step 4: Commit** `build: remove Poco`

---

### Task 6: Итог этапа

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (этап 5 выполнен; следующий — этап 6), журнал `docs/superpowers/ledgers/2026-10-27-phase5-remove-poco.md`

- [ ] **Step 1: Обновить roadmap и журнал. Commit** `docs: phase 5 complete — Poco removed`
- [ ] **Step 2: Свежее ревью всей ветки** (фокус — Review Focus), исправления отдельными коммитами с тестами.
- [ ] **Step 3: С согласия пользователя — push ветки `phase5-pothosutil-without-poco`, PR в `pothos2`, CI на всех четырёх job-ах.**

## Вне этапа

- Замена libsimdpp на xsimd — отдельный этап (roadmap).
- Тулкиты (`flow`, `blocks`, `comms`) по-прежнему используют Poco сами; адаптация тулкитов — не цель этапов ядра.
