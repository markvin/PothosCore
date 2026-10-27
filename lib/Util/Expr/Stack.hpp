// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Stack use limit of the internal expression evaluator (not installed)

#pragma once
#include "Util/Expr/Value.hpp"
#include <cstddef>
#include <cstdint>

namespace Pothos {
namespace Util {
namespace Expr {

/*!
 * Stack bytes that parsing or evaluating one expression may use.
 * Frame sizes differ a lot between compilers and build types, so the nesting
 * counters alone cannot keep recursion within small thread stacks.
 */
constexpr std::size_t StackBudget = 128*1024;

//! Measures the stack used since construction (address distance of locals)
class StackGuard
{
public:
    StackGuard(void):
        _base(here())
    {
        return;
    }

    //! \throws ExprError at pos when more than StackBudget is in use
    void check(const std::size_t pos) const
    {
        const auto now = here();
        const auto used = (_base > now)? _base - now : now - _base;
        if (used > StackBudget) throw ExprError("expression is nested too deeply", pos);
    }

private:
    static std::uintptr_t here(void)
    {
        volatile char marker = 0;
        return reinterpret_cast<std::uintptr_t>(&marker);
    }

    std::uintptr_t _base;
};

} //namespace Expr
} //namespace Util
} //namespace Pothos
