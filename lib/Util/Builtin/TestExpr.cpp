// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Self tests for the internal expression evaluator (lib/Util/Expr)

#include <Pothos/Testing.hpp>
#include "Util/Expr/Value.hpp"
#include "Util/Expr/Lexer.hpp"
#include "Util/Expr/Parser.hpp"
#include <complex>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

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
    POTHOS_TEST_EQUAL(errorPos([]{tokenize("9223372036854775808");}), 0);
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
    POTHOS_TEST_EQUAL(parseDump(std::string(100, '(') + "1" + std::string(100, ')')), "1");
    std::string flat("[");
    for (std::size_t i = 0; i < n; i++) flat += "1,";
    POTHOS_TEST_TRUE(parse(flat + "]")->children.size() == n);
}
