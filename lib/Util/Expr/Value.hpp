// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Value type of the internal expression evaluator (not installed)

#pragma once
#include <complex>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Pothos {
namespace Util {
namespace Expr {

struct Value;

//! List value: elements in order
using List = std::vector<Value>;

//! Dict value: key/value pairs in insertion order, keys looked up with ==
using Dict = std::vector<std::pair<Value, Value>>;

//! An evaluation error at an offset (0-based) into the expression
class ExprError : public std::runtime_error
{
public:
    ExprError(const std::string &message, const std::size_t pos):
        std::runtime_error(message),
        pos(pos)
    {
        return;
    }

    std::size_t pos;
};

//! A dynamically typed value: None, bool, int, float, complex, str, list, dict
struct Value
{
    using Data = std::variant<std::monostate, bool, std::int64_t, double, std::complex<double>, std::string, List, Dict>;

    Value(void) = default;
    Value(const bool v): data(v) {}
    template <std::integral T> requires (not std::same_as<T, bool>)
    Value(const T v): data(static_cast<std::int64_t>(v)) {}
    Value(const double v): data(v) {}
    Value(const std::complex<double> &v): data(v) {}
    Value(std::string v): data(std::move(v)) {}
    Value(const char *v): data(std::string(v)) {}
    Value(List v): data(std::move(v)) {}
    Value(Dict v): data(std::move(v)) {}

    template <typename T>
    bool is(void) const
    {
        return std::holds_alternative<T>(data);
    }

    bool isNone(void) const
    {
        return this->is<std::monostate>();
    }

    //! Python type name: "None", "bool", "int", "float", "complex", "str", "list", "dict"
    std::string_view typeName(void) const;

    //! Python truthiness
    bool truthy(void) const;

    //! Python-like representation for messages and tests
    std::string repr(void) const;

    //! Equality, numbers compare by value across bool/int/float/complex
    bool operator==(const Value &other) const;

    Data data;
};

} //namespace Expr
} //namespace Util
} //namespace Pothos
