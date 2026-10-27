// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Tokenizer of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace Pothos {
namespace Util {
namespace Expr {

enum class Tok
{
    Int,    //!< integer literal, literal holds int
    Float,  //!< float literal, literal holds float
    Imag,   //!< imaginary literal (3j), literal holds complex
    String, //!< string literal, literal holds the decoded str
    Name,   //!< identifier or keyword
    Op,     //!< operator or punctuation
    End,    //!< end of input
};

struct Token
{
    Tok kind;
    std::string_view text; //!< source text (points into the expression)
    std::size_t pos;       //!< offset into the expression (0-based)
    Value literal;
    bool needsMinus = false; //!< Int 2**63: valid only as the operand of a unary minus (literal holds INT64_MIN)
};

/*!
 * Split an expression into tokens, the last one is always Tok::End.
 * \throws ExprError at the offending character
 */
std::vector<Token> tokenize(std::string_view expr);

/*!
 * Convert validated decimal digits to a double (locale independent).
 * Out of range values become inf or 0 like Python.
 * \param intPart digits before the point (may be empty)
 * \param fracPart digits after the point (may be empty)
 * \param expPart exponent digits with an optional sign (may be empty)
 */
double parseDecimalFloat(std::string_view intPart, std::string_view fracPart, std::string_view expPart);

} //namespace Expr
} //namespace Util
} //namespace Pothos
