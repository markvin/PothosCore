# Phase 2: own expression evaluator — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `Pothos::Util::EvalEnvironment` runs on an own evaluator (C++23, no dependencies) with a Python-like syntax. muparserx is fully removed (the submodule, CMake, CI, `MupOctalValReader`, the hacks `splitExpr`/`__map__`/`__tmp__N`).

**Architecture:** An internal module `lib/Util/Expr/` (namespace `Pothos::Util::Expr`, the headers are not installed): lexer → parser (recursive descent by precedence level, Python grammar) → AST → evaluator over an own value type `Value` (`std::variant`). Lists and dicts are normal values of the grammar. They are not a preprocessing of the string. At the border with the public API, a conversion `Value ↔ Pothos::Object` exists (lists → `ProxyVector`, dicts → `ProxyMap`). The public header loses the private helpers (`splitExpr`, `_evalList`, `_evalMap`). The rest of the API does not change.

**Tech Stack:** C++23 (`std::variant`, `std::complex`, `std::from_chars`, `std::shared_mutex`, `std::format`), CMake/CTest, the self-tests `POTHOS_TEST_BLOCK`.

**Spec:** `docs/superpowers/specs/2026-10-26-core-modernization-design.md` (the section about muparserx), notes — `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` ("Notes for the plan of phase 2").

## Global Constraints

- C++23, no compiler extensions. Linux GCC/Clang, macOS, Windows MSVC: the CI stays green on all four jobs.
- No new dependencies. `include/Pothos/**` does not include the code of `lib/Util/Expr/`, and the build does not install it.
- The syntax is a **superset of JSON** (`TopologyMakeJSON.cpp:35` and `BlockEval.cpp:85` give `arg.dump()`): `true`/`false`/`null`, JSON numbers (`-3`, `1e-05`), JSON strings with all escapes (also `\uXXXX`, `\/`), arrays, objects.
- **An unknown name is always an exception** (`TopologyMakeJSON.cpp:21-31` turns a string that failed to evaluate into a literal: `int32` and `hello` must throw).
- An error is a `Pothos::Exception` with a message that has the position (column from 1) and the source expression.
- Compatibility of the syntax with muparserx and with the `.pothos` topologies is not necessary (spec).
- The check of each task: `cmake --build build && ctest --test-dir build --output-on-failure` → all PASS. One self-test: the command from `CLAUDE.md` with `-DTEST_PATH=/util/tests/<name>`.
- Push to `origin` only with the consent of the user.

## Decisions about semantics (for approval)

The spec asks for a "Python-like" syntax. The list below has what the spec does not make exact. Where Python and the requirements of the spec differ, the spec wins.

1. **Value types:** `None`, `bool`, `int` (int64), `float` (double), `complex` (complex<double>), `str`, `list`, `dict`. The numeric tower is `bool < int < float < complex`. In arithmetic, `bool` is `int` (as in Python).
2. **Integers are 64-bit with a strict check:** An overflow in a literal, in `+ - * ** << //`, and in `abs`/unary minus gives an error (it does not silently change to double). Literals: decimal, `0x`, `0o`, `0b` (the prefix can have any case), `_` between digits. A leading zero in a decimal literal (`0315`) is an error, as in Python.
3. **Division as in Python:** `/` always gives float (complex for complex). `//` is a division that rounds down. `%` is a remainder with the sign of the divisor. An integer division by zero gives an error. For float, IEEE applies (`1/0.0 = inf`), so that DSP expressions are not blocked.
4. **`^` is XOR, the power is `**`** (in muparserx `^` was the power: old expressions like `2^3` silently give another result — the Changelog notes this). `**` is right-associative, `-2**2 == -4`. `int ** int` with a non-negative exponent is `int`, otherwise float/complex.
5. **Element-wise operations (a requirement of the spec, not Python):** The arithmetic and bitwise binary operators and the unary `- + ~` apply to a `list` element by element (recursively): `list ⊕ scalar`, `scalar ⊕ list`, `list ⊕ list` of the same length (otherwise an error). Therefore `[1,2] + [3,4] == [4,6]` (not a concatenation) and `2*[1,2] == [2,4]` (not a repeat). The comparisons of lists are not element-wise: `==`/`!=` compare the whole lists, and `<` and the like give an error. `str + str` is a concatenation, and `str * int` is an error.
6. **Logic:** `and`/`or` with short-circuit return the operand (as in Python: `x or 5`). `not` returns bool. The truth value follows Python (`0`, `0.0`, `""`, `[]`, `{}`, `None` are false). Chained comparisons (`0 < x < 10`). `a if c else b`. The operators `&&`, `||`, `!`, `?:` are not supported.
7. **Indexing:** `list[i]`, `str[i]` (negative indexes as in Python), `dict[key]`. Slices are not supported.
8. **Built-in names** (a user constant with the same name overrides them, for example `j` or `e`):
   - constants: `True`/`true`, `False`/`false`, `None`/`null`, `j` (the imaginary unit), `pi`, `e`, `inf`, `nan`;
   - functions: `abs`, `min`, `max` (several arguments or one list), `sum`, `len`, `round(x[, n])`, `floor`, `ceil`, `sqrt`, `exp`, `log(x[, base])`, `ln`, `log2`, `log10`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, `pow`, `real`, `imag`, `conj`, `arg`, `int`, `float`, `complex(re[, im])`, `str`, `bool`, `range(stop | start, stop[, step])`. The unary math functions apply to lists element by element (`sin([0, pi])`).
   - The math for complex uses the `std::` overloads for complex. `sqrt(-1)` for float is `nan` (as `std::sqrt`), and `sqrt(-1+0j)` is `1j`.
9. **Strings:** in `"..."` and `'...'`. The escapes are `\" \' \\ \/ \n \t \r \b \f \0 \xHH \uXXXX` (`\u` with surrogate pairs → UTF-8). An unknown escape is an error.
10. **Dicts:** A key is any value except `list`/`dict`. For a duplicate key the last one wins (Python; at present the first one wins). A trailing comma in `[...]`, `{...}` and in the arguments of a call is allowed.
11. **Conversion of the result to `Object`:** `None` → an empty `Pothos::Object()`, `bool` → `bool`, `int` → `long long`, `float` → `double`, `complex` → `std::complex<double>`, `str` → `std::string`, `list` → `ProxyVector`, `dict` → `ProxyMap` (through the `managed` environment, as at present).
12. **Conversion of a constant from `Object`:** An empty one → `None`. `std::string`, `bool`. By `DType`: complex → complex, float → float, integers ≤ 64 bits → int (`uint64` greater than `INT64_MAX` → float, as at present for wide types). `ProxyVector`/`ProxyMap` (and everything that converts to them, for example `std::vector<int>`) → recursively. Anything else → the error `registerConstantObj` with the type name.
13. **Thread safety:** `eval` parses and evaluates with no shared lock during the parse. The table of constants is under `std::shared_mutex` (a read takes a shared lock, `register*`/`unregister` take an exclusive lock). The nesting depth is limited (256 levels): an error replaces a stack overflow.

## Review Focus

1. **JSON → expression:** Any value that `nlohmann::json::dump()` gives for numbers, bool, null, strings with control characters and non-ASCII characters, and nested arrays/objects evaluates back to the same value. The test `test_eval_json_superset` (Task 6).
2. **Unknown names throw:** `int32`, `hello`, `float32` (without quotes) → `Pothos::Exception`, not a value. `Topology::make` depends on this. The test `test_eval_unknown_name_throws` (Task 6).
3. **An int64 overflow** in a literal and in arithmetic gives an error, not UB (the check is before the operation, with no signed overflow). The test `test_expr_int_overflow` (Task 3).
4. **Deep nesting** (`((((...))))` × 10000, `[[[[...]]]]`, `-------...1`) gives an exception, not a crash. The test `test_expr_depth_limit` (Task 2).
5. **Parallel `eval` and `registerConstant*`** on one environment from several threads: no races (check under TSan locally, if available, otherwise logically). The test `test_eval_concurrent` (Task 6).

---

### Task 1: Value and lexer

**Files:**
- Create: `lib/Util/Expr/Value.hpp`, `lib/Util/Expr/Value.cpp`, `lib/Util/Expr/Lexer.hpp`, `lib/Util/Expr/Lexer.cpp`, `lib/Util/Builtin/TestExpr.cpp`
- Modify: `lib/CMakeLists.txt`

**Interfaces (namespace `Pothos::Util::Expr`):**
- `struct Value;` — a wrapper over `std::variant<std::monostate, bool, std::int64_t, double, std::complex<double>, std::string, List, Dict>`, where `using List = std::vector<Value>; using Dict = std::vector<std::pair<Value, Value>>;` (insertion order, a search by `==`). Methods: `typeName()` (`"None"`, `"bool"`, `"int"`, …), `bool truthy() const`, `bool operator==(const Value&) const` (numbers compare by value through the tower: `1 == 1.0 == True`), `std::string repr() const` (Python-like, for messages and tests: `[1, 2.5, 'a', None]`).
- `class ExprError : public std::runtime_error` with the field `std::size_t pos` (the offset in the expression, 0-based) — an internal exception. It is translated to `Pothos::Exception` at the border (Task 6).
- `enum class Tok { Int, Float, Imag, String, Name, Op, End };` `struct Token { Tok kind; std::string_view text; std::size_t pos; Value literal; };`
- `std::vector<Token> tokenize(std::string_view expr);` — skips whitespace. The operators (the longest match) are `** // << >> <= >= == != + - * / % & | ^ ~ < > ( ) [ ] { } , :`. The keywords (`and or not if else`) are returned as `Name`. The literals are parsed into `literal` by decisions 2 and 9. An error is an `ExprError` with the position of the character.

- [ ] **Step 1: The tests in `TestExpr.cpp`** (`POTHOS_TEST_BLOCK("/util/tests", ...)`, the file in `lib/CMakeLists.txt`):
  - `test_expr_lex_numbers`: `"42"`→Int 42; `"0x1F"`, `"0o17"`, `"0b101"`, `"1_000"`; `"1.5"`, `".5"`, `"1."`, `"1e-05"`, `"2E3"` → Float; `"3j"`, `"1.5J"` → Imag; `"9223372036854775807"` ok, `"9223372036854775808"` → ExprError; `"0315"` → ExprError; `"0x"` → ExprError.
  - `test_expr_lex_strings`: `R"("a\"b")"`→`a"b`; `R"('it\'s')"`; `R"("é")`→`"\xc3\xa9"`; `R"("😀")`→4 bytes of UTF-8; `R"("\/\n\t")"`; `R"("abc)"`(not closed) → ExprError pos 0; `R"("\q")"` → ExprError.
  - `test_expr_lex_ops`: `"a**-b//c"` → `a`, `**`, `-`, `b`, `//`, `c`, End; the positions of the tokens are correct; `"$"` → ExprError with pos 0.
  - `test_expr_value_basics`: `Value(1) == Value(1.0)`, `Value(true) == Value(1)`, `repr` of a list/dict, `truthy` for empty containers.
- [ ] **Step 2: Run** the build → Expected: FAIL (no headers).
- [ ] **Step 3: Implement `Value` and `tokenize`** (numbers use `std::from_chars`; for int, with a range check; for float, `std::from_chars` after the removal of `_`).
- [ ] **Step 4: Run** the single self-tests `test_expr_lex_*`, `test_expr_value_basics` → PASS; the full `ctest` → all PASS.
- [ ] **Step 5: Commit** `feat(expr): value type and tokenizer for the new expression evaluator`

---

### Task 2: Parser and AST

**Files:**
- Create: `lib/Util/Expr/Parser.hpp`, `lib/Util/Expr/Parser.cpp`
- Modify: `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:**
- `struct Node;` with `kind` (`Literal`, `Name`, `Unary`, `Binary`, `Compare` (a chain), `And`, `Or`, `Not`, `IfElse`, `List`, `Dict`, `Call`, `Index`), `std::string op`, `std::vector<std::unique_ptr<Node>> children`, `std::size_t pos`, `Value literal`, `std::string name`.
- `std::unique_ptr<Node> parse(std::string_view expr);` — the whole input must be one expression (extra tokens → an error "unexpected 'x'" with the position).
- `std::string dump(const Node&);` — an S-expression for the tests: `(+ 1 (* 2 3))`.

The grammar (from the bottom to the top by precedence, as in Python):
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
A call is valid only for a name (`Call` keeps the function name). A counter of the recursion depth gives an error at > 256.

- [ ] **Step 1: The tests:**
  - `test_expr_parse_precedence`: `dump(parse("1 + 2 * 3"))=="(+ 1 (* 2 3))"`; `"-2**2"`→`(neg (** 2 2))`; `"2**3**2"`→`(** 2 (** 3 2))`; `"a if b else c if d else e"`; `"not a == b"`→`(not (== a b))`; `"0 < x < 10"`→`(cmp 0 < x < 10)`; `"1 | 2 ^ 3 & 4 << 1"`.
  - `test_expr_parse_containers`: `"[]"`, `"[1, ]"`, `"{}"`, `"{1: 2, }"`, `"[1, [2, 3]]"`, `"f(1, 2,)"`, `"a[0][1]"`, `"{'a': [1, {'b': 2}]}"`.
  - `test_expr_parse_errors` (the test checks `pos`): `"1 +"` (pos 3, "unexpected end"), `"(1"`, `"[1 2]"` (pos 3), `"{1}"`, `"1 2"`, `""` (empty), `"f(1)(2)"`, `"1 if 2"`.
  - `test_expr_depth_limit`: 10000 `(` + `1` + 10000 `)`, 10000 `[`…, 10000 `-` + `1` → ExprError, no crash.
- [ ] **Step 2: Run** → FAIL (no parser).
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the self-tests `test_expr_parse_*`, `test_expr_depth_limit` → PASS; the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): recursive-descent parser with Python precedence`

---

### Task 3: Evaluator — scalars, operators, logic

**Files:**
- Create: `lib/Util/Expr/Eval.hpp`, `lib/Util/Expr/Eval.cpp`, `lib/Util/Expr/Ops.cpp` (arithmetic and checked integer operations)
- Modify: `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:**
- `using Lookup = std::function<const Value *(std::string_view)>;` — the user constants (nullptr means not found; then the built-in names, then the error `unknown name 'x'`).
- `Value evaluate(const Node &, const Lookup &);`
- `Value evaluate(std::string_view expr, const Lookup &);` — parse + evaluate.
- Inside `Ops.cpp`: `Value binaryOp(std::string_view op, const Value&, const Value&, std::size_t pos);`, `Value unaryOp(...)`, `int compare(...)`. For integers, the overflow check is before the operation (`std::numeric_limits`, no `__builtin_*` — MSVC).

- [ ] **Step 1: The tests** (`evaluate(...)` is compared through `Value ==` and a check of `typeName()`):
  - `test_expr_arith`: `1+2*3`=7 int; `7/2`=3.5 float; `7//2`=3; `-7//2`=-4; `-7%3`=2; `7.5//2`=3.0; `2**10`=1024 int; `2**-1`=0.5; `(-8)**(1/3)` → complex; `1+2j`+`1` → complex(2,2); `True+True`=2 int; `"ab"+"cd"`; `1/0.0`=inf; `1/0` → an error with the pos of the operator; `5%0` → an error.
  - `test_expr_int_overflow`: `9223372036854775807+1`, `-9223372036854775807-2`, `2**63`, `3037000500*3037000500`, `1<<64`, `-(-9223372036854775807-1)`, `(-9223372036854775807-1)//-1`, `2**62*2` → an error; `1<<62`, `-9223372036854775807-1` → ok.
  - `test_expr_bitwise`: `6&3`=2, `6|3`=7, `6^3`=5, `~5`=-6, `1<<4`=16, `-16>>2`=-4; `1.0&1` → an error "unsupported operand types".
  - `test_expr_compare_logic`: `1<2<3` True, `3>2>2` False, `1==1.0` True, `"a"<"b"` True, `1<"a"` → an error, `1=="a"` False, `0 or 5`=5, `2 and 0`=0, `not []`=True, `0 and undefined_name`=0 (the short-circuit does not touch the right part), `1 if 0 else 2`=2, `1+2j < 3` → an error.
  - `test_expr_names`: `x+1` with Lookup `x=41` → 42; `foo` without Lookup → the error `unknown name 'foo'` with pos; `True`, `true`, `None`, `null`, `pi`, `j*j`=-1+0j; a user `j=5` overrides the built-in one.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** (the containers and functions throw "not implemented" for now — Task 4/5 close them).
- [ ] **Step 4: Run** the self-tests → PASS; the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): evaluate scalars, operators, comparisons and logic`

---

### Task 4: Containers and element-wise operations

**Files:**
- Modify: `lib/Util/Expr/Eval.cpp`, `lib/Util/Expr/Ops.cpp`, `lib/Util/Builtin/TestExpr.cpp`

- [ ] **Step 1: The tests:**
  - `test_expr_containers`: `[]`, `[1, [2, 'x'], 3]`, `{}`, `{'a': 1, 'b': [1, 2]}`, `{1: 2, 1: 3}` → `{1: 3}`; `{[1]: 2}` → an error "unhashable"; `[1,2,3][-1]`=3; `[1][5]` → an error with the pos of the index; `{'a': 1}['a']`=1; `{'a': 1}['b']` → the error `key 'b' not found`; `"abc"[1]`=`"b"`; `5[0]` → an error.
  - `test_expr_elementwise`: `(2 * [1, 2, 3]) + [3, 2, 1]` = `[5, 6, 7]`; `[1, 2] + [3, 4]`=`[4, 6]`; `[1, 2] + [1]` → a length error; `-[1, [2, 3]]` = `[-1, [-2, -3]]`; `[1, 2] / 2` = `[0.5, 1.0]`; `[1,2] == [1,2]` True; `[1,2] < [3,4]` → an error; `{'a': 1} + 1` → an error; `['a', 'b'] + 'c'` = `['ac', 'bc']`.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the self-tests → PASS; the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): lists, dicts, indexing and element-wise operators`

---

### Task 5: Built-in functions

**Files:**
- Create: `lib/Util/Expr/Builtins.cpp`
- Modify: `lib/Util/Expr/Eval.cpp`, `lib/Util/Builtin/TestExpr.cpp`, `lib/CMakeLists.txt`

**Interfaces:** A table `std::string_view → function(std::span<const Value> args, std::size_t pos) -> Value` with a check of the number of arguments (the error `sin() takes 1 argument (2 given)`). An unknown function → `unknown function 'foo'`. A call of a non-function (`pi(1)`) → an error.

- [ ] **Step 1: The tests:**
  - `test_expr_math`: `pow(2, 3)`=8 (int), `sqrt(4)`=2.0, `sqrt(-1)` → nan, `sqrt(-1+0j)`=1j, `log2(8)`=3.0, `log(e)`=1.0, `log(8, 2)`=3.0, `ln(1)`=0.0, `atan2(1, 1)`=pi/4, `abs(-3)`=3 int, `abs(3+4j)`=5.0, `round(2.5)`=2 (banker's rounding, as Python), `round(2.567, 2)`=2.57, `floor(-1.5)`=-2 int, `ceil(1.2)`=2 int, `real(1+2j)`=1.0, `imag(1+2j)`=2.0, `conj(1+2j)`, `arg(1j)`=pi/2, `sin([0, pi/2])` ≈ `[0, 1]`.
  - `test_expr_builtins`: `min(3, 1, 2)`=1, `max([1, 5, 2])`=5, `min([])` → an error, `sum([1, 2.5])`=3.5, `len([1, 2])`=2, `len('abc')`=3, `len({})`=0, `int(3.9)`=3, `int('42')`=42, `float('1e3')`=1000.0, `complex(1, 2)`, `str(1.5)`=`'1.5'`, `bool([])`=False, `range(3)`=`[0, 1, 2]`, `range(1, 7, 2)`=`[1, 3, 5]`, `range(0, 1, 0)` → an error, `range(10000000)` → an error (a length limit of 1 000 000), `sin(1, 2)` → an error about the number of arguments, `foo(1)` → `unknown function 'foo'`.
  - A user constant `sin=1` does not break the call `sin(0)` (functions and constants are different namespaces).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the self-tests → PASS; the full `ctest` → PASS.
- [ ] **Step 5: Commit** `feat(expr): built-in math, conversion and container functions`

---

### Task 6: `EvalEnvironment` on the new evaluator

**Files:**
- Create: `lib/Util/Expr/Convert.hpp`, `lib/Util/Expr/Convert.cpp` (`Value ↔ Pothos::Object`)
- Modify: `include/Pothos/Util/EvalEnvironment.hpp` (remove `splitExpr`, `_evalList`, `_evalMap`; the documentation of the syntax goes in the Doxygen comment of the class — a short grammar, the types, the differences from Python), `lib/Util/EvalEnvironment.cpp` (rewrite), `lib/Util/Builtin/TestEvalExpression.cpp`, `lib/CMakeLists.txt`
- Delete: `lib/Util/EvalEnvironmentListParsers.cpp`, `lib/Util/Builtin/MupOctalValReader.cpp`, `lib/Util/Builtin/MupOctalValReader.hpp`

**Interfaces:**
- `Pothos::Object toObject(const Value &);` and `Value fromObject(const Pothos::Object &);` — by decisions 11–12.
- `EvalEnvironment::Impl`: `std::shared_mutex mutex; std::map<std::string, Value, std::less<>> constants;`. `eval` is `Expr::evaluate` with a `Lookup` over `constants` under a shared lock. `ExprError` → `Pothos::Exception("EvalEnvironment::eval", "<msg> at column N in: <expr>")`. `registerConstantExpr(key, expr)` evaluates first, then writes under an exclusive lock (the error contains the name of the key). The code checks the name of the constant: an identifier, not a keyword (`and`, `if`, …) — otherwise an error.

- [ ] **Step 1: Update/add the tests in `TestEvalExpression.cpp`** (the existing tests stay — their syntax is already compatible; `test_eval_octal_constant` passes with `0o315`):
  - `test_eval_json_superset`: for a set of `nlohmann::json` values (`-3`, `1e-05`, `true`, `null`, `"tab\tquote\"é"`, `"\u0001"`, `[1, [2.5, "x"], {}]`, `{"a": {"b": [true, null]}}`, `1.7976931348623157e308`) — `eval(j.dump())`, a comparison of the result with the source value (numbers/strings/nesting; `null` → an empty Object).
  - `test_eval_unknown_name_throws`: `int32`, `hello`, `float32`, `complex_float32` → `Pothos::Exception`. The message contains `unknown name` and `column`.
  - `test_eval_topology_doc_example`: `registerConstantExpr("fiz", "3")`, `eval("-fiz") == -3` (the example from `Topology.hpp`).
  - `test_eval_object_roundtrip`: `registerConstantObj` for `std::vector<std::string>`, `ProxyMap`, `uint64_t(UINT64_MAX)` (→ float), `std::complex<float>`, an empty `Object()` (→ `None`: `eval("x == None")` → True; there is no `is` operator). An unsupported type (`Pothos::DType`) → an exception with the type name.
  - `test_eval_constant_name_rules`: `registerConstantExpr("if", "1")` → an exception; `unregisterConstant("missing")` — no error; a second registration replaces the value.
  - `test_eval_concurrent`: 8 threads × 2000 iterations: `eval("x * [1, 2, 3]")` and `registerConstantExpr("x", "<i>")` on one `EvalEnvironment`. No exception, and the result is always a list of length 3.
  - `test_eval_error_message`: `eval("1 + * 2")` → the message contains `column 5` and the source expression.
- [ ] **Step 2: Run** → FAIL (the new tests; muparserx is still connected, the old code compiles).
- [ ] **Step 3: Rewrite `EvalEnvironment.cpp`**, delete the hacks and `MupOctalValReader`. Remove `pothos_muparserx` from `target_link_libraries` in `lib/CMakeLists.txt` (the submodule and `third_party` go in Task 7).
- [ ] **Step 4: Check the callers in the core** — `lib/Framework/TopologyMakeJSON.cpp`, `lib/Util/Builtin/BlockEval.cpp` (the behavior "a string that failed to evaluate is a literal" stays; you do not need to change the code if the topology tests PASS). Run the full `ctest` → all PASS (also `/framework/tests/test_topology_*`).
- [ ] **Step 5: Commit** `feat: EvalEnvironment on the built-in expression evaluator, drop muparserx usage`

---

### Task 7: Removal of muparserx

**Files:**
- Modify: `third_party/CMakeLists.txt` (remove `pothos_muparserx`), `.gitmodules` and the index (`git rm muparserx`), `.github/workflows/ci.yml` (`git submodule update --init poco libsimdpp`), `tests/consumer/CMakeLists.txt` (the regex: `Poco|nlohmann|simdpp`), `lib/CMakeLists.txt` (the comment about transitional deps), `README.md` (the Dependencies section, the command that initializes the submodules), `CLAUDE.md` (the submodules of the core, the rule about transitional deps, the section about the evaluator: where the code is, the syntax — a link to the Doxygen of `EvalEnvironment.hpp`), `Changelog.txt` (an entry for pothos2: the new syntax of expressions, `^` is now XOR, `/` is always float)

- [ ] **Step 1: RED:** `grep -rniI 'muparser\|mup::' --exclude-dir=build --exclude-dir=.cache --exclude-dir=docs --exclude-dir={poco,flow,blocks,muparserx} .` → finds references (record the number).
- [ ] **Step 2: Delete the submodule and the references** (`git submodule deinit -f muparserx && git rm muparserx`; keep the directory `.git/modules/muparserx` — do not touch the shared git storage).
- [ ] **Step 3: A clean configuration:** `cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-clean && ctest --test-dir build-clean --output-on-failure` → all PASS. The second grep from Step 1 → empty (except the historical lines of `Changelog.txt`). Delete `build-clean`.
- [ ] **Step 4: Commit** `build: remove the muparserx submodule`

---

### Task 8: Result of the phase

**Files:**
- Modify: `docs/superpowers/plans/2026-10-26-core-modernization-roadmap.md` (status: phase 2 is done, the next one is phase 4), the ledger `docs/superpowers/ledgers/2026-10-27-phase2-expression-evaluator.md`

- [ ] **Step 1: A build with Clang** (`-DCMAKE_CXX_COMPILER=clang++`, a separate directory) and the full `ctest` → PASS, with no new warnings in `lib/Util/Expr/`.
- [ ] **Step 2: Update the roadmap and the ledger.**
- [ ] **Step 3: Commit** `docs: phase 2 complete — built-in expression evaluator`
- [ ] **Step 4: A fresh review of the whole branch** (one reviewer, the focus is the section Review Focus). Make the fixes in separate commits.
- [ ] **Step 5: With the consent of the user, push the branch `phase2-expression-evaluator` and make a PR to `pothos2`. Wait for the CI on all four jobs.**

## Out of the phase

- The submodules `flow` and `blocks` (they are not in the core build) use `EvalEnvironment` only through the public API. Their default expressions (`log2(val)`, `[1, 1, -1]`, `0x55`, `1e6`, `"float64"`) are compatible with the new syntax. You do not need to adapt the saved `.pothos` files (spec).
- The commented example `DType("int32")` in the test is not restored: a call of arbitrary Pothos functions from expressions is not a goal of the phase.
