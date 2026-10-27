///
/// \file Util/EvalEnvironment.hpp
///
/// Expression evaluation utilities.
///
/// \copyright
/// Copyright (c) 2014-2016 Josh Blum
///                    2026 Pothos contributors
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Object.hpp>
#include <string>
#include <memory>

namespace Pothos {
namespace Util {

/*!
 * The evaluation environment evaluates expressions with a Python-like syntax.
 * The syntax is a superset of JSON: any JSON value is a valid expression.
 *
 * Values: None (null), bool (True/true, False/false), int (64-bit),
 * float (double), complex (3j, j is the imaginary unit), str ('...' or "..."),
 * list ([1, 2]) and dict ({'key': value}).
 *
 * Literals: 42, 0x2A, 0o52, 0b101010, 1_000, 1.5, .5, 1e-05, 2j;
 * a decimal integer with a leading zero (0315) is an error, use 0o315;
 * a decimal integer beyond the 64-bit range is a float.
 * String escapes: \" \' \\ \/ \n \t \r \b \f \0 \xHH \uXXXX.
 *
 * Operators by increasing precedence:
 *  - a if cond else b
 *  - or, and, not (short-circuit, return an operand like Python)
 *  - comparisons < > == != <= >= (chained: 0 < x < 10)
 *  - | then ^ (xor) then &, shifts << >>
 *  - + -, then * / // %, then unary - + ~, then ** (power, right-associative)
 *  - calls f(x), indexing x[i] (negative indexes count from the end)
 *
 * Differences from Python:
 *  - integer overflow is an error (no big integers);
 *  - arithmetic and bitwise operators apply element-wise to lists:
 *    [1, 2] + [3, 4] == [4, 6], 2 * [1, 2] == [2, 4];
 *  - no slices, tuples, attributes, lambdas or assignments.
 *
 * Built-in constants: pi, e, inf, nan, j (a registered constant with the
 * same name takes precedence). Built-in functions: abs, min, max, sum, len,
 * round, floor, ceil, sqrt, exp, log(x[, base]), ln, log2, log10, sin, cos,
 * tan, asin, acos, atan, atan2, sinh, cosh, tanh, pow, real, imag, conj, arg,
 * int, float, complex, str, bool, range. Math functions apply element-wise
 * to lists.
 *
 * Results: None is a null Object, int is long long, float is double,
 * complex is std::complex<double>, str is std::string, list is a ProxyVector
 * and dict is a ProxyMap.
 *
 * An unknown name is always an error. All methods are thread-safe.
 */
class POTHOS_API EvalEnvironment
{
public:

    /*!
     * Create a new empty eval environment in a shared ptr.
     * This is a convenience factory for EvalEnvironment.
     */
    static std::shared_ptr<EvalEnvironment> make(void);

    /*!
     * Create an empty eval environment.
     * The environment can evaluate expressions with eval() and can
     * be extended with new constants using the registerConstant calls.
     */
    EvalEnvironment(void);

    /*!
     * Try to evaluate an expression in this environment.
     * \param expression a string expression
     * \throws Pothos::Exception with the evaluation error message
     * \return a proxy object representing the evaluated expression
     */
    Pothos::Object eval(const std::string &expression);

    /*!
     * Register a constant that will be used in other expressions.
     * For an expression like 2*x, then "x" would be a constant.
     * The key must be an identifier that is not a keyword.
     * \throws Pothos::Exception when the expression fails to evaluate
     */
    void registerConstantExpr(const std::string &key, const std::string &expr);

    /*!
     * Register a constant that will be used in other expressions.
     * Supported: null Object, strings, bool, numbers, and objects that
     * convert to ProxyVector or ProxyMap (recursively).
     * \throws Pothos::Exception for an unsupported object type
     */
    void registerConstantObj(const std::string &key, const Pothos::Object &obj);

    /*!
     * Unregister a previously registered constant.
     */
    void unregisterConstant(const std::string &key);

private:
    struct Impl; std::shared_ptr<Impl> _impl;
};

} //namespace Util
} //namespace Pothos
