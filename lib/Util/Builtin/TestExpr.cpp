// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Self tests for the internal expression evaluator (lib/Util/Expr)

#include <Pothos/Testing.hpp>
#include "Util/Expr/Value.hpp"
#include "Util/Expr/Lexer.hpp"
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
