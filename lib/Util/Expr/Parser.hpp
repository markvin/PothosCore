// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Parser of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Pothos {
namespace Util {
namespace Expr {

/*!
 * Nesting of parentheses, containers, calls and prefix operators.
 * Parsing and evaluation recurse: the limits keep the stack use within
 * small thread stacks (512 KiB secondary threads on macOS).
 */
constexpr std::size_t MaxNesting = 100;

//! Height of the syntax tree (left-associative chains like 1+1+...+1 grow it without nesting)
constexpr std::size_t MaxTreeDepth = 256;

struct Node
{
    enum class Kind
    {
        Literal, //!< literal value
        Name,    //!< name lookup (name)
        Unary,   //!< op is "-", "+" or "~"; one child
        Binary,  //!< arithmetic/bitwise op; two children
        Compare, //!< comparison chain: children are operands, ops between them
        And,     //!< short-circuit and over the children
        Or,      //!< short-circuit or over the children
        Not,     //!< logical not of one child
        IfElse,  //!< children: condition, value if true, value if false
        List,    //!< list display of the children
        Dict,    //!< dict display: children are key, value, key, value...
        Call,    //!< call of function name with the children as arguments
        Index,   //!< children: container, index
    };

    Kind kind;
    std::size_t pos; //!< offset of the token that produced the node (the operator for operators)
    std::string op;
    std::vector<std::string> ops; //!< comparison operators of a Compare node
    std::string name;
    Value literal;
    std::vector<std::unique_ptr<Node>> children;
    std::size_t depth = 1; //!< height of the subtree (bounds evaluation recursion)
};

/*!
 * Parse a whole expression into a syntax tree.
 * \throws ExprError with the offset of the offending token
 */
std::unique_ptr<Node> parse(std::string_view expr);

//! S-expression of the tree for tests, for example "(+ 1 (* 2 3))"
std::string dump(const Node &node);

} //namespace Expr
} //namespace Util
} //namespace Pothos
