# SDD ledger — plan: docs/superpowers/plans/2026-10-27-phase2-expression-evaluator.md
Pre-flight: T1 Value/Token → T2 parser (Node.literal Value) ok; T2 parse/Node → T3 evaluate ok; T3 Lookup/evaluate → T6 EvalEnvironment ok; T5 builtins table → T3 Call node (name) ok; T4/T5 extend T3 Eval.cpp/Ops.cpp ok. No conflicts.
Task 1: complete (commits e256da3..c52ee9a, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.15 sec)
Task 2: Ruling: added Node::ops (Compare operators) and Node::depth; limits split into nesting 256 (parens/containers/prefix ops/**) and tree height 1024 (left-assoc chains 1+1+...) — plan names one limit of 256; a flat 300-term sum stays valid — cost if wrong: change two constants
Task 2: complete (commits c52ee9a..d579cd4, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.32 sec)
Task 3: Ruling: added lib/Util/Expr/Ops.hpp (declares binaryOp/unaryOp/compareOp; plan listed only Ops.cpp and a compare() returning int) — compareOp returns bool so nan stays unordered — cost if wrong: none (internal)
Task 3: complete (commits d579cd4..22ca416, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.81 sec)
Task 4: complete (commits 22ca416..4af5d8c, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.75 sec)
Task 5: Ruling: added Builtins.hpp (isBuiltinFunction/callBuiltin) and exported Lexer parseDecimalFloat for float('...'); round(int, n<0) rounds half-even like Python; real()/imag() always return float — plan only listed Builtins.cpp — cost if wrong: none (internal)
Task 5: complete (commits 4af5d8c..cb59f02, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.32 sec)
Task 6: Ruling: JSON dump of INT64_MIN (-9223372036854775808) failed — the lexer marks the literal 2**63 (Token::needsMinus) and the parser folds it only directly after a unary minus (not before **); anywhere else it is 'integer literal is too large' — required by the JSON-superset constraint — cost if wrong: small special case in lexer/parser
Task 6: Ruling: registerConstantObj also validates the key as an identifier (plan only named registerConstantExpr) — both register constants usable in expressions — cost if wrong: a caller registering a non-identifier key now gets an exception instead of an unusable constant
Task 6: complete (commits cb59f02..9878f2a, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.57 sec)
Task 7: Ruling: also dropped libmuparserx-dev from debian/control (not listed in the plan) and added a 'pothos2 (in development)' section to Changelog.txt (none existed); submodule.muparserx entries in the shared .git/config and .git/modules left untouched — cost if wrong: none
Task 7: complete (commits 9878f2a..efeba6d, tests: ctest --test-dir build --output-on-failure → Total Test time (real) =  12.77 sec)
Task 8: clang++ build-clang: ctest 5/5 pass, no warnings in lib/Util/Expr, EvalEnvironment.cpp, TestExpr.cpp
Final review: opus reviewer, verdict "With fixes" (0 Critical, 2 Important, 8 Minor)
Final: fixed I-1 stack overflow on small thread stacks — test_expr_depth_small_stack (256 KiB thread) RED (SIGSEGV) → GREEN; parser uses precedence climbing + one and/or function, MaxNesting 256→100, MaxTreeDepth 1024→256 (now in Parser.hpp); suite 5/5
Final: fixed I-2 libc++ stream fallback turned subnormals into 0 / overflow into DBL_MAX — test_expr_float_stream_fallback RED → GREEN (parseFloatStream decides from the stored value); suite 5/5
Final: M-3 decided by the user (2026-10-27): decimal integer literals beyond int64 become float (bare 2**63 too; -9223372036854775808 stays int) — test_eval_json_uint64 and lexer/overflow tests RED → GREEN, suite 5/5
Final: minor (deferred): M-1 from_chars out-of-range direction is decided by the exponent sign, not the magnitude ("0.000…1" with 400 zeros → inf)
Final: minor (deferred): M-2 Value nesting depth is unbounded for values built via constants (registerConstant* chains, deep ProxyVector)
Final: minor (deferred): M-4 user constants may shadow true/false/null/True/False/None
Final: minor (deferred): M-5 round(int, -19) returns 0 instead of overflow
Final: minor (deferred): M-6 strings are byte strings: "\xe9" is one byte, len("é") == 2
Final: minor (deferred): M-7 str()/repr() of floats use the shortest std::format form ('1e+05', Python '100000.0')
Final: minor (deferred): M-8 only ExprError is translated to Pothos::Exception (bad_alloc/length_error escape)
CI #1 on PR #2: Linux green; macOS (Debug) and Windows (Release) crashed in test_expr_depth_small_stack (256 KiB thread), macOS also failed the exact sqrt(-1+0j) == 1j check (libc++ returns a tiny nonzero real part)
CI: Ruling: nesting counters cannot bound the stack across compilers — added StackGuard (lib/Util/Expr/Stack.hpp): parser Nest and every evaluator step throw "nested too deeply" past a 128 KiB budget measured from the entry of parse/evaluate; counters stay as a second limit. GCC reaches ~75 nesting levels within the budget (-O0 and -O2); the small-stack test now runs on a budget+64 KiB thread, requires 25 levels everywhere and only "evaluates or fails cleanly" at MaxNesting — cost if wrong: deep expressions (>~40 levels on MSVC/Apple) get an error instead of a value
CI: sqrt(-1+0j) test compares with a 1e-15 tolerance
