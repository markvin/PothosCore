// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Self tests for the internal expression evaluator (lib/Util/Expr)

#include <Pothos/Testing.hpp>
#include "Util/Expr/Value.hpp"
#include "Util/Expr/Lexer.hpp"
#include "Util/Expr/Parser.hpp"
#include "Util/Expr/Eval.hpp"
#include "Util/Expr/Stack.hpp"
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <complex>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

using namespace Pothos::Util::Expr;

namespace {

//! Position of the ExprError thrown by f, or throws when f does not fail
template <typename Fn>
std::size_t errorPos(Fn &&f)
{
    try
    {
        f();
    }
    catch (const ExprError &ex)
    {
        return ex.pos;
    }
    throw std::runtime_error("no ExprError thrown");
}

Token lexOne(const std::string &expr)
{
    const auto toks = tokenize(expr);
    if (toks.size() != 2 or toks.back().kind != Tok::End) throw std::runtime_error("expected one token: " + expr);
    return toks.front();
}

std::string lexString(const std::string &expr)
{
    const auto tok = lexOne(expr);
    if (tok.kind != Tok::String) throw std::runtime_error("not a string token: " + expr);
    return std::get<std::string>(tok.literal.data);
}

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_expr_lex_numbers)
{
    auto tok = lexOne("42");
    POTHOS_TEST_TRUE(tok.kind == Tok::Int);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(tok.literal.data), 42);

    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("0x1F").literal.data), 31);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("0X1f").literal.data), 31);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("0o17").literal.data), 15);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("0b101").literal.data), 5);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("1_000").literal.data), 1000);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("0").literal.data), 0);
    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("00").literal.data), 0);

    for (const auto &[text, value] : std::vector<std::pair<std::string, double>>{
        {"1.5", 1.5}, {".5", 0.5}, {"1.", 1.0}, {"1e-05", 1e-05}, {"2E3", 2000.0},
        {"0315.0", 315.0}, {"1_0.2_5", 10.25}, {"1.7976931348623157e308", 1.7976931348623157e308}})
    {
        tok = lexOne(text);
        POTHOS_TEST_TRUE(tok.kind == Tok::Float);
        POTHOS_TEST_EQUAL(std::get<double>(tok.literal.data), value);
    }

    tok = lexOne("3j");
    POTHOS_TEST_TRUE(tok.kind == Tok::Imag);
    POTHOS_TEST_TRUE(std::get<std::complex<double>>(tok.literal.data) == std::complex<double>(0.0, 3.0));
    tok = lexOne("1.5J");
    POTHOS_TEST_TRUE(tok.kind == Tok::Imag);
    POTHOS_TEST_TRUE(std::get<std::complex<double>>(tok.literal.data) == std::complex<double>(0.0, 1.5));

    POTHOS_TEST_EQUAL(std::get<std::int64_t>(lexOne("9223372036854775807").literal.data), INT64_MAX);
    //2**63 only exists as the operand of a unary minus: the lexer marks it, the parser checks
    tok = lexOne("9223372036854775808");
    POTHOS_TEST_TRUE(tok.kind == Tok::Int and tok.needsMinus);
    POTHOS_TEST_TRUE(not lexOne("9223372036854775807").needsMinus);
    //larger decimal literals (JSON uint64) become float
    tok = lexOne("18446744073709551615");
    POTHOS_TEST_TRUE(tok.kind == Tok::Float);
    POTHOS_TEST_EQUAL(std::get<double>(tok.literal.data), 18446744073709551615.0);
    POTHOS_TEST_TRUE(lexOne("1" + std::string(400, '0')).kind == Tok::Float);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("0x10000000000000000");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("0x10000000000000000");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("1 + 0315");}), 4);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("0x");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("0b102");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("12abc");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("1__0");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("10_");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("1e");}), 0);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_lex_strings)
{
    POTHOS_TEST_EQUAL(lexString(R"("a\"b")"), "a\"b");
    POTHOS_TEST_EQUAL(lexString(R"('it\'s')"), "it's");
    POTHOS_TEST_EQUAL(lexString(R"('say "hi"')"), "say \"hi\"");
    POTHOS_TEST_EQUAL(lexString(R"("é")"), "\xc3\xa9");
    POTHOS_TEST_EQUAL(lexString("\"\xc3\xa9\""), "\xc3\xa9");
    POTHOS_TEST_EQUAL(lexString(R"("😀")"), "\xf0\x9f\x98\x80");
    POTHOS_TEST_EQUAL(lexString(R"("\/\n\t\r\b\f\\")"), "/\n\t\r\b\f\\");
    POTHOS_TEST_EQUAL(lexString(R"("\x41\u0001")"), "A\x01");
    POTHOS_TEST_EQUAL(lexString(R"("a\0b")"), std::string("a\0b", 3));
    POTHOS_TEST_EQUAL(lexString(R"("")"), "");

    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"("abc)");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"(1 + 'abc)");}), 4);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"("a\q")");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"("\u12")");}), 1);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"("\xZZ")");}), 1);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize(R"("\ud83d")");}), 1);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_lex_ops)
{
    const auto toks = tokenize("a**-b//c");
    const std::vector<std::string> texts{"a", "**", "-", "b", "//", "c"};
    const std::vector<std::size_t> positions{0, 1, 3, 4, 5, 7};
    POTHOS_TEST_EQUAL(toks.size(), texts.size()+1);
    for (std::size_t i = 0; i < texts.size(); i++)
    {
        POTHOS_TEST_EQUAL(std::string(toks[i].text), texts[i]);
        POTHOS_TEST_EQUAL(toks[i].pos, positions[i]);
    }
    POTHOS_TEST_TRUE(toks[0].kind == Tok::Name);
    POTHOS_TEST_TRUE(toks[1].kind == Tok::Op);
    POTHOS_TEST_TRUE(toks.back().kind == Tok::End);
    POTHOS_TEST_EQUAL(toks.back().pos, 8);

    const auto ops = tokenize("<<>>>=<===!=&|^~%()[]{},:");
    const std::vector<std::string> opTexts{"<<", ">>", ">=", "<=", "==", "!=", "&", "|", "^", "~", "%", "(", ")", "[", "]", "{", "}", ",", ":"};
    POTHOS_TEST_EQUAL(ops.size(), opTexts.size()+1);
    for (std::size_t i = 0; i < opTexts.size(); i++) POTHOS_TEST_EQUAL(std::string(ops[i].text), opTexts[i]);

    const auto kw = tokenize("not  _x1\tand\n");
    POTHOS_TEST_EQUAL(kw.size(), 4);
    POTHOS_TEST_TRUE(kw[0].kind == Tok::Name);
    POTHOS_TEST_EQUAL(std::string(kw[1].text), "_x1");
    POTHOS_TEST_EQUAL(kw[2].pos, 9);

    POTHOS_TEST_EQUAL(errorPos([]{tokenize("$");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("1 ! 2");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("a = 1");}), 2);
    POTHOS_TEST_EQUAL(tokenize("").size(), 1);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_value_basics)
{
    POTHOS_TEST_TRUE(Value(1) == Value(1.0));
    POTHOS_TEST_TRUE(Value(true) == Value(1));
    POTHOS_TEST_TRUE(Value(1.0) == Value(std::complex<double>(1.0, 0.0)));
    POTHOS_TEST_TRUE(not (Value(1) == Value(1.5)));
    POTHOS_TEST_TRUE(not (Value(1) == Value("1")));
    POTHOS_TEST_TRUE(not (Value(INT64_MAX) == Value(9223372036854775808.0)));
    POTHOS_TEST_TRUE(Value() == Value());
    POTHOS_TEST_TRUE(not (Value() == Value(0)));
    POTHOS_TEST_TRUE(Value(List{Value(1), Value("a")}) == Value(List{Value(1.0), Value("a")}));
    POTHOS_TEST_TRUE(Value(Dict{{Value("a"), Value(1)}, {Value("b"), Value(2)}}) ==
        Value(Dict{{Value("b"), Value(2)}, {Value("a"), Value(1)}}));

    POTHOS_TEST_EQUAL(Value().typeName(), "None");
    POTHOS_TEST_EQUAL(Value(false).typeName(), "bool");
    POTHOS_TEST_EQUAL(Value(3).typeName(), "int");
    POTHOS_TEST_EQUAL(Value(3.0).typeName(), "float");
    POTHOS_TEST_EQUAL(Value(std::complex<double>()).typeName(), "complex");
    POTHOS_TEST_EQUAL(Value("x").typeName(), "str");
    POTHOS_TEST_EQUAL(Value(List{}).typeName(), "list");
    POTHOS_TEST_EQUAL(Value(Dict{}).typeName(), "dict");

    POTHOS_TEST_EQUAL(Value(List{Value(1), Value(2.5), Value("a'b"), Value(), Value(true)}).repr(),
        R"([1, 2.5, 'a\'b', None, True])");
    POTHOS_TEST_EQUAL(Value(Dict{{Value("k"), Value(List{Value(1.0)})}}).repr(), "{'k': [1.0]}");
    POTHOS_TEST_EQUAL(Value(std::complex<double>(1.0, -2.0)).repr(), "(1-2j)");
    POTHOS_TEST_EQUAL(Value(std::complex<double>(0.0, 2.0)).repr(), "2j");
    POTHOS_TEST_EQUAL(Value(1e100).repr(), "1e+100");
    POTHOS_TEST_EQUAL(Value("\n").repr(), R"('\n')");

    POTHOS_TEST_TRUE(not Value().truthy());
    POTHOS_TEST_TRUE(not Value(0).truthy());
    POTHOS_TEST_TRUE(not Value(0.0).truthy());
    POTHOS_TEST_TRUE(not Value("").truthy());
    POTHOS_TEST_TRUE(not Value(List{}).truthy());
    POTHOS_TEST_TRUE(not Value(Dict{}).truthy());
    POTHOS_TEST_TRUE(Value(List{Value()}).truthy());
    POTHOS_TEST_TRUE(Value(std::complex<double>(0.0, 1.0)).truthy());
    POTHOS_TEST_TRUE(Value("0").truthy());
}

namespace {

std::string parseDump(const std::string &expr)
{
    return dump(*parse(expr));
}

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_expr_parse_precedence)
{
    POTHOS_TEST_EQUAL(parseDump("1 + 2 * 3"), "(+ 1 (* 2 3))");
    POTHOS_TEST_EQUAL(parseDump("1 - 2 - 3"), "(- (- 1 2) 3)");
    POTHOS_TEST_EQUAL(parseDump("(1 + 2) * 3"), "(* (+ 1 2) 3)");
    POTHOS_TEST_EQUAL(parseDump("-2**2"), "(neg (** 2 2))");
    POTHOS_TEST_EQUAL(parseDump("2**-1"), "(** 2 (neg 1))");
    POTHOS_TEST_EQUAL(parseDump("2**3**2"), "(** 2 (** 3 2))");
    POTHOS_TEST_EQUAL(parseDump("+~x"), "(pos (inv x))");
    POTHOS_TEST_EQUAL(parseDump("a // b % c / d"), "(/ (% (// a b) c) d)");
    POTHOS_TEST_EQUAL(parseDump("a if b else c if d else e"), "(if b a (if d c e))");
    POTHOS_TEST_EQUAL(parseDump("not a == b"), "(not (== a b))");
    POTHOS_TEST_EQUAL(parseDump("0 < x < 10"), "(cmp 0 < x < 10)");
    POTHOS_TEST_EQUAL(parseDump("a != b"), "(!= a b)");
    POTHOS_TEST_EQUAL(parseDump("a or b and not c or d"), "(or a (and b (not c)) d)");
    POTHOS_TEST_EQUAL(parseDump("1 | 2 ^ 3 & 4 << 1"), "(| 1 (^ 2 (& 3 (<< 4 1))))");
    POTHOS_TEST_EQUAL(parseDump("a >> 1 + 2"), "(>> a (+ 1 2))");
    POTHOS_TEST_EQUAL(parseDump("'s' + 1.5 + 2j"), "(+ (+ 's' 1.5) 2j)");
    POTHOS_TEST_EQUAL(parseDump("-a[0]**2"), "(neg (** (index a 0) 2))");
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_parse_containers)
{
    POTHOS_TEST_EQUAL(parseDump("[]"), "(list)");
    POTHOS_TEST_EQUAL(parseDump("[1, ]"), "(list 1)");
    POTHOS_TEST_EQUAL(parseDump("{}"), "(dict)");
    POTHOS_TEST_EQUAL(parseDump("{1: 2, }"), "(dict 1 2)");
    POTHOS_TEST_EQUAL(parseDump("[1, [2, 3]]"), "(list 1 (list 2 3))");
    POTHOS_TEST_EQUAL(parseDump("f(1, 2,)"), "(call f 1 2)");
    POTHOS_TEST_EQUAL(parseDump("f()"), "(call f)");
    POTHOS_TEST_EQUAL(parseDump("a[0][1]"), "(index (index a 0) 1)");
    POTHOS_TEST_EQUAL(parseDump("{'a': [1, {'b': 2}]}"), "(dict 'a' (list 1 (dict 'b' 2)))");
    POTHOS_TEST_EQUAL(parseDump("f(x)[0]"), "(index (call f x) 0)");
    POTHOS_TEST_EQUAL(parseDump("[1 if a else 2, b]"), "(list (if a 1 2) b)");
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_parse_errors)
{
    POTHOS_TEST_EQUAL(errorPos([]{parse("1 +");}), 3);
    POTHOS_TEST_EQUAL(errorPos([]{parse("(1");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{parse("[1 2]");}), 3);
    POTHOS_TEST_EQUAL(errorPos([]{parse("{1}");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{parse("{1: 2 3: 4}");}), 6);
    POTHOS_TEST_EQUAL(errorPos([]{parse("1 2");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{parse("");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{parse("   ");}), 3);
    POTHOS_TEST_EQUAL(errorPos([]{parse("f(1)(2)");}), 4);
    POTHOS_TEST_EQUAL(errorPos([]{parse("1 if 2");}), 6);
    POTHOS_TEST_EQUAL(errorPos([]{parse("a and");}), 5);
    POTHOS_TEST_EQUAL(errorPos([]{parse("if + 1");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{parse("1 + )");}), 4);
    POTHOS_TEST_EQUAL(errorPos([]{parse("[,]");}), 1);
    POTHOS_TEST_EQUAL(errorPos([]{parse("a[]");}), 2);

    try
    {
        parse("1 +");
        POTHOS_TEST_TRUE(false);
    }
    catch (const ExprError &ex)
    {
        POTHOS_TEST_TRUE(std::string(ex.what()).find("unexpected end") != std::string::npos);
    }
    try
    {
        parse("1 2");
        POTHOS_TEST_TRUE(false);
    }
    catch (const ExprError &ex)
    {
        POTHOS_TEST_TRUE(std::string(ex.what()).find("unexpected '2'") != std::string::npos);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_depth_limit)
{
    const std::size_t n = 10000;
    POTHOS_TEST_THROWS(parse(std::string(n, '(') + "1" + std::string(n, ')')), ExprError);
    POTHOS_TEST_THROWS(parse(std::string(n, '[') + std::string(n, ']')), ExprError);
    POTHOS_TEST_THROWS(parse(std::string(n, '-') + "1"), ExprError);
    POTHOS_TEST_THROWS(parse(std::string(n, '~') + "1"), ExprError);

    std::string notChain, powChain("2"), sumChain("1"), callChain, idxChain("a");
    for (std::size_t i = 0; i < n; i++)
    {
        notChain += "not ";
        powChain += "**2";
        sumChain += "+1";
        callChain += "f(";
        idxChain += "[0]";
    }
    POTHOS_TEST_THROWS(parse(notChain + "1"), ExprError);
    POTHOS_TEST_THROWS(parse(powChain), ExprError);
    POTHOS_TEST_THROWS(parse(sumChain), ExprError);
    POTHOS_TEST_THROWS(parse(callChain + "1" + std::string(n, ')')), ExprError);
    POTHOS_TEST_THROWS(parse(idxChain), ExprError);

    //moderate nesting and long flat containers are fine
    POTHOS_TEST_EQUAL(parseDump(std::string(25, '(') + "1" + std::string(25, ')')), "1");
    std::string flat("[");
    for (std::size_t i = 0; i < n; i++) flat += "1,";
    POTHOS_TEST_TRUE(parse(flat + "]")->children.size() == n);
}

namespace {

Value ev(const std::string &expr, const std::map<std::string, Value, std::less<>> &names = {})
{
    return evaluate(expr, [&names](const std::string_view name) -> const Value *
    {
        const auto it = names.find(name);
        return (it == names.end())? nullptr : &it->second;
    });
}

//! Value equality plus the same type name
bool same(const Value &a, const Value &b)
{
    return a == b and a.typeName() == b.typeName();
}

std::string errorMessage(const std::string &expr)
{
    try
    {
        ev(expr);
    }
    catch (const ExprError &ex)
    {
        return ex.what();
    }
    throw std::runtime_error("no ExprError thrown: " + expr);
}

using C = std::complex<double>;

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_expr_arith)
{
    POTHOS_TEST_TRUE(same(ev("1+2*3"), Value(7)));
    POTHOS_TEST_TRUE(same(ev("7/2"), Value(3.5)));
    POTHOS_TEST_TRUE(same(ev("6/2"), Value(3.0)));
    POTHOS_TEST_TRUE(same(ev("7//2"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("-7//2"), Value(-4)));
    POTHOS_TEST_TRUE(same(ev("-7%3"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("7%-3"), Value(-2)));
    POTHOS_TEST_TRUE(same(ev("7.5//2"), Value(3.0)));
    POTHOS_TEST_TRUE(same(ev("-7.5//2"), Value(-4.0)));
    POTHOS_TEST_TRUE(same(ev("-7.5%2"), Value(0.5)));
    POTHOS_TEST_TRUE(same(ev("1.5*2"), Value(3.0)));
    POTHOS_TEST_TRUE(same(ev("2**10"), Value(1024)));
    POTHOS_TEST_TRUE(same(ev("(-2)**3"), Value(-8)));
    POTHOS_TEST_TRUE(same(ev("0**0"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("2**-1"), Value(0.5)));
    POTHOS_TEST_TRUE(same(ev("4**0.5"), Value(2.0)));
    const auto cubeRoot = ev("(-8)**(1/3)");
    POTHOS_TEST_EQUAL(cubeRoot.typeName(), "complex");
    POTHOS_TEST_CLOSE(std::get<C>(cubeRoot.data).real(), 1.0, 1e-9);
    POTHOS_TEST_CLOSE(std::get<C>(cubeRoot.data).imag(), std::sqrt(3.0), 1e-9);
    POTHOS_TEST_TRUE(same(ev("(1+2j)+1"), Value(C(2.0, 2.0))));
    POTHOS_TEST_TRUE(same(ev("(1+2j)*(3-1j)"), Value(C(5.0, 5.0))));
    POTHOS_TEST_TRUE(same(ev("1j**2"), Value(C(-1.0, 0.0))));
    POTHOS_TEST_TRUE(same(ev("(1+1j)/2"), Value(C(0.5, 0.5))));
    POTHOS_TEST_TRUE(same(ev("True+True"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("+True"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("-(3)"), Value(-3)));
    POTHOS_TEST_TRUE(same(ev("-1.5"), Value(-1.5)));
    POTHOS_TEST_TRUE(same(ev("'ab'+\"cd\""), Value("abcd")));
    POTHOS_TEST_TRUE(same(ev("1/0.0"), Value(std::numeric_limits<double>::infinity())));
    POTHOS_TEST_TRUE(std::isnan(std::get<double>(ev("0.0/0.0").data)));

    POTHOS_TEST_EQUAL(errorPos([]{ev("1/0");}), 1);
    POTHOS_TEST_EQUAL(errorPos([]{ev("5 % 0");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{ev("5 // 0");}), 2);
    POTHOS_TEST_TRUE(errorMessage("1/0").find("division by zero") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("'a' * 2");}), 4);
    POTHOS_TEST_TRUE(errorMessage("'a' - 1").find("unsupported operand types for -: 'str' and 'int'") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("1j // 2");}), 3);
    POTHOS_TEST_EQUAL(errorPos([]{ev("1j % 2");}), 3);
    POTHOS_TEST_EQUAL(errorPos([]{ev("-'a'");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{ev("None + 1");}), 5);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_int_overflow)
{
    for (const auto expr : {
        "9223372036854775807+1", "-9223372036854775807-2", "2**63", "3037000500*3037000500",
        "1<<64", "1<<63", "-(-9223372036854775807-1)", "(-9223372036854775807-1)//-1",
        "2**62*2", "3**40", "(-9223372036854775807-1)*-1", "-1*(-9223372036854775807-1)",
        "9223372036854775807--1", "~0<<63<<1"})
    {
        POTHOS_TEST_THROWS(ev(expr), ExprError);
        POTHOS_TEST_TRUE(errorMessage(expr).find("integer overflow") != std::string::npos);
    }
    POTHOS_TEST_TRUE(same(ev("1<<62"), Value(std::int64_t(1) << 62)));
    POTHOS_TEST_TRUE(same(ev("-1<<63"), Value(INT64_MIN)));
    POTHOS_TEST_TRUE(same(ev("-9223372036854775807-1"), Value(INT64_MIN)));
    POTHOS_TEST_TRUE(same(ev("-9223372036854775808"), Value(INT64_MIN)));
    POTHOS_TEST_TRUE(same(ev("[1, - 9223372036854775808]"), Value(List{Value(1), Value(INT64_MIN)})));
    POTHOS_TEST_TRUE(same(ev("-9223372036854775808 + 1"), Value(INT64_MIN+1)));
    POTHOS_TEST_TRUE(same(ev("9223372036854775808"), Value(9223372036854775808.0)));
    POTHOS_TEST_TRUE(same(ev("1 - 9223372036854775808"), Value(1.0 - 9223372036854775808.0)));
    POTHOS_TEST_TRUE(same(ev("-9223372036854775808**1"), Value(-9223372036854775808.0)));
    POTHOS_TEST_TRUE(same(ev("-9223372036854775809"), Value(-9223372036854775809.0)));
    POTHOS_TEST_TRUE(same(ev("18446744073709551615"), Value(18446744073709551615.0)));
    POTHOS_TEST_TRUE(errorMessage("--9223372036854775808").find("integer overflow") != std::string::npos);
    POTHOS_TEST_TRUE(same(ev("(-9223372036854775807-1)%-1"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("(-2)**63"), Value(INT64_MIN)));
    POTHOS_TEST_TRUE(same(ev("3037000499*3037000499"), Value(INT64_C(9223372030926249001))));
    POTHOS_TEST_TRUE(same(ev("1**100000"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("(-1)**100001"), Value(-1)));
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_bitwise)
{
    POTHOS_TEST_TRUE(same(ev("6&3"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("6|3"), Value(7)));
    POTHOS_TEST_TRUE(same(ev("6^3"), Value(5)));
    POTHOS_TEST_TRUE(same(ev("~5"), Value(-6)));
    POTHOS_TEST_TRUE(same(ev("1<<4"), Value(16)));
    POTHOS_TEST_TRUE(same(ev("-16>>2"), Value(-4)));
    POTHOS_TEST_TRUE(same(ev("-1>>100"), Value(-1)));
    POTHOS_TEST_TRUE(same(ev("5>>64"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("0<<1000"), Value(0)));
    POTHOS_TEST_EQUAL(errorPos([]{ev("1.0&1");}), 3);
    POTHOS_TEST_TRUE(errorMessage("1.0&1").find("unsupported operand types") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("1 << -1");}), 2);
    POTHOS_TEST_EQUAL(errorPos([]{ev("~1.5");}), 0);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_compare_logic)
{
    POTHOS_TEST_TRUE(same(ev("1<2<3"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("3>2>2"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("1==1.0"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("1!=1.0"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("1<2.5"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("True<2"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("9007199254740993 > 9007199254740992.0"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("9223372036854775807 < 9223372036854775808.0"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("nan == nan"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("nan < 1 or nan >= 1"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("'a'<'b'"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("'abc'<='abc'"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("1=='a'"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("None==None"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("None==0"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("1j==1j"), Value(true)));
    POTHOS_TEST_EQUAL(errorPos([]{ev("1<'a'");}), 1);
    POTHOS_TEST_EQUAL(errorPos([]{ev("1+2j < 3");}), 5);
    POTHOS_TEST_EQUAL(errorPos([]{ev("None < 1");}), 5);
    POTHOS_TEST_TRUE(errorMessage("1<'a'").find("'<' not supported between 'int' and 'str'") != std::string::npos);

    POTHOS_TEST_TRUE(same(ev("0 or 5"), Value(5)));
    POTHOS_TEST_TRUE(same(ev("3 or 5"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("2 and 0"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("2 and 'x'"), Value("x")));
    POTHOS_TEST_TRUE(same(ev("not ''"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("not 2"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("0 and undefined_name"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("1 or undefined_name"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("1 if 0 else 2"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("1 if 'x' else undefined_name"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("5 < 3 < undefined_name"), Value(false)));
    POTHOS_TEST_THROWS(ev("1 and undefined_name"), ExprError);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_names)
{
    POTHOS_TEST_TRUE(same(ev("x+1", {{"x", Value(41)}}), Value(42)));
    POTHOS_TEST_EQUAL(errorPos([]{ev("1 + foo");}), 4);
    POTHOS_TEST_TRUE(errorMessage("foo").find("unknown name 'foo'") != std::string::npos);
    POTHOS_TEST_THROWS(ev("int32"), ExprError);
    POTHOS_TEST_THROWS(ev("hello"), ExprError);

    POTHOS_TEST_TRUE(same(ev("True"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("true"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("False"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("false"), Value(false)));
    POTHOS_TEST_TRUE(ev("None").isNone());
    POTHOS_TEST_TRUE(ev("null").isNone());
    POTHOS_TEST_CLOSE(std::get<double>(ev("pi").data), 3.141592653589793, 1e-15);
    POTHOS_TEST_CLOSE(std::get<double>(ev("e").data), 2.718281828459045, 1e-15);
    POTHOS_TEST_TRUE(std::isinf(std::get<double>(ev("-inf").data)));
    POTHOS_TEST_TRUE(std::isnan(std::get<double>(ev("nan").data)));
    POTHOS_TEST_TRUE(same(ev("j*j"), Value(C(-1.0, 0.0))));
    POTHOS_TEST_TRUE(same(ev("j", {{"j", Value(5)}}), Value(5)));
    POTHOS_TEST_TRUE(same(ev("pi", {{"pi", Value("x")}}), Value("x")));
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_containers)
{
    POTHOS_TEST_TRUE(same(ev("[]"), Value(List{})));
    POTHOS_TEST_TRUE(same(ev("[1, [2, 'x'], 3]"), Value(List{Value(1), Value(List{Value(2), Value("x")}), Value(3)})));
    POTHOS_TEST_TRUE(same(ev("{}"), Value(Dict{})));
    POTHOS_TEST_EQUAL(ev("{'a': 1, 'b': [1, 2]}").repr(), "{'a': 1, 'b': [1, 2]}");
    POTHOS_TEST_EQUAL(ev("{1: 2, 1: 3}").repr(), "{1: 3}");
    POTHOS_TEST_EQUAL(ev("{1: 2, 'x': 0, 1.0: 3}").repr(), "{1: 3, 'x': 0}");
    POTHOS_TEST_EQUAL(ev("{None: 1, 1 if 0 else 2: 3}").repr(), "{None: 1, 2: 3}");
    POTHOS_TEST_EQUAL(errorPos([]{ev("{[1]: 2}");}), 1);
    POTHOS_TEST_TRUE(errorMessage("{[1]: 2}").find("unhashable type: 'list'") != std::string::npos);
    POTHOS_TEST_THROWS(ev("{{}: 2}"), ExprError);

    POTHOS_TEST_TRUE(same(ev("[1,2,3][-1]"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("[1,2,3][0]"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("[1,2,3][True]"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("[[1, 2], [3, 4]][1][0]"), Value(3)));
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1][5]");}), 4);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1][-2]");}), 4);
    POTHOS_TEST_TRUE(errorMessage("[1][5]").find("index out of range") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1][0.0]");}), 4);
    POTHOS_TEST_TRUE(same(ev("{'a': 1}['a']"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("{1: 'one'}[1.0]"), Value("one")));
    POTHOS_TEST_EQUAL(errorPos([]{ev("{'a': 1}['b']");}), 9);
    POTHOS_TEST_TRUE(errorMessage("{'a': 1}['b']").find("key 'b' not found") != std::string::npos);
    POTHOS_TEST_TRUE(same(ev("'abc'[1]"), Value("b")));
    POTHOS_TEST_TRUE(same(ev("'abc'[-1]"), Value("c")));
    POTHOS_TEST_EQUAL(errorPos([]{ev("'abc'[3]");}), 6);
    POTHOS_TEST_EQUAL(errorPos([]{ev("5[0]");}), 1);
    POTHOS_TEST_TRUE(errorMessage("5[0]").find("'int' object is not subscriptable") != std::string::npos);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_elementwise)
{
    const auto list = [](std::initializer_list<Value> items){return Value(List(items));};
    POTHOS_TEST_TRUE(same(ev("(2 * [1, 2, 3]) + [3, 2, 1]"), list({5, 6, 7})));
    POTHOS_TEST_TRUE(same(ev("[1, 2] + [3, 4]"), list({4, 6})));
    POTHOS_TEST_TRUE(same(ev("[1, 2] - 1"), list({0, 1})));
    POTHOS_TEST_TRUE(same(ev("10 - [1, 2]"), list({9, 8})));
    POTHOS_TEST_TRUE(same(ev("[1, 2] / 2"), list({0.5, 1.0})));
    POTHOS_TEST_TRUE(same(ev("[1, 2] ** 2"), list({1, 4})));
    POTHOS_TEST_TRUE(same(ev("[6, 5] & 3"), list({2, 1})));
    POTHOS_TEST_TRUE(same(ev("[1, [2, 3]] * 2"), list({2, list({4, 6})})));
    POTHOS_TEST_TRUE(same(ev("-[1, [2, 3]]"), list({-1, list({-2, -3})})));
    POTHOS_TEST_TRUE(same(ev("~[0, 1]"), list({-1, -2})));
    POTHOS_TEST_TRUE(same(ev("['a', 'b'] + 'c'"), list({"ac", "bc"})));
    POTHOS_TEST_TRUE(same(ev("[] + 1"), list({})));
    POTHOS_TEST_TRUE(same(ev("j * [1, 2]"), list({C(0.0, 1.0), C(0.0, 2.0)})));

    POTHOS_TEST_EQUAL(errorPos([]{ev("[1, 2] + [1]");}), 7);
    POTHOS_TEST_TRUE(errorMessage("[1, 2] + [1]").find("length") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1, 'a'] * 2");}), 9);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1, 0] // 0");}), 7);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[9223372036854775807] + 1");}), 22);

    POTHOS_TEST_TRUE(same(ev("[1,2] == [1,2.0]"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("[1,2] != [1,2,3]"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("{'a': [1]} == {'a': [1.0]}"), Value(true)));
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1,2] < [3,4]");}), 6);
    POTHOS_TEST_EQUAL(errorPos([]{ev("{'a': 1} + 1");}), 9);
    POTHOS_TEST_EQUAL(errorPos([]{ev("-{}");}), 0);
    POTHOS_TEST_EQUAL(errorPos([]{ev("[1] + {}");}), 4);
}

namespace {

double fl(const std::string &expr)
{
    const auto v = ev(expr);
    if (not v.is<double>()) throw std::runtime_error(expr + " is not a float: " + v.repr());
    return std::get<double>(v.data);
}

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_expr_math)
{
    const double pi = 3.141592653589793;
    POTHOS_TEST_TRUE(same(ev("pow(2, 3)"), Value(8)));
    POTHOS_TEST_TRUE(same(ev("pow(2, 0.5)"), Value(std::sqrt(2.0))));
    POTHOS_TEST_TRUE(same(ev("sqrt(4)"), Value(2.0)));
    POTHOS_TEST_TRUE(std::isnan(fl("sqrt(-1)")));
    POTHOS_TEST_CLOSE(std::abs(std::get<C>(ev("sqrt(-1+0j)").data) - C(0.0, 1.0)), 0.0, 1e-15);
    POTHOS_TEST_TRUE(same(ev("log2(8)"), Value(3.0)));
    POTHOS_TEST_TRUE(same(ev("log10(1000)"), Value(3.0)));
    POTHOS_TEST_CLOSE(fl("log(e)"), 1.0, 1e-15);
    POTHOS_TEST_CLOSE(fl("log(8, 2)"), 3.0, 1e-12);
    POTHOS_TEST_TRUE(same(ev("ln(1)"), Value(0.0)));
    POTHOS_TEST_CLOSE(fl("exp(1)"), 2.718281828459045, 1e-15);
    POTHOS_TEST_CLOSE(fl("atan2(1, 1)"), pi/4, 1e-15);
    POTHOS_TEST_CLOSE(fl("sin(pi/2) + cos(0) + tan(0)"), 2.0, 1e-15);
    POTHOS_TEST_CLOSE(fl("asin(1) + acos(1) + atan(0)"), pi/2, 1e-15);
    POTHOS_TEST_CLOSE(fl("sinh(0) + cosh(0) + tanh(0)"), 1.0, 1e-15);
    POTHOS_TEST_TRUE(same(ev("abs(-3)"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("abs(-2.5)"), Value(2.5)));
    POTHOS_TEST_TRUE(same(ev("abs(3+4j)"), Value(5.0)));
    POTHOS_TEST_THROWS(ev("abs(-9223372036854775807-1)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("round(2.5)"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("round(3.5)"), Value(4)));
    POTHOS_TEST_TRUE(same(ev("round(-0.5)"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("round(2.567, 2)"), Value(2.57)));
    POTHOS_TEST_TRUE(same(ev("round(7)"), Value(7)));
    POTHOS_TEST_TRUE(same(ev("round(1250, -2)"), Value(1200)));
    POTHOS_TEST_TRUE(same(ev("round(1350, -2)"), Value(1400)));
    POTHOS_TEST_TRUE(same(ev("round(-1351, -2)"), Value(-1400)));
    POTHOS_TEST_THROWS(ev("round(nan)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("floor(-1.5)"), Value(-2)));
    POTHOS_TEST_TRUE(same(ev("ceil(1.2)"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("floor(3)"), Value(3)));
    POTHOS_TEST_THROWS(ev("floor(inf)"), ExprError);
    POTHOS_TEST_THROWS(ev("ceil(1e300)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("real(1+2j)"), Value(1.0)));
    POTHOS_TEST_TRUE(same(ev("imag(1+2j)"), Value(2.0)));
    POTHOS_TEST_TRUE(same(ev("imag(5)"), Value(0.0)));
    POTHOS_TEST_TRUE(same(ev("conj(1+2j)"), Value(C(1.0, -2.0))));
    POTHOS_TEST_CLOSE(fl("arg(1j)"), pi/2, 1e-15);
    const auto sins = ev("sin([0, pi/2])");
    POTHOS_TEST_TRUE(sins.is<List>());
    POTHOS_TEST_EQUAL(std::get<List>(sins.data).size(), 2);
    POTHOS_TEST_CLOSE(std::get<double>(std::get<List>(sins.data)[0].data), 0.0, 1e-15);
    POTHOS_TEST_CLOSE(std::get<double>(std::get<List>(sins.data)[1].data), 1.0, 1e-15);
    POTHOS_TEST_TRUE(same(ev("abs([-1, [2.5, -3]])"), Value(List{Value(1), Value(List{Value(2.5), Value(3)})})));
    POTHOS_TEST_EQUAL(errorPos([]{ev("1 + sin('a')");}), 4);
    POTHOS_TEST_TRUE(errorMessage("sin('a')").find("sin() argument must be a number, not 'str'") != std::string::npos);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_builtins)
{
    const auto list = [](std::initializer_list<Value> items){return Value(List(items));};
    POTHOS_TEST_TRUE(same(ev("min(3, 1, 2)"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("max([1, 5, 2])"), Value(5)));
    POTHOS_TEST_TRUE(same(ev("max(1, 2.5)"), Value(2.5)));
    POTHOS_TEST_TRUE(same(ev("min('b', 'a')"), Value("a")));
    POTHOS_TEST_THROWS(ev("min([])"), ExprError);
    POTHOS_TEST_THROWS(ev("min(1, 'a')"), ExprError);
    POTHOS_TEST_THROWS(ev("max(5)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("sum([1, 2.5])"), Value(3.5)));
    POTHOS_TEST_TRUE(same(ev("sum([])"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("sum([[1, 2], [3, 4]])"), list({4, 6})));
    POTHOS_TEST_TRUE(same(ev("len([1, 2])"), Value(2)));
    POTHOS_TEST_TRUE(same(ev("len('abc')"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("len({})"), Value(0)));
    POTHOS_TEST_THROWS(ev("len(5)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("int(3.9)"), Value(3)));
    POTHOS_TEST_TRUE(same(ev("int(-3.9)"), Value(-3)));
    POTHOS_TEST_TRUE(same(ev("int(True)"), Value(1)));
    POTHOS_TEST_TRUE(same(ev("int('42')"), Value(42)));
    POTHOS_TEST_TRUE(same(ev("int(' -7 ')"), Value(-7)));
    POTHOS_TEST_THROWS(ev("int('4x')"), ExprError);
    POTHOS_TEST_THROWS(ev("int('')"), ExprError);
    POTHOS_TEST_THROWS(ev("int(1e300)"), ExprError);
    POTHOS_TEST_THROWS(ev("int(1j)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("float('1e3')"), Value(1000.0)));
    POTHOS_TEST_TRUE(same(ev("float(' -2.5')"), Value(-2.5)));
    POTHOS_TEST_TRUE(same(ev("float(2)"), Value(2.0)));
    POTHOS_TEST_TRUE(std::isinf(fl("float('-inf')")));
    POTHOS_TEST_TRUE(std::isnan(fl("float('nan')")));
    POTHOS_TEST_THROWS(ev("float('x1')"), ExprError);
    POTHOS_TEST_THROWS(ev("float('1.5.2')"), ExprError);
    POTHOS_TEST_TRUE(same(ev("complex(1, 2)"), Value(C(1.0, 2.0))));
    POTHOS_TEST_TRUE(same(ev("complex(3)"), Value(C(3.0, 0.0))));
    POTHOS_TEST_TRUE(same(ev("str(1.5)"), Value("1.5")));
    POTHOS_TEST_TRUE(same(ev("str('a')"), Value("a")));
    POTHOS_TEST_TRUE(same(ev("str([1, 'a'])"), Value("[1, 'a']")));
    POTHOS_TEST_TRUE(same(ev("bool([])"), Value(false)));
    POTHOS_TEST_TRUE(same(ev("bool('x')"), Value(true)));
    POTHOS_TEST_TRUE(same(ev("range(3)"), list({0, 1, 2})));
    POTHOS_TEST_TRUE(same(ev("range(1, 7, 2)"), list({1, 3, 5})));
    POTHOS_TEST_TRUE(same(ev("range(5, 0, -2)"), list({5, 3, 1})));
    POTHOS_TEST_TRUE(same(ev("range(0)"), list({})));
    POTHOS_TEST_TRUE(same(ev("range(3, 1)"), list({})));
    POTHOS_TEST_TRUE(same(ev("len(range(-9223372036854775807-1, -9223372036854775807-1+10))"), Value(10)));
    POTHOS_TEST_THROWS(ev("range(0, 1, 0)"), ExprError);
    POTHOS_TEST_THROWS(ev("range(10000000)"), ExprError);
    POTHOS_TEST_THROWS(ev("range(-9223372036854775807-1, 9223372036854775807)"), ExprError);
    POTHOS_TEST_THROWS(ev("range(1.5)"), ExprError);

    POTHOS_TEST_EQUAL(errorPos([]{ev("1 + sin(1, 2)");}), 4);
    POTHOS_TEST_TRUE(errorMessage("sin(1, 2)").find("sin() takes 1 argument (2 given)") != std::string::npos);
    POTHOS_TEST_TRUE(errorMessage("atan2(1)").find("atan2() takes 2 arguments (1 given)") != std::string::npos);
    POTHOS_TEST_TRUE(errorMessage("range()").find("range() takes 1 to 3 arguments (0 given)") != std::string::npos);
    POTHOS_TEST_TRUE(errorMessage("min()").find("min() takes at least 1 argument (0 given)") != std::string::npos);
    POTHOS_TEST_EQUAL(errorPos([]{ev("2 * foo(1)");}), 4);
    POTHOS_TEST_TRUE(errorMessage("foo(1)").find("unknown function 'foo'") != std::string::npos);
    POTHOS_TEST_TRUE(errorMessage("pi(1)").find("'pi' is not a function") != std::string::npos);
    POTHOS_TEST_TRUE(errorMessage("sin").find("unknown name 'sin'") != std::string::npos);

    //functions and constants are separate namespaces
    POTHOS_TEST_TRUE(same(ev("sin(0) + sin", {{"sin", Value(1)}}), Value(1.0)));
    POTHOS_TEST_TRUE(errorMessage("x(1)").find("unknown function 'x'") != std::string::npos);
}

namespace {

//! Run fn on a thread with a small stack, rethrow what it threw
void runOnSmallStack(const std::size_t stackSize, const std::function<void()> &fn)
{
    struct Job
    {
        const std::function<void()> *fn;
        std::exception_ptr error;
        static void run(Job *job)
        {
            try {(*job->fn)();}
            catch (...) {job->error = std::current_exception();}
        }
    } job{&fn, nullptr};
#ifdef _WIN32
    const auto entry = [](LPVOID arg) -> DWORD {Job::run(static_cast<Job *>(arg)); return 0;};
    const auto thread = CreateThread(nullptr, stackSize, entry, &job, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (thread == nullptr) throw std::runtime_error("CreateThread failed");
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, stackSize);
    pthread_t thread;
    const auto entry = [](void *arg) -> void * {Job::run(static_cast<Job *>(arg)); return nullptr;};
    const int rc = pthread_create(&thread, &attr, entry, &job);
    pthread_attr_destroy(&attr);
    if (rc != 0) throw std::runtime_error("pthread_create failed");
    pthread_join(thread, nullptr);
#endif
    if (job.error) std::rethrow_exception(job.error);
}

std::string repeat(const std::string &s, const std::size_t n)
{
    std::string out;
    for (std::size_t i = 0; i < n; i++) out += s;
    return out;
}

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_expr_depth_small_stack)
{
    //the stack budget (StackBudget) plus a margin: well below the 512 KiB of macOS secondary threads
    const std::size_t stack = StackBudget + 64*1024;

    //deep expressions either evaluate or fail cleanly, whatever the frame sizes of the compiler
    const auto deepOrTooDeep = [](const std::string &expr, const Value &expected)
    {
        try
        {
            return same(ev(expr), expected);
        }
        catch (const ExprError &ex)
        {
            return std::string(ex.what()).find("nested too deeply") != std::string::npos;
        }
    };

    runOnSmallStack(stack, [&deepOrTooDeep]
    {
        //moderate nesting always works
        const std::size_t m = 25;
        if (not same(ev(repeat("(", m) + "1" + repeat(")", m)), Value(1))) throw std::runtime_error("parens");
        if (not same(ev(repeat("abs(", m) + "-1" + repeat(")", m)), Value(1))) throw std::runtime_error("calls");
        if (ev(repeat("[", m) + repeat("]", m)).repr().size() != 2*m) throw std::runtime_error("lists");
        if (not same(ev(repeat("-", m) + "1"), Value(-1))) throw std::runtime_error("unary");
        if (not same(ev("1" + repeat("+1", 99)), Value(100))) throw std::runtime_error("sum");

        //the deepest valid expressions evaluate or report the stack budget
        const auto n = MaxNesting - 1;
        if (not deepOrTooDeep(repeat("(", n) + "1" + repeat(")", n), Value(1))) throw std::runtime_error("parens n");
        if (not deepOrTooDeep(repeat("abs(", n-1) + "-1" + repeat(")", n-1), Value(1))) throw std::runtime_error("calls n");
        if (not deepOrTooDeep(repeat("-", n) + "1", Value(n % 2? -1 : 1))) throw std::runtime_error("unary n");
        if (not deepOrTooDeep("1" + repeat("+1", MaxTreeDepth-1), Value(int(MaxTreeDepth)))) throw std::runtime_error("sum n");
        if (not deepOrTooDeep(repeat("[", n) + repeat("]", n) + repeat("[0]", n-1), Value(List{}))) throw std::runtime_error("index n");
    });
    for (const auto &expr : {
        repeat("(", 10000) + "1" + repeat(")", 10000), repeat("[", 10000), repeat("-", 10000) + "1",
        repeat("abs(", 10000) + "1", "1" + repeat("+1", 10000), "a" + repeat("[0]", 10000),
        repeat("not ", 10000) + "1", "2" + repeat("**2", 10000), repeat("1 if 1 else ", 10000) + "1",
        repeat("{1:", 10000)})
    {
        bool threw = false;
        runOnSmallStack(stack, [&expr, &threw]
        {
            try {ev(expr);}
            catch (const ExprError &) {threw = true;}
        });
        POTHOS_TEST_TRUE(threw);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_float_stream_fallback)
{
    //the istringstream path is used where floating point from_chars is missing (Apple libc++)
    POTHOS_TEST_EQUAL(parseFloatStream("2.5"), 2.5);
    POTHOS_TEST_EQUAL(parseFloatStream("5e-324"), 5e-324);
    POTHOS_TEST_EQUAL(parseFloatStream("2.2250738585072014e-308"), 2.2250738585072014e-308);
    POTHOS_TEST_EQUAL(parseFloatStream("1.7976931348623157e308"), 1.7976931348623157e308);
    POTHOS_TEST_EQUAL(parseFloatStream("1e-400"), 0.0);
    POTHOS_TEST_TRUE(std::isinf(parseFloatStream("1e400")));
    POTHOS_TEST_EQUAL(std::get<double>(lexOne("5e-324").literal.data), 5e-324);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_float_out_of_range)
{
    //the direction comes from the value, not from the sign of the written exponent
    const std::string zeros(400, '0');
    POTHOS_TEST_EQUAL(std::get<double>(ev("0." + zeros + "1").data), 0.0);
    POTHOS_TEST_EQUAL(std::get<double>(ev("0." + zeros + "1e5").data), 0.0);
    POTHOS_TEST_TRUE(std::isinf(std::get<double>(ev("1" + zeros + ".0").data)));
    POTHOS_TEST_TRUE(std::isinf(std::get<double>(ev("1" + zeros + ".0e-5").data)));
    POTHOS_TEST_CLOSE(std::get<double>(ev("1" + zeros + ".0e-100").data), 1e300, 1e285);
    POTHOS_TEST_TRUE(std::isinf(std::get<double>(ev("1e400").data)));
    POTHOS_TEST_EQUAL(std::get<double>(ev("1e-400").data), 0.0);
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_round_overflow)
{
    //round to -19 digits: the result 10**19 does not fit an int64
    POTHOS_TEST_THROWS(ev("round(9000000000000000000, -19)"), ExprError);
    POTHOS_TEST_THROWS(ev("round(-9000000000000000000, -19)"), ExprError);
    POTHOS_TEST_TRUE(same(ev("round(5000000000000000000, -19)"), Value(0)));
    POTHOS_TEST_TRUE(same(ev("round(9000000000000000000, -20)"), Value(0)));
}

POTHOS_TEST_BLOCK("/util/tests", test_expr_float_repr_python)
{
    //the repr rule of Python: fixed notation for an exponent from -4 to 15
    POTHOS_TEST_EQUAL(Value(100000.0).repr(), "100000.0");
    POTHOS_TEST_EQUAL(Value(1e15).repr(), "1000000000000000.0");
    POTHOS_TEST_EQUAL(Value(1e16).repr(), "1e+16");
    POTHOS_TEST_EQUAL(Value(0.0001).repr(), "0.0001");
    POTHOS_TEST_EQUAL(Value(0.00001).repr(), "1e-05");
    POTHOS_TEST_EQUAL(Value(1.5).repr(), "1.5");
    POTHOS_TEST_EQUAL(Value(-0.0).repr(), "-0.0");
    POTHOS_TEST_EQUAL(Value(0.1).repr(), "0.1");
    POTHOS_TEST_EQUAL(Value(123456789012345680.0).repr(), "1.2345678901234568e+17");
    POTHOS_TEST_EQUAL(Value(1.5e-7).repr(), "1.5e-07");
    POTHOS_TEST_EQUAL(Value(1e100).repr(), "1e+100");
    POTHOS_TEST_EQUAL(ev("str(1e5)").repr(), "'100000.0'");
}
