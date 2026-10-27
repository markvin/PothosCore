// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Operators of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <cstddef>
#include <string_view>

namespace Pothos {
namespace Util {
namespace Expr {

//! Arithmetic or bitwise binary operator, pos is reported on errors
Value binaryOp(std::string_view op, const Value &a, const Value &b, std::size_t pos);

//! Unary "-", "+" or "~"
Value unaryOp(std::string_view op, const Value &a, std::size_t pos);

//! Comparison operator: "<", ">", "==", "!=", "<=", ">="
bool compareOp(std::string_view op, const Value &a, const Value &b, std::size_t pos);

} //namespace Expr
} //namespace Util
} //namespace Pothos
