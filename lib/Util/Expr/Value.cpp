// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Value.hpp"
#include <algorithm>
#include <cmath>
#include <format>

using namespace Pothos::Util::Expr;

namespace {

bool isNumber(const Value &v)
{
    return v.is<bool>() or v.is<std::int64_t>() or v.is<double>() or v.is<std::complex<double>>();
}

//exact comparison of an integer with a double (no rounding of large integers)
bool intEqualsDouble(const std::int64_t i, const double d)
{
    if (not std::isfinite(d) or d != std::floor(d)) return false;
    if (d < -9223372036854775808.0 or d >= 9223372036854775808.0) return false;
    return static_cast<std::int64_t>(d) == i;
}

//equality of two real numbers (bool, int or float)
bool realEquals(const Value &a, const Value &b)
{
    const auto asInt = [](const Value &v){return v.is<bool>()? std::int64_t(std::get<bool>(v.data)) : std::get<std::int64_t>(v.data);};
    const bool aFloat = a.is<double>(), bFloat = b.is<double>();
    if (aFloat and bFloat) return std::get<double>(a.data) == std::get<double>(b.data);
    if (aFloat) return intEqualsDouble(asInt(b), std::get<double>(a.data));
    if (bFloat) return intEqualsDouble(asInt(a), std::get<double>(b.data));
    return asInt(a) == asInt(b);
}

bool numberEquals(const Value &a, const Value &b)
{
    const bool aCplx = a.is<std::complex<double>>(), bCplx = b.is<std::complex<double>>();
    if (aCplx and bCplx) return std::get<std::complex<double>>(a.data) == std::get<std::complex<double>>(b.data);
    if (aCplx or bCplx)
    {
        const auto &c = std::get<std::complex<double>>((aCplx? a : b).data);
        if (c.imag() != 0.0) return false;
        return realEquals(Value(c.real()), aCplx? b : a);
    }
    return realEquals(a, b);
}

std::string floatRepr(const double d)
{
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d < 0? "-inf" : "inf";
    auto s = std::format("{}", d);
    if (s.find_first_of(".e") == std::string::npos) s += ".0";
    return s;
}

std::string strRepr(const std::string &s)
{
    std::string out("'");
    for (const char ch : s)
    {
        switch (ch)
        {
        case '\'': out += "\\'"; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20 or ch == 0x7f) out += std::format("\\x{:02x}", static_cast<unsigned char>(ch));
            else out += ch;
        }
    }
    return out + "'";
}

} //namespace

std::string_view Value::typeName(void) const
{
    static constexpr std::string_view names[] = {"None", "bool", "int", "float", "complex", "str", "list", "dict"};
    return names[data.index()];
}

bool Value::truthy(void) const
{
    return std::visit([](const auto &v) -> bool
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return false;
        else if constexpr (std::is_same_v<T, std::complex<double>>) return v != std::complex<double>();
        else if constexpr (std::is_same_v<T, std::string> or std::is_same_v<T, List> or std::is_same_v<T, Dict>) return not v.empty();
        else return v != T(0);
    }, data);
}

std::string Value::repr(void) const
{
    return std::visit([](const auto &v) -> std::string
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "None";
        else if constexpr (std::is_same_v<T, bool>) return v? "True" : "False";
        else if constexpr (std::is_same_v<T, std::int64_t>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return floatRepr(v);
        else if constexpr (std::is_same_v<T, std::complex<double>>)
        {
            auto im = floatRepr(v.imag());
            if (im.ends_with(".0")) im.resize(im.size()-2);
            if (v.real() == 0.0 and not std::signbit(v.real())) return im + "j";
            auto re = floatRepr(v.real());
            if (re.ends_with(".0")) re.resize(re.size()-2);
            const bool sign = im.front() == '-';
            return "(" + re + (sign? "" : "+") + im + "j)";
        }
        else if constexpr (std::is_same_v<T, std::string>) return strRepr(v);
        else if constexpr (std::is_same_v<T, List>)
        {
            std::string out("[");
            for (std::size_t i = 0; i < v.size(); i++) out += (i? ", " : "") + v[i].repr();
            return out + "]";
        }
        else
        {
            std::string out("{");
            for (std::size_t i = 0; i < v.size(); i++) out += (i? ", " : "") + v[i].first.repr() + ": " + v[i].second.repr();
            return out + "}";
        }
    }, data);
}

bool Value::operator==(const Value &other) const
{
    if (isNumber(*this) and isNumber(other)) return numberEquals(*this, other);
    if (data.index() != other.data.index()) return false;
    if (const auto *d = std::get_if<Dict>(&data))
    {
        const auto &o = std::get<Dict>(other.data);
        if (d->size() != o.size()) return false;
        return std::ranges::all_of(*d, [&o](const auto &kv)
        {
            const auto it = std::ranges::find_if(o, [&kv](const auto &okv){return okv.first == kv.first;});
            return it != o.end() and it->second == kv.second;
        });
    }
    if (const auto *l = std::get_if<List>(&data)) return *l == std::get<List>(other.data);
    return data == other.data;
}
