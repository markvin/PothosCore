// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Evaluator of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include "Util/Expr/Parser.hpp"
#include <functional>
#include <string_view>

namespace Pothos {
namespace Util {
namespace Expr {

/*!
 * User constants: the value of a name or nullptr when it is not defined.
 * User constants take precedence over built-in names (True, pi, j...).
 */
using Lookup = std::function<const Value *(std::string_view)>;

/*!
 * Evaluate a syntax tree.
 * \throws ExprError with the offset of the failing node
 */
Value evaluate(const Node &node, const Lookup &lookup);

//! Parse and evaluate an expression
Value evaluate(std::string_view expr, const Lookup &lookup);

} //namespace Expr
} //namespace Util
} //namespace Pothos
