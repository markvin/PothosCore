// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Conversions between expression values and Pothos objects (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <Pothos/Object/Object.hpp>

namespace Pothos {
namespace Util {
namespace Expr {

/*!
 * Convert a value to an object: None -> null object, bool, int -> long long,
 * float -> double, complex -> std::complex<double>, str -> std::string,
 * list -> ProxyVector, dict -> ProxyMap (proxies of the managed environment).
 */
Pothos::Object toObject(const Value &value);

/*!
 * Convert an object to a value: null object -> None, std::string, bool,
 * numbers by their DType (integers wider than int -> float), and anything
 * convertible to ProxyVector or ProxyMap recursively.
 * \throws Pothos::Exception for an unsupported type (names the type)
 */
Value fromObject(const Pothos::Object &object);

} //namespace Expr
} //namespace Util
} //namespace Pothos
