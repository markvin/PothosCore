// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Built-in functions of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <cstddef>
#include <span>
#include <string_view>

namespace Pothos {
namespace Util {
namespace Expr {

//! Is there a built-in function with this name?
bool isBuiltinFunction(std::string_view name);

/*!
 * Call a built-in function (checks the number of arguments).
 * \throws ExprError at pos for bad arguments or an unknown function
 */
Value callBuiltin(std::string_view name, std::span<const Value> args, std::size_t pos);

} //namespace Expr
} //namespace Util
} //namespace Pothos
