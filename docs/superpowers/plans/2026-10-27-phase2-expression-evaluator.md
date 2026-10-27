# Этап 2: собственный вычислитель выражений — план реализации

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `Pothos::Util::EvalEnvironment` работает на собственном вычислителе (C++23, без зависимостей) с Python-подобным синтаксисом; muparserx полностью удалён (сабмодуль, CMake, CI, `MupOctalValReader`, хаки `splitExpr`/`__map__`/`__tmp__N`).

**Architecture:** Внутренний модуль `lib/Util/Expr/` (namespace `Pothos::Util::Expr`, заголовки не устанавливаются): лексер → парсер (рекурсивный спуск по уровням приоритета, Python-грамматика) → AST → вычислитель над собственным типом значения `Value` (`std::variant`). Списки и словари — обычные значения грамматики, а не предобработка строки. На границе с публичным API — преобразование `Value ↔ Pothos::Object` (списки → `ProxyVector`, словари → `ProxyMap`). Публичный заголовок теряет приватные хелперы (`splitExpr`, `_evalList`, `_evalMap`), остальной API не меняется.

**Tech Stack:** C++23 (`std::variant`, `std::complex`, `std::from_chars`, `std::shared_mutex`, `std::format`), CMake/CTest, самотесты `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (раздел про muparserx), заметки — `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` («Заметки для плана этапа 2»).

## Global Constraints

- C++23, без расширений компилятора; Linux GCC/Clang, macOS, Windows MSVC — CI на всех четырёх job-ах остаётся зелёным.
- Никаких новых зависимостей. Код `lib/Util/Expr/` не включается из `include/Pothos/**` и не устанавливается.
- Синтаксис — **надмножество JSON** (`TopologyMakeJSON.cpp:35`, `BlockEval.cpp:85` отдают `arg.dump()`): `true`/`false`/`null`, числа JSON (`-3`, `1e-05`), строки JSON со всеми экранированиями (включая `\uXXXX`, `\/`), массивы, объекты.
- **Неизвестное имя — всегда исключение** (`TopologyMakeJSON.cpp:21-31` превращает невычислившуюся строку в литерал: `int32`, `hello` обязаны бросать).
- Ошибка — `Pothos::Exception` с сообщением, где есть позиция (колонка с 1) и исходное выражение.
- Совместимость синтаксиса с muparserx и с `.pothos`-топологиями не требуется (спека).
- Проверка каждой задачи: `cmake --build build && ctest --test-dir build --output-on-failure` → все PASS. Одиночный самотест: команда из `CLAUDE.md` с `-DTEST_PATH=/util/tests/<имя>`.
- Push в `origin` — только с согласия пользователя.

## Решения по семантике (на утверждение)

Спека задаёт «Python-подобный» синтаксис; ниже — то, что она не уточняет. Где Python и требования спеки расходятся, побеждает спека.

1. **Типы значений:** `None`, `bool`, `int` (int64), `float` (double), `complex` (complex<double>), `str`, `list`, `dict`. Числовая башня `bool < int < float < complex`; `bool` в арифметике — `int` (как в Python).
2. **Целые — 64-битные со строгой проверкой:** переполнение в литерале, `+ - * ** << //` и `abs`/унарном минусе → ошибка (а не тихий переход в double). Литералы: десятичные, `0x`, `0o`, `0b` (регистр префикса любой), `_` между цифрами; ведущий ноль у десятичного (`0315`) — ошибка, как в Python.
3. **Деление как в Python:** `/` всегда даёт float (complex для complex), `//` — деление с округлением вниз, `%` — остаток со знаком делителя. Целочисленное деление на ноль → ошибка; для float — IEEE (`1/0.0 = inf`), чтобы не мешать DSP-выражениям.
4. **`^` — XOR, степень — `**`** (в muparserx `^` был степенью: старые выражения вида `2^3` молча дадут другой результат — отмечается в Changelog). `**` правоассоциативна, `-2**2 == -4`; `int ** int` с неотрицательным показателем — `int`, иначе float/complex.
5. **Поэлементные операции (требование спеки, не Python):** арифметические и побитовые бинарные операторы и унарные `- + ~` над `list` применяются поэлементно (рекурсивно): `list ⊕ скаляр`, `скаляр ⊕ list`, `list ⊕ list` одинаковой длины (иначе ошибка). Поэтому `[1,2] + [3,4] == [4,6]` (не конкатенация) и `2*[1,2] == [2,4]` (не повтор). Сравнения над списками не поэлементные: `==`/`!=` сравнивают целиком, `<` и т.п. — ошибка. `str + str` — конкатенация, `str * int` — ошибка.
6. **Логика:** `and`/`or` с коротким замыканием возвращают операнд (как в Python: `x or 5`), `not` — bool; истинность по Python (`0`, `0.0`, `""`, `[]`, `{}`, `None` — ложь). Сравнения цепочкой (`0 < x < 10`). `a if c else b`. Операторы `&&`, `||`, `!`, `?:` не поддерживаются.
7. **Индексация:** `list[i]`, `str[i]` (отрицательные индексы как в Python), `dict[key]`. Срезы не поддерживаются.
8. **Встроенные имена** (пользовательская константа с тем же именем их перекрывает, например `j` или `e`):
   - константы: `True`/`true`, `False`/`false`, `None`/`null`, `j` (мнимая единица), `pi`, `e`, `inf`, `nan`;
   - функции: `abs`, `min`, `max` (несколько аргументов или один список), `sum`, `len`, `round(x[, n])`, `floor`, `ceil`, `sqrt`, `exp`, `log(x[, base])`, `ln`, `log2`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `pow`, `real`, `imag`, `conj`, `arg`, `int`, `float`, `complex(re[, im])`, `str`, `bool`, `range(stop | start, stop[, step])`. Унарные математические функции применяются к спискам поэлементно (`sin([0, pi])`).
   - Математика для complex — через `std::` перегрузки для complex; `sqrt(-1)` для float — `nan` (как `std::sqrt`), `sqrt(-1+0j)` — `1j`.
9. **Строки:** в `"..."` и `'...'`; экранирования `\" \' \\ \/ \n \t \r \b \f \0 \xHH \uXXXX` (`\u` с суррогатными парами → UTF-8). Неизвестное экранирование — ошибка.
10. **Словари:** ключ — любое значение, кроме `list`/`dict`; дубликат ключа — побеждает последний (Python; сейчас побеждает первый). Висячая запятая в `[...]`, `{...}` и в аргументах вызова разрешена.
11. **Преобразование результата в `Object`:** `None` → пустой `Pothos::Object()`, `bool` → `bool`, `int` → `long long`, `float` → `double`, `complex` → `std::complex<double>`, `str` → `std::string`, `list` → `ProxyVector`, `dict` → `ProxyMap` (через окружение `managed`, как сейчас).
12. **Преобразование константы из `Object`:** пустой → `None`; `std::string`, `bool`; по `DType`: complex → complex, float → float, целые ≤ 64 бит → int (`uint64` больше `INT64_MAX` → float, как сейчас для широких типов); `ProxyVector`/`ProxyMap` (и всё, что в них конвертируется, например `std::vector<int>`) — рекурсивно; прочее → ошибка `registerConstantObj` с именем типа.
13. **Потокобезопасность:** `eval` разбирает и вычисляет без общей блокировки на время разбора; таблица констант под `std::shared_mutex` (чтение — разделяемая блокировка, `register*`/`unregister` — исключительная). Глубина вложенности ограничена (256 уровней) — ошибка вместо переполнения стека.

## Review Focus

1. **JSON → выражение:** любое значение, которое отдаёт `nlohmann::json::dump()` для чисел, bool, null, строк с управляющими символами и не-ASCII, вложенных массивов/объектов, вычисляется обратно в то же значение. Тест `test_eval_json_superset` (Task 6).
2. **Неизвестные имена бросают:** `int32`, `hello`, `float32` (без кавычек) → `Pothos::Exception`, а не значение — от этого зависит `Topology::make`. Тест `test_eval_unknown_name_throws` (Task 6).
3. **Переполнение int64** в литерале и арифметике даёт ошибку, а не UB (проверка до операции, без знакового переполнения). Тест `test_expr_int_overflow` (Task 3).
4. **Глубокая вложенность** (`((((...))))` × 10000, `[[[[...]]]]`, `-------...1`) — исключение, не падение. Тест `test_expr_depth_limit` (Task 2).
5. **Параллельные `eval` и `registerConstant*`** на одном окружении из нескольких потоков — без гонок (проверяется под TSan локально, если доступен, иначе логически). Тест `test_eval_concurrent` (Task 6).

---

### Task 1: Значение и лексер

**Files:**
- Create: `lib/Util/Expr/Value.hpp`, `lib/Util/Expr/Value.cpp`, `lib/Util/Expr/Lexer.hpp`, `lib/Util/Expr/Lexer.cpp`, `lib/Util/Builtin/TestExpr.cpp`
- Modify: `lib/CMakeLists.txt`

**Interfaces (namespace `Pothos::Util::Expr`):**
- `struct Value;` — обёртка над `std::variant<std::monostate, bool, std::int64_t, double, std::complex<double>, std::string, List, Dict>`, где `using List = std::vector<Value>; using Dict = std::vector<std::pair<Value, Value>>;` (порядок вставки, поиск по `==`). Методы: `typeName()` (`"None"`, `"bool"`, `"int"`, …), `bool truthy() const`, `bool operator==(const Value&) const` (числа сравниваются по значению через башню: `1 == 1.0 == True`), `std::string repr() const` (Python-подобный, для сообщений и тестов: `[1, 2.5, 'a', None]`).
- `class ExprError : public std::runtime_error` с полем `std::size_t pos` (смещение в выражении, 0-based) — внутреннее исключение; в `Pothos::Exception` переводится на границе (Task 6).
- `enum class Tok { Int, Float, Imag, String, Name, Op, End };` `struct Token { Tok kind; std::string_view text; std::size_t pos; Value literal; };`
- `std::vector<Token> tokenize(std::string_view expr);` — пропускает пробелы; операторы (самое длинное совпадение): `** // << >> <= >= == != + - * / % & | ^ ~ < > ( ) [ ] { } , :`; ключевые слова (`and or not if else`) возвращаются как `Name`; литералы разобраны в `literal` по решениям 2 и 9; ошибка — `ExprError` с позицией символа.

- [ ] **Step 1: Тесты в `TestExpr.cpp`** (`POTHOS_TEST_BLOCK("/util/tests", ...)`, файл в `lib/CMakeLists.txt`):
  - `test_expr_lex_numbers`: `"42"`→Int 42; `"0x1F"`, `"0o17"`, `"0b101"`, `"1_000"`; `"1.5"`, `".5"`, `"1."`, `"1e-05"`, `"2E3"` → Float; `"3j"`, `"1.5J"` → Imag; `"9223372036854775807"` ok, `"9223372036854775808"` → ExprError; `"0315"` → ExprError; `"0x"` → ExprError.
  - `test_expr_lex_strings`: `R"("a\"b")"`→`a"b`; `R"('it\'s')"`; `R"("é")`→`"\xc3\xa9"`; `R"("😀")`→4 байта UTF-8; `R"("\/\n\t")"`; `R"("abc)"`(незакрытая) → ExprError pos 0; `R"("\q")"` → ExprError.
  - `test_expr_lex_ops`: `"a**-b//c"` → `a`, `**`, `-`, `b`, `//`, `c`, End; позиции токенов верны; `"$"` → ExprError с pos 0.
  - `test_expr_value_basics`: `Value(1) == Value(1.0)`, `Value(true) == Value(1)`, `repr` списка/словаря, `truthy` для пустых контейнеров.
- [ ] **Step 2: Run** сборку → Expected: FAIL (нет заголовков).
- [ ] **Step 3: Реализовать `Value` и `tokenize`** (числа — `std::from_chars`; для int — с проверкой диапазона; для float — `std::from_chars` после удаления `_`).
- [ ] **Step 4: Run** одиночные самотесты `test_expr_lex_*`, `test_expr_value_basics` → PASS; полный `ctest` → все PASS.
- [ ] **Step 5: Commit** `feat(expr): value type and tokenizer for the new expression evaluator`

---

### Task 2: Парсер и AST

**Files:**
- Create: `lib/Util/Expr/Parser.hpp`, `lib/Util/Expr/Parser.cpp`
- Modify: `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:**
- `struct Node;` с `kind` (`Literal`, `Name`, `Unary`, `Binary`, `Compare` (цепочка), `And`, `Or`, `Not`, `IfElse`, `List`, `Dict`, `Call`, `Index`), `std::string op`, `std::vector<std::unique_ptr<Node>> children`, `std::size_t pos`, `Value literal`, `std::string name`.
- `std::unique_ptr<Node> parse(std::string_view expr);` — весь вход должен быть одним выражением (лишние токены → ошибка «unexpected 'x'» с позицией).
- `std::string dump(const Node&);` — S-выражение для тестов: `(+ 1 (* 2 3))`.

Грамматика (снизу вверх по приоритету, как в Python):
```
expr     := or_expr ['if' or_expr 'else' expr]
or_expr  := and_expr ('or' and_expr)*
and_expr := not_expr ('and' not_expr)*
not_expr := 'not' not_expr | compare
compare  := bit_or (('<'|'>'|'=='|'!='|'<='|'>=') bit_or)*
bit_or   := bit_xor ('|' bit_xor)*      bit_xor := bit_and ('^' bit_and)*
bit_and  := shift ('&' shift)*         shift   := arith (('<<'|'>>') arith)*
arith    := term (('+'|'-') term)*      term    := unary (('*'|'/'|'//'|'%') unary)*
unary    := ('-'|'+'|'~') unary | power
power    := postfix ['**' unary]
postfix  := atom ( '(' args ')' | '[' expr ']' )*
atom     := literal | NAME | '(' expr ')' | '[' [expr (',' expr)* [',']] ']'
          | '{' [expr ':' expr (',' expr ':' expr)* [',']] '}'
```
Вызов допустим только у имени (`Call` хранит имя функции); счётчик глубины рекурсии — ошибка при > 256.

- [ ] **Step 1: Тесты:**
  - `test_expr_parse_precedence`: `dump(parse("1 + 2 * 3"))=="(+ 1 (* 2 3))"`; `"-2**2"`→`(neg (** 2 2))`; `"2**3**2"`→`(** 2 (** 3 2))`; `"a if b else c if d else e"`; `"not a == b"`→`(not (== a b))`; `"0 < x < 10"`→`(cmp 0 < x < 10)`; `"1 | 2 ^ 3 & 4 << 1"`.
  - `test_expr_parse_containers`: `"[]"`, `"[1, ]"`, `"{}"`, `"{1: 2, }"`, `"[1, [2, 3]]"`, `"f(1, 2,)"`, `"a[0][1]"`, `"{'a': [1, {'b': 2}]}"`.
  - `test_expr_parse_errors` (проверяется `pos`): `"1 +"` (pos 3, «unexpected end»), `"(1"`, `"[1 2]"` (pos 3), `"{1}"`, `"1 2"`, `""` (пустое), `"f(1)(2)"`, `"1 if 2"`.
  - `test_expr_depth_limit`: 10000 `(` + `1` + 10000 `)`, 10000 `[`…, 10000 `-` + `1` → ExprError, без падения.
- [ ] **Step 2: Run** → FAIL (нет парсера).
- [ ] **Step 3: Реализовать.**
- [ ] **Step 4: Run** самотесты `test_expr_parse_*`, `test_expr_depth_limit` → PASS; полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): recursive-descent parser with Python precedence`

---

### Task 3: Вычислитель — скаляры, операторы, логика

**Files:**
- Create: `lib/Util/Expr/Eval.hpp`, `lib/Util/Expr/Eval.cpp`, `lib/Util/Expr/Ops.cpp` (арифметика и проверенные целочисленные операции)
- Modify: `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:**
- `using Lookup = std::function<const Value *(std::string_view)>;` — пользовательские константы (nullptr — не найдено, тогда встроенные имена, затем ошибка `unknown name 'x'`).
- `Value evaluate(const Node &, const Lookup &);`
- `Value evaluate(std::string_view expr, const Lookup &);` — parse + evaluate.
- Внутри `Ops.cpp`: `Value binaryOp(std::string_view op, const Value&, const Value&, std::size_t pos);`, `Value unaryOp(...)`, `int compare(...)`; целые — проверка переполнения до операции (`std::numeric_limits`, без `__builtin_*` — MSVC).

- [ ] **Step 1: Тесты** (`evaluate(...)` сравнивается через `Value ==` и проверку `typeName()`):
  - `test_expr_arith`: `1+2*3`=7 int; `7/2`=3.5 float; `7//2`=3; `-7//2`=-4; `-7%3`=2; `7.5//2`=3.0; `2**10`=1024 int; `2**-1`=0.5; `(-8)**(1/3)` → complex; `1+2j`+`1` → complex(2,2); `True+True`=2 int; `"ab"+"cd"`; `1/0.0`=inf; `1/0` → ошибка с pos оператора; `5%0` → ошибка.
  - `test_expr_int_overflow`: `9223372036854775807+1`, `-9223372036854775807-2`, `2**63`, `3037000500*3037000500`, `1<<64`, `-(-9223372036854775807-1)`, `(-9223372036854775807-1)//-1`, `2**62*2` → ошибка; `1<<62`, `-9223372036854775807-1` → ok.
  - `test_expr_bitwise`: `6&3`=2, `6|3`=7, `6^3`=5, `~5`=-6, `1<<4`=16, `-16>>2`=-4; `1.0&1` → ошибка «unsupported operand types».
  - `test_expr_compare_logic`: `1<2<3` True, `3>2>2` False, `1==1.0` True, `"a"<"b"` True, `1<"a"` → ошибка, `1=="a"` False, `0 or 5`=5, `2 and 0`=0, `not []`=True, `0 and undefined_name`=0 (короткое замыкание не трогает правую часть), `1 if 0 else 2`=2, `1+2j < 3` → ошибка.
  - `test_expr_names`: `x+1` при Lookup `x=41` → 42; `foo` без Lookup → ошибка `unknown name 'foo'` с pos; `True`, `true`, `None`, `null`, `pi`, `j*j`=-1+0j; пользовательская `j=5` перекрывает встроенную.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать** (контейнеры и функции пока бросают «not implemented» — закрываются в Task 4/5).
- [ ] **Step 4: Run** самотесты → PASS; полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): evaluate scalars, operators, comparisons and logic`

---

### Task 4: Контейнеры и поэлементные операции

**Files:**
- Modify: `lib/Util/Expr/Eval.cpp`, `lib/Util/Expr/Ops.cpp`, `lib/Util/Builtin/TestExpr.cpp`

- [ ] **Step 1: Тесты:**
  - `test_expr_containers`: `[]`, `[1, [2, 'x'], 3]`, `{}`, `{'a': 1, 'b': [1, 2]}`, `{1: 2, 1: 3}` → `{1: 3}`; `{[1]: 2}` → ошибка «unhashable»; `[1,2,3][-1]`=3; `[1][5]` → ошибка с pos индекса; `{'a': 1}['a']`=1; `{'a': 1}['b']` → ошибка `key 'b' not found`; `"abc"[1]`=`"b"`; `5[0]` → ошибка.
  - `test_expr_elementwise`: `(2 * [1, 2, 3]) + [3, 2, 1]` = `[5, 6, 7]`; `[1, 2] + [3, 4]`=`[4, 6]`; `[1, 2] + [1]` → ошибка длины; `-[1, [2, 3]]` = `[-1, [-2, -3]]`; `[1, 2] / 2` = `[0.5, 1.0]`; `[1,2] == [1,2]` True; `[1,2] < [3,4]` → ошибка; `{'a': 1} + 1` → ошибка; `['a', 'b'] + 'c'` = `['ac', 'bc']`.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать.**
- [ ] **Step 4: Run** самотесты → PASS; полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): lists, dicts, indexing and element-wise operators`

---

### Task 5: Встроенные функции

**Files:**
- Create: `lib/Util/Expr/Builtins.cpp`
- Modify: `lib/Util/Expr/Eval.cpp`, `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:** таблица `std::string_view → функция(std::span<const Value> args, std::size_t pos) -> Value` с проверкой числа аргументов (ошибка `sin() takes 1 argument (2 given)`); неизвестная функция → `unknown function 'foo'`; вызов не-функции (`pi(1)`) → ошибка.

- [ ] **Step 1: Тесты:**
  - `test_expr_math`: `pow(2, 3)`=8 (int), `sqrt(4)`=2.0, `sqrt(-1)` → nan, `sqrt(-1+0j)`=1j, `log2(8)`=3.0, `log(e)`=1.0, `log(8, 2)`=3.0, `ln(1)`=0.0, `atan2(1, 1)`=pi/4, `abs(-3)`=3 int, `abs(3+4j)`=5.0, `round(2.5)`=2 (банковское, как Python), `round(2.567, 2)`=2.57, `floor(-1.5)`=-2 int, `ceil(1.2)`=2 int, `real(1+2j)`=1.0, `imag(1+2j)`=2.0, `conj(1+2j)`, `arg(1j)`=pi/2, `sin([0, pi/2])` ≈ `[0, 1]`.
  - `test_expr_builtins`: `min(3, 1, 2)`=1, `max([1, 5, 2])`=5, `min([])` → ошибка, `sum([1, 2.5])`=3.5, `len([1, 2])`=2, `len('abc')`=3, `len({})`=0, `int(3.9)`=3, `int('42')`=42, `float('1e3')`=1000.0, `complex(1, 2)`, `str(1.5)`=`'1.5'`, `bool([])`=False, `range(3)`=`[0, 1, 2]`, `range(1, 7, 2)`=`[1, 3, 5]`, `range(0, 1, 0)` → ошибка, `range(10000000)` → ошибка (лимит длины 1 000 000), `sin(1, 2)` → ошибка числа аргументов, `foo(1)` → `unknown function 'foo'`.
  - Пользовательская константа `sin=1` не ломает вызов `sin(0)` (функции и константы — разные пространства имён).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Реализовать.**
- [ ] **Step 4: Run** самотесты → PASS; полный `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): built-in math, conversion and container functions`

---

### Task 6: `EvalEnvironment` на новом вычислителе

**Files:**
- Create: `lib/Util/Expr/Convert.hpp`, `lib/Util/Expr/Convert.cpp` (`Value ↔ Pothos::Object`)
- Modify: `include/Pothos/Util/EvalEnvironment.hpp` (убрать `splitExpr`, `_evalList`, `_evalMap`; документация синтаксиса в Doxygen-комментарии класса — краткая грамматика, типы, отличия от Python), `lib/Util/EvalEnvironment.cpp` (переписать), `lib/Util/Builtin/TestEvalExpression.cpp`, `lib/CMakeLists.txt`
- Delete: `lib/Util/EvalEnvironmentListParsers.cpp`, `lib/Util/Builtin/MupOctalValReader.cpp`, `lib/Util/Builtin/MupOctalValReader.hpp`

**Interfaces:**
- `Pothos::Object toObject(const Value &);` и `Value fromObject(const Pothos::Object &);` — по решениям 11–12.
- `EvalEnvironment::Impl`: `std::shared_mutex mutex; std::map<std::string, Value, std::less<>> constants;`. `eval` — `Expr::evaluate` с `Lookup` по `constants` под разделяемой блокировкой; `ExprError` → `Pothos::Exception("EvalEnvironment::eval", "<msg> at column N in: <expr>")`. `registerConstantExpr(key, expr)` — вычислить, затем записать под исключительной блокировкой (ошибка содержит имя ключа). Имя константы проверяется: идентификатор, не ключевое слово (`and`, `if`, …) — иначе ошибка.

- [ ] **Step 1: Обновить/добавить тесты в `TestEvalExpression.cpp`** (существующие сохраняются — их синтаксис уже совместим; `test_eval_octal_constant` проходит с `0o315`):
  - `test_eval_json_superset`: для набора `nlohmann::json` (`-3`, `1e-05`, `true`, `null`, `"tab\tquote\"é"`, `"\u0001"`, `[1, [2.5, "x"], {}]`, `{"a": {"b": [true, null]}}`, `1.7976931348623157e308`) — `eval(j.dump())`, сравнение результата с исходным значением (числа/строки/вложенность; `null` → пустой Object).
  - `test_eval_unknown_name_throws`: `int32`, `hello`, `float32`, `complex_float32` → `Pothos::Exception`; сообщение содержит `unknown name` и `column`.
  - `test_eval_topology_doc_example`: `registerConstantExpr("fiz", "3")`, `eval("-fiz") == -3` (пример из `Topology.hpp`).
  - `test_eval_object_roundtrip`: `registerConstantObj` для `std::vector<std::string>`, `ProxyMap`, `uint64_t(UINT64_MAX)` (→ float), `std::complex<float>`, пустого `Object()` (→ `None`: `eval("x == None")` → True; оператора `is` нет); неподдерживаемый тип (`Pothos::DType`) → исключение с именем типа.
  - `test_eval_constant_name_rules`: `registerConstantExpr("if", "1")` → исключение; `unregisterConstant("missing")` — без ошибки; перерегистрация заменяет значение.
  - `test_eval_concurrent`: 8 потоков × 2000 итераций: `eval("x * [1, 2, 3]")` и `registerConstantExpr("x", "<i>")` на одном `EvalEnvironment`; ни одного исключения, результат всегда список длины 3.
  - `test_eval_error_message`: `eval("1 + * 2")` → сообщение содержит `column 5` и исходное выражение.
- [ ] **Step 2: Run** → FAIL (новые тесты; muparserx ещё подключён, старый код компилируется).
- [ ] **Step 3: Переписать `EvalEnvironment.cpp`**, удалить хаки и `MupOctalValReader`; убрать `pothos_muparserx` из `target_link_libraries` в `lib/CMakeLists.txt` (сабмодуль и `third_party` — в Task 7).
- [ ] **Step 4: Проверить вызывающих в ядре** — `lib/Framework/TopologyMakeJSON.cpp`, `lib/Util/Builtin/BlockEval.cpp` (поведение «строка, которая не вычислилась, — литерал» сохраняется; кода менять не нужно, если тесты топологий PASS). Run полный `ctest` → все PASS (в т.ч. `/framework/tests/test_topology_*`).
- [ ] **Step 5: Commit** `feat: EvalEnvironment on the built-in expression evaluator, drop muparserx usage`

---

### Task 7: Удаление muparserx

**Files:**
- Modify: `third_party/CMakeLists.txt` (убрать `pothos_muparserx`), `.gitmodules` и индекс (`git rm muparserx`), `.github/workflows/ci.yml` (`git submodule update --init poco libsimdpp`), `tests/consumer/CMakeLists.txt` (регулярка: `Poco|nlohmann|simdpp`), `lib/CMakeLists.txt` (комментарий про transitional deps), `README.md` (раздел Dependencies, команда инициализации сабмодулей), `CLAUDE.md` (сабмодули ядра, правило про transitional deps, раздел про вычислитель: где код, синтаксис — ссылка на Doxygen `EvalEnvironment.hpp`), `Changelog.txt` (запись для pothos2: новый синтаксис выражений, `^` теперь XOR, `/` всегда float)

- [ ] **Step 1: RED:** `grep -rniI 'muparser\|mup::' --exclude-dir=build --exclude-dir=.cache --exclude-dir=docs --exclude-dir={poco,flow,blocks,muparserx} .` → находит ссылки (зафиксировать число).
- [ ] **Step 2: Удалить сабмодуль и ссылки** (`git submodule deinit -f muparserx && git rm muparserx`, каталог `.git/modules/muparserx` оставить — не трогать общее git-хранилище).
- [ ] **Step 3: Чистая конфигурация:** `cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-clean && ctest --test-dir build-clean --output-on-failure` → все PASS; повторный grep из Step 1 → пусто (кроме исторических строк `Changelog.txt`). Удалить `build-clean`.
- [ ] **Step 4: Commit** `build: remove the muparserx submodule`

---

### Task 8: Итог этапа

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (статус: этап 2 выполнен, следующий — этап 4), журнал `docs/superpowers/ledgers/2026-10-27-phase2-expression-evaluator.md`

- [ ] **Step 1: Сборка с Clang** (`-DCMAKE_CXX_COMPILER=clang++`, отдельный каталог) и полный `ctest` → PASS, без новых предупреждений в `lib/Util/Expr/`.
- [ ] **Step 2: Обновить roadmap и журнал.**
- [ ] **Step 3: Commit** `docs: phase 2 complete — built-in expression evaluator`
- [ ] **Step 4: Свежее ревью всей ветки** (один ревьюер, фокус — раздел Review Focus), исправления отдельными коммитами.
- [ ] **Step 5: С согласия пользователя — push ветки `phase2-expression-evaluator` и PR в `pothos2`, дождаться CI на всех четырёх job-ах.**

## Вне этапа

- Подмодули `flow` и `blocks` (не входят в сборку ядра) используют `EvalEnvironment` только через публичный API; их выражения по умолчанию (`log2(val)`, `[1, 1, -1]`, `0x55`, `1e6`, `"float64"`) совместимы с новым синтаксисом. Адаптация сохранённых `.pothos` не требуется (спека).
- Закомментированный пример `DType("int32")` в тесте не восстанавливается: вызов произвольных Pothos-функций из выражений — не цель этапа.
