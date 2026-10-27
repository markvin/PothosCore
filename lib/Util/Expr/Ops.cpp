// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Ops.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

using namespace Pothos::Util::Expr;

namespace {

using C = std::complex<double>;
constexpr auto IntMax = std::numeric_limits<std::int64_t>::max();
constexpr auto IntMin = std::numeric_limits<std::int64_t>::min();

[[noreturn]] void fail(const std::string &message, const std::size_t pos)
{
    throw ExprError(message, pos);
}

[[noreturn]] void overflow(const std::size_t pos)
{
    fail("integer overflow", pos);
}

[[noreturn]] void unsupported(const std::string_view op, const Value &a, const Value &b, const std::size_t pos)
{
    fail("unsupported operand types for " + std::string(op) + ": '" +
        std::string(a.typeName()) + "' and '" + std::string(b.typeName()) + "'", pos);
}

/***********************************************************************
 * numeric tower: bool < int < float < complex
 **********************************************************************/
enum Rank {NotNumber = -1, RankBool, RankInt, RankFloat, RankComplex};

Rank rank(const Value &v)
{
    if (v.is<bool>()) return RankBool;
    if (v.is<std::int64_t>()) return RankInt;
    if (v.is<double>()) return RankFloat;
    if (v.is<C>()) return RankComplex;
    return NotNumber;
}

std::int64_t toInt(const Value &v)
{
    if (v.is<bool>()) return std::get<bool>(v.data)? 1 : 0;
    return std::get<std::int64_t>(v.data);
}

double toFloat(const Value &v)
{
    if (v.is<double>()) return std::get<double>(v.data);
    return static_cast<double>(toInt(v));
}

C toComplex(const Value &v)
{
    if (v.is<C>()) return std::get<C>(v.data);
    return C(toFloat(v), 0.0);
}

/***********************************************************************
 * checked 64-bit integer arithmetic (no signed overflow, no compiler builtins)
 **********************************************************************/
std::int64_t addInt(const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if ((b > 0 and a > IntMax - b) or (b < 0 and a < IntMin - b)) overflow(pos);
    return a + b;
}

std::int64_t subInt(const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if ((b < 0 and a > IntMax + b) or (b > 0 and a < IntMin + b)) overflow(pos);
    return a - b;
}

std::int64_t mulInt(const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if (a > 0)
    {
        if (b > 0) {if (a > IntMax / b) overflow(pos);}
        else {if (b < IntMin / a) overflow(pos);}
    }
    else
    {
        if (b > 0) {if (a < IntMin / b) overflow(pos);}
        else {if (a != 0 and b < IntMax / a) overflow(pos);}
    }
    return a * b;
}

std::int64_t floorDivInt(const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if (b == 0) fail("integer division by zero", pos);
    if (a == IntMin and b == -1) overflow(pos);
    auto q = a / b;
    if (a % b != 0 and ((a < 0) != (b < 0))) q--;
    return q;
}

std::int64_t modInt(const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if (b == 0) fail("integer modulo by zero", pos);
    if (b == -1) return 0;
    auto r = a % b;
    if (r != 0 and ((r < 0) != (b < 0))) r += b;
    return r;
}

std::int64_t powInt(std::int64_t base, std::int64_t exp, const std::size_t pos)
{
    std::int64_t result = 1;
    while (exp != 0)
    {
        if (exp & 1) result = mulInt(result, base, pos);
        exp >>= 1;
        //squaring is only needed while bits remain, the result would overflow too
        if (exp != 0) base = mulInt(base, base, pos);
    }
    return result;
}

std::int64_t shiftInt(const std::string_view op, const std::int64_t a, const std::int64_t n, const std::size_t pos)
{
    if (n < 0) fail("negative shift count", pos);
    if (op == ">>") return (n >= 64)? (a < 0? -1 : 0) : (a >> n);
    if (a == 0) return 0;
    if (n >= 64 or a > (IntMax >> n) or a < (IntMin >> n)) overflow(pos);
    return a << n; //well defined since C++20, the range check above makes it exact
}

/***********************************************************************
 * float division semantics of Python
 **********************************************************************/
void floatDivMod(const double a, const double b, double &div, double &mod)
{
    mod = std::fmod(a, b);
    div = (a - mod) / b;
    if (mod != 0.0)
    {
        if ((b < 0.0) != (mod < 0.0))
        {
            mod += b;
            div -= 1.0;
        }
    }
    else mod = std::copysign(0.0, b);
    if (div != 0.0)
    {
        const double floorDiv = std::floor(div);
        div = (div - floorDiv > 0.5)? floorDiv + 1.0 : floorDiv;
    }
    else div = std::copysign(0.0, a / b);
}

Value powFloat(const double a, const double b)
{
    //negative base with a fractional exponent has a complex result
    if (a < 0.0 and std::isfinite(b) and b != std::floor(b)) return Value(std::pow(C(a, 0.0), C(b, 0.0)));
    return Value(std::pow(a, b));
}

Value powComplex(const C &a, const C &b)
{
    //small integer exponents by repeated multiplication: exact for 1j**2
    if (b.imag() == 0.0 and b.real() == std::floor(b.real()) and std::abs(b.real()) <= 100.0)
    {
        auto n = static_cast<std::int64_t>(std::abs(b.real()));
        C result(1.0, 0.0), base(a);
        while (n != 0)
        {
            if (n & 1) result *= base;
            n >>= 1;
            if (n != 0) base *= base;
        }
        return Value((b.real() < 0.0)? C(1.0, 0.0) / result : result);
    }
    return Value(std::pow(a, b));
}

Value intOp(const std::string_view op, const std::int64_t a, const std::int64_t b, const std::size_t pos)
{
    if (op == "+") return Value(addInt(a, b, pos));
    if (op == "-") return Value(subInt(a, b, pos));
    if (op == "*") return Value(mulInt(a, b, pos));
    if (op == "/")
    {
        if (b == 0) fail("division by zero", pos);
        return Value(static_cast<double>(a) / static_cast<double>(b));
    }
    if (op == "//") return Value(floorDivInt(a, b, pos));
    if (op == "%") return Value(modInt(a, b, pos));
    if (op == "**")
    {
        if (b >= 0) return Value(powInt(a, b, pos));
        return powFloat(static_cast<double>(a), static_cast<double>(b));
    }
    if (op == "&") return Value(a & b);
    if (op == "|") return Value(a | b);
    if (op == "^") return Value(a ^ b);
    return Value(shiftInt(op, a, b, pos));
}

Value floatOp(const std::string_view op, const double a, const double b)
{
    if (op == "+") return Value(a + b);
    if (op == "-") return Value(a - b);
    if (op == "*") return Value(a * b);
    if (op == "/") return Value(a / b);
    if (op == "**") return powFloat(a, b);
    double div = 0.0, mod = 0.0;
    floatDivMod(a, b, div, mod);
    return Value((op == "//")? div : mod);
}

Value complexOp(const std::string_view op, const C &a, const C &b)
{
    if (op == "+") return Value(a + b);
    if (op == "-") return Value(a - b);
    if (op == "*") return Value(a * b);
    if (op == "/") return Value(a / b);
    return powComplex(a, b);
}

bool isBitwise(const std::string_view op)
{
    return op == "&" or op == "|" or op == "^" or op == "<<" or op == ">>";
}

/***********************************************************************
 * ordering of real numbers: exact for int against float
 **********************************************************************/
//! -1, 0, 1 for less, equal, greater; 2 when unordered (nan)
int compareIntFloat(const std::int64_t i, const double d)
{
    if (std::isnan(d)) return 2;
    if (d >= 9223372036854775808.0) return -1;
    if (d < -9223372036854775808.0) return 1;
    const double t = std::trunc(d);
    const auto ti = static_cast<std::int64_t>(t);
    if (i != ti) return (i < ti)? -1 : 1;
    const double frac = d - t;
    return (frac > 0.0)? -1 : (frac < 0.0)? 1 : 0;
}

int compareReal(const Value &a, const Value &b)
{
    const bool aFloat = a.is<double>(), bFloat = b.is<double>();
    if (aFloat and bFloat)
    {
        const double x = std::get<double>(a.data), y = std::get<double>(b.data);
        if (std::isnan(x) or std::isnan(y)) return 2;
        return (x < y)? -1 : (x > y)? 1 : 0;
    }
    if (bFloat) return compareIntFloat(toInt(a), std::get<double>(b.data));
    if (aFloat)
    {
        const int c = compareIntFloat(toInt(b), std::get<double>(a.data));
        return (c == 2)? 2 : -c;
    }
    const auto x = toInt(a), y = toInt(b);
    return (x < y)? -1 : (x > y)? 1 : 0;
}

} //namespace

Value Pothos::Util::Expr::binaryOp(const std::string_view op, const Value &a, const Value &b, const std::size_t pos)
{
    if (op == "+" and a.is<std::string>() and b.is<std::string>())
    {
        return Value(std::get<std::string>(a.data) + std::get<std::string>(b.data));
    }

    const auto ra = rank(a), rb = rank(b);
    if (ra == NotNumber or rb == NotNumber) unsupported(op, a, b, pos);
    const auto r = std::max({ra, rb, RankInt});
    if (r == RankInt) return intOp(op, toInt(a), toInt(b), pos);
    if (isBitwise(op)) unsupported(op, a, b, pos);
    if (r == RankFloat) return floatOp(op, toFloat(a), toFloat(b));
    if (op == "//" or op == "%") unsupported(op, a, b, pos);
    return complexOp(op, toComplex(a), toComplex(b));
}

Value Pothos::Util::Expr::unaryOp(const std::string_view op, const Value &a, const std::size_t pos)
{
    const auto r = rank(a);
    if (r == NotNumber or (op == "~" and r > RankInt))
    {
        fail("bad operand type for unary " + std::string(op) + ": '" + std::string(a.typeName()) + "'", pos);
    }
    if (op == "~") return Value(~toInt(a));
    if (r <= RankInt)
    {
        const auto i = toInt(a);
        if (op == "+") return Value(i);
        if (i == IntMin) overflow(pos);
        return Value(-i);
    }
    if (op == "+") return a;
    if (r == RankFloat) return Value(-std::get<double>(a.data));
    return Value(-std::get<C>(a.data));
}

bool Pothos::Util::Expr::compareOp(const std::string_view op, const Value &a, const Value &b, const std::size_t pos)
{
    if (op == "==") return a == b;
    if (op == "!=") return not (a == b);

    int c = 0;
    const auto ra = rank(a), rb = rank(b);
    if (ra != NotNumber and rb != NotNumber and ra != RankComplex and rb != RankComplex) c = compareReal(a, b);
    else if (a.is<std::string>() and b.is<std::string>())
    {
        const int s = std::get<std::string>(a.data).compare(std::get<std::string>(b.data));
        c = (s < 0)? -1 : (s > 0)? 1 : 0;
    }
    else fail("'" + std::string(op) + "' not supported between '" +
        std::string(a.typeName()) + "' and '" + std::string(b.typeName()) + "'", pos);

    if (c == 2) return false; //nan is unordered
    if (op == "<") return c < 0;
    if (op == ">") return c > 0;
    if (op == "<=") return c <= 0;
    return c >= 0;
}
