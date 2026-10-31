// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Builtins.hpp"
#include "Util/Expr/Lexer.hpp"
#include "Util/Expr/Ops.hpp"
#include "Util/StringUtils.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>

using namespace Pothos::Util::Expr;

namespace {

using C = std::complex<double>;

//! Longest list that range() may create
constexpr std::uint64_t MaxRangeLength = 1000000;

struct Builtin;
using Fn = Value (*)(const Builtin &self, std::span<const Value> args, std::size_t pos);

struct Builtin
{
    std::string_view name;
    std::size_t minArgs;
    std::size_t maxArgs;
    Fn fn;
    double (*real)(double) = nullptr; //!< real version of a math function
    C (*cplx)(const C &) = nullptr;   //!< complex version of a math function
};

[[noreturn]] void fail(const Builtin &self, const std::string &message, const std::size_t pos)
{
    throw ExprError(std::string(self.name) + "() " + message, pos);
}

bool isNumber(const Value &v)
{
    return v.is<bool>() or v.is<std::int64_t>() or v.is<double>() or v.is<C>();
}

bool isInt(const Value &v)
{
    return v.is<bool>() or v.is<std::int64_t>();
}

std::int64_t toInt(const Value &v)
{
    return v.is<bool>()? std::int64_t(std::get<bool>(v.data)) : std::get<std::int64_t>(v.data);
}

double toFloat(const Value &v)
{
    return v.is<double>()? std::get<double>(v.data) : static_cast<double>(toInt(v));
}

C toComplex(const Value &v)
{
    return v.is<C>()? std::get<C>(v.data) : C(toFloat(v), 0.0);
}

void requireNumber(const Builtin &self, const Value &v, const std::size_t pos)
{
    if (not isNumber(v)) fail(self, "argument must be a number, not '" + std::string(v.typeName()) + "'", pos);
}

void requireReal(const Builtin &self, const Value &v, const std::size_t pos)
{
    if (not isNumber(v) or v.is<C>()) fail(self, "argument must be a real number, not '" + std::string(v.typeName()) + "'", pos);
}

void requireInt(const Builtin &self, const Value &v, const std::size_t pos)
{
    if (not isInt(v)) fail(self, "argument must be an integer, not '" + std::string(v.typeName()) + "'", pos);
}

//! Float to int for floor/ceil/round/int: fails for inf, nan and out of range values
std::int64_t floatToInt(const Builtin &self, const double d, const std::size_t pos)
{
    if (not std::isfinite(d)) fail(self, "cannot convert float " + Value(d).repr() + " to integer", pos);
    if (d < -9223372036854775808.0 or d >= 9223372036854775808.0) throw ExprError("integer overflow", pos);
    return static_cast<std::int64_t>(d);
}

//! Apply f to v, or to every element of a (nested) list
template <typename F>
Value mapList(const Value &v, F &&f)
{
    const auto *list = std::get_if<List>(&v.data);
    if (list == nullptr) return f(v);
    List out;
    out.reserve(list->size());
    for (const auto &item : *list) out.push_back(mapList(item, f));
    return Value(std::move(out));
}

/***********************************************************************
 * math
 **********************************************************************/
Value mathFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        if (v.is<C>()) return Value(self.cplx(std::get<C>(v.data)));
        return Value(self.real(toFloat(v)));
    });
}

Value logOne(const Builtin &self, const Value &v, const std::size_t pos)
{
    requireNumber(self, v, pos);
    if (v.is<C>()) return Value(std::log(std::get<C>(v.data)));
    return Value(std::log(toFloat(v)));
}

Value logFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    const auto x = mapList(args[0], [&](const Value &v){return logOne(self, v, pos);});
    if (args.size() == 1) return x;
    return binaryOp("/", x, logOne(self, args[1], pos), pos);
}

Value absFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        if (v.is<C>()) return Value(std::abs(std::get<C>(v.data)));
        if (v.is<double>()) return Value(std::fabs(std::get<double>(v.data)));
        const auto i = toInt(v);
        if (i == std::numeric_limits<std::int64_t>::min()) throw ExprError("integer overflow", pos);
        return Value(i < 0? -i : i);
    });
}

Value roundingFn(const Builtin &self, std::span<const Value> args, const std::size_t pos, double (*round)(double))
{
    return mapList(args[0], [&](const Value &v)
    {
        requireReal(self, v, pos);
        if (isInt(v)) return Value(toInt(v));
        return Value(floatToInt(self, round(std::get<double>(v.data)), pos));
    });
}

Value floorFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return roundingFn(self, args, pos, [](double x){return std::floor(x);});
}

Value ceilFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return roundingFn(self, args, pos, [](double x){return std::ceil(x);});
}

//! Round half to even (the default floating point rounding mode)
double roundHalfEven(const double x)
{
    return std::nearbyint(x);
}

Value roundFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    if (args.size() == 1) return roundingFn(self, args, pos, roundHalfEven);

    requireInt(self, args[1], pos);
    const auto n = toInt(args[1]);
    return mapList(args[0], [&](const Value &v)
    {
        requireReal(self, v, pos);
        if (isInt(v))
        {
            const auto x = toInt(v);
            if (n >= 0) return Value(x);
            if (n < -19) return Value(0);
            //to -19 digits: |x| above 5e18 rounds to 1e19, which an int64 cannot hold
            if (n == -19)
            {
                if (x > 5000000000000000000LL or x < -5000000000000000000LL) throw ExprError("integer overflow", pos);
                return Value(0);
            }
            std::int64_t p = 1;
            for (std::int64_t k = 0; k < -n; k++) p *= 10;
            auto q = x / p, r = x % p;
            if (r < 0)
            {
                r += p;
                q--;
            }
            if (2*r > p or (2*r == p and q % 2 != 0)) q++;
            return binaryOp("*", Value(q), Value(p), pos);
        }
        const double x = std::get<double>(v.data);
        if (not std::isfinite(x) or n > 300) return Value(x);
        if (n < -300) return Value(std::copysign(0.0, x));
        const double scale = std::pow(10.0, static_cast<double>(n));
        const double scaled = x * scale;
        if (not std::isfinite(scaled)) return Value(x);
        return Value(roundHalfEven(scaled) / scale);
    });
}

Value powFn(const Builtin &, std::span<const Value> args, const std::size_t pos)
{
    return binaryOp("**", args[0], args[1], pos);
}

Value atan2Fn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    requireReal(self, args[0], pos);
    requireReal(self, args[1], pos);
    return Value(std::atan2(toFloat(args[0]), toFloat(args[1])));
}

Value realFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        return Value(toComplex(v).real());
    });
}

Value imagFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        return Value(toComplex(v).imag());
    });
}

Value conjFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        return v.is<C>()? Value(std::conj(std::get<C>(v.data))) : v;
    });
}

Value argFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return mapList(args[0], [&](const Value &v)
    {
        requireNumber(self, v, pos);
        return Value(std::arg(toComplex(v)));
    });
}

/***********************************************************************
 * containers
 **********************************************************************/
Value minMax(const Builtin &self, std::span<const Value> args, const std::size_t pos, const std::string_view better)
{
    std::span<const Value> items = args;
    if (args.size() == 1)
    {
        const auto *list = std::get_if<List>(&args[0].data);
        if (list == nullptr) fail(self, "argument must be a list, not '" + std::string(args[0].typeName()) + "'", pos);
        items = *list;
    }
    if (items.empty()) fail(self, "arg is an empty sequence", pos);
    const Value *best = &items[0];
    for (const auto &item : items.subspan(1))
    {
        if (compareOp(better, item, *best, pos)) best = &item;
    }
    return *best;
}

Value minFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return minMax(self, args, pos, "<");
}

Value maxFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    return minMax(self, args, pos, ">");
}

Value sumFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    const auto *list = std::get_if<List>(&args[0].data);
    if (list == nullptr) fail(self, "argument must be a list, not '" + std::string(args[0].typeName()) + "'", pos);
    Value acc(0);
    for (const auto &item : *list) acc = binaryOp("+", acc, item, pos);
    return acc;
}

Value lenFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    const auto &v = args[0];
    if (const auto *list = std::get_if<List>(&v.data)) return Value(list->size());
    if (const auto *dict = std::get_if<Dict>(&v.data)) return Value(dict->size());
    if (const auto *str = std::get_if<std::string>(&v.data)) return Value(str->size());
    fail(self, "argument has no length: '" + std::string(v.typeName()) + "'", pos);
}

Value rangeFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    for (const auto &arg : args) requireInt(self, arg, pos);
    const std::int64_t start = (args.size() == 1)? 0 : toInt(args[0]);
    const std::int64_t stop = (args.size() == 1)? toInt(args[0]) : toInt(args[1]);
    const std::int64_t step = (args.size() == 3)? toInt(args[2]) : 1;
    if (step == 0) fail(self, "step must not be zero", pos);

    //length without overflow: unsigned distance divided by the unsigned step
    std::uint64_t length = 0;
    if (step > 0 and start < stop)
    {
        length = (std::uint64_t(stop) - std::uint64_t(start) - 1) / std::uint64_t(step) + 1;
    }
    if (step < 0 and start > stop)
    {
        length = (std::uint64_t(start) - std::uint64_t(stop) - 1) / (std::uint64_t(0) - std::uint64_t(step)) + 1;
    }
    if (length > MaxRangeLength) fail(self, "result is too long (more than " + std::to_string(MaxRangeLength) + " items)", pos);

    List out;
    out.reserve(static_cast<std::size_t>(length));
    for (std::uint64_t i = 0; i < length; i++)
    {
        out.emplace_back(static_cast<std::int64_t>(std::uint64_t(start) + i*std::uint64_t(step)));
    }
    return Value(std::move(out));
}

/***********************************************************************
 * conversions
 **********************************************************************/
//! Split an optional sign from trimmed text
std::string_view splitSign(std::string_view text, bool &negative)
{
    negative = text.starts_with('-');
    if (text.starts_with('-') or text.starts_with('+')) text.remove_prefix(1);
    return text;
}

Value intFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    const auto &v = args[0];
    if (isInt(v)) return Value(toInt(v));
    if (v.is<double>()) return Value(floatToInt(self, std::trunc(std::get<double>(v.data)), pos));
    const auto *str = std::get_if<std::string>(&v.data);
    if (str == nullptr) fail(self, "argument must be a real number or a string, not '" + std::string(v.typeName()) + "'", pos);

    const auto text = Pothos::Util::Detail::trim(*str);
    bool negative = false;
    const auto digits = splitSign(text, negative);
    std::uint64_t magnitude = 0;
    const auto result = std::from_chars(digits.data(), digits.data()+digits.size(), magnitude);
    if (digits.empty() or result.ptr != digits.data()+digits.size() or (result.ec != std::errc() and result.ec != std::errc::result_out_of_range))
    {
        fail(self, "invalid literal: " + v.repr(), pos);
    }
    const std::uint64_t limit = negative? std::uint64_t(1) << 63 : (std::uint64_t(1) << 63) - 1;
    if (result.ec == std::errc::result_out_of_range or magnitude > limit) throw ExprError("integer overflow", pos);
    return Value(negative? static_cast<std::int64_t>(std::uint64_t(0) - magnitude) : static_cast<std::int64_t>(magnitude));
}

//! Parse a float string: [sign] (digits [. digits] | . digits) [e [sign] digits] | inf | infinity | nan
bool parseFloatString(const std::string_view str, double &out)
{
    bool negative = false;
    const auto text = splitSign(Pothos::Util::Detail::trim(str), negative);
    const auto lower = Pothos::Util::Detail::toLower(text);
    const double sign = negative? -1.0 : 1.0;
    if (lower == "inf" or lower == "infinity")
    {
        out = sign*std::numeric_limits<double>::infinity();
        return true;
    }
    if (lower == "nan")
    {
        out = std::numeric_limits<double>::quiet_NaN();
        return true;
    }

    std::size_t i = 0;
    const auto digitRun = [&]
    {
        const auto start = i;
        while (i < text.size() and text[i] >= '0' and text[i] <= '9') i++;
        return text.substr(start, i-start);
    };
    const auto intPart = digitRun();
    std::string_view fracPart;
    if (i < text.size() and text[i] == '.')
    {
        i++;
        fracPart = digitRun();
    }
    if (intPart.empty() and fracPart.empty()) return false;
    std::string expPart;
    if (i < text.size() and (text[i] == 'e' or text[i] == 'E'))
    {
        i++;
        if (i < text.size() and (text[i] == '+' or text[i] == '-')) expPart += text[i++];
        const auto expDigits = digitRun();
        if (expDigits.empty()) return false;
        expPart += expDigits;
    }
    if (i != text.size()) return false;
    out = sign*parseDecimalFloat(intPart, fracPart, expPart);
    return true;
}

Value floatFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    const auto &v = args[0];
    if (const auto *str = std::get_if<std::string>(&v.data))
    {
        double out = 0.0;
        if (not parseFloatString(*str, out)) fail(self, "invalid literal: " + v.repr(), pos);
        return Value(out);
    }
    requireReal(self, v, pos);
    return Value(toFloat(v));
}

Value complexFn(const Builtin &self, std::span<const Value> args, const std::size_t pos)
{
    for (const auto &arg : args) requireNumber(self, arg, pos);
    const C re = toComplex(args[0]);
    const C im = (args.size() == 2)? toComplex(args[1]) : C();
    return Value(re + im*C(0.0, 1.0));
}

Value strFn(const Builtin &, std::span<const Value> args, const std::size_t)
{
    if (args[0].is<std::string>()) return args[0];
    return Value(args[0].repr());
}

Value boolFn(const Builtin &, std::span<const Value> args, const std::size_t)
{
    return Value(args[0].truthy());
}

/***********************************************************************
 * the table
 **********************************************************************/
constexpr auto Many = std::numeric_limits<std::size_t>::max();

double log2Real(const double x) {return std::log2(x);}
C log2Complex(const C &x) {return std::log(x) / std::numbers::ln2;}

const Builtin builtins[] = {
    {"abs", 1, 1, absFn},
    {"min", 1, Many, minFn},
    {"max", 1, Many, maxFn},
    {"sum", 1, 1, sumFn},
    {"len", 1, 1, lenFn},
    {"round", 1, 2, roundFn},
    {"floor", 1, 1, floorFn},
    {"ceil", 1, 1, ceilFn},
    {"sqrt", 1, 1, mathFn, [](double x){return std::sqrt(x);}, [](const C &x){return std::sqrt(x);}},
    {"exp", 1, 1, mathFn, [](double x){return std::exp(x);}, [](const C &x){return std::exp(x);}},
    {"log", 1, 2, logFn},
    {"ln", 1, 1, logFn},
    {"log2", 1, 1, mathFn, log2Real, log2Complex},
    {"log10", 1, 1, mathFn, [](double x){return std::log10(x);}, [](const C &x){return std::log10(x);}},
    {"sin", 1, 1, mathFn, [](double x){return std::sin(x);}, [](const C &x){return std::sin(x);}},
    {"cos", 1, 1, mathFn, [](double x){return std::cos(x);}, [](const C &x){return std::cos(x);}},
    {"tan", 1, 1, mathFn, [](double x){return std::tan(x);}, [](const C &x){return std::tan(x);}},
    {"asin", 1, 1, mathFn, [](double x){return std::asin(x);}, [](const C &x){return std::asin(x);}},
    {"acos", 1, 1, mathFn, [](double x){return std::acos(x);}, [](const C &x){return std::acos(x);}},
    {"atan", 1, 1, mathFn, [](double x){return std::atan(x);}, [](const C &x){return std::atan(x);}},
    {"atan2", 2, 2, atan2Fn},
    {"sinh", 1, 1, mathFn, [](double x){return std::sinh(x);}, [](const C &x){return std::sinh(x);}},
    {"cosh", 1, 1, mathFn, [](double x){return std::cosh(x);}, [](const C &x){return std::cosh(x);}},
    {"tanh", 1, 1, mathFn, [](double x){return std::tanh(x);}, [](const C &x){return std::tanh(x);}},
    {"pow", 2, 2, powFn},
    {"real", 1, 1, realFn},
    {"imag", 1, 1, imagFn},
    {"conj", 1, 1, conjFn},
    {"arg", 1, 1, argFn},
    {"int", 1, 1, intFn},
    {"float", 1, 1, floatFn},
    {"complex", 1, 2, complexFn},
    {"str", 1, 1, strFn},
    {"bool", 1, 1, boolFn},
    {"range", 1, 3, rangeFn},
};

const Builtin *findBuiltin(const std::string_view name)
{
    const auto it = std::ranges::find(builtins, name, &Builtin::name);
    return (it == std::end(builtins))? nullptr : it;
}

std::string argumentCount(const std::size_t n)
{
    return std::to_string(n) + ((n == 1)? " argument" : " arguments");
}

} //namespace

bool Pothos::Util::Expr::isBuiltinFunction(const std::string_view name)
{
    return findBuiltin(name) != nullptr;
}

Value Pothos::Util::Expr::callBuiltin(const std::string_view name, const std::span<const Value> args, const std::size_t pos)
{
    const auto *self = findBuiltin(name);
    if (self == nullptr) throw ExprError("unknown function '" + std::string(name) + "'", pos);
    if (args.size() < self->minArgs or args.size() > self->maxArgs)
    {
        std::string expected;
        if (self->minArgs == self->maxArgs) expected = argumentCount(self->minArgs);
        else if (self->maxArgs == Many) expected = "at least " + argumentCount(self->minArgs);
        else expected = std::to_string(self->minArgs) + " to " + argumentCount(self->maxArgs);
        fail(*self, "takes " + expected + " (" + std::to_string(args.size()) + " given)", pos);
    }
    return self->fn(*self, args, pos);
}
