// Copyright (c) 2014-2017 Josh Blum
//                    2020 Nicholas Corgan
//                    2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Util/EvalEnvironment.hpp>
#include <Pothos/Exception.hpp>
#include "Util/Expr/Convert.hpp"
#include "Util/Expr/Eval.hpp"
#include "Util/Expr/Parser.hpp" //MaxTreeDepth
#include <array>
#include <algorithm>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>

using namespace Pothos::Util;

namespace {

//! Constant names are identifiers that are not keywords or literal names
bool isValidName(const std::string &name)
{
    static const std::array<std::string_view, 11> keywords{"and", "or", "not", "if", "else",
        "true", "false", "null", "True", "False", "None"};
    const auto isStart = [](const char ch){return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z') or ch == '_';};
    const auto isChar = [&isStart](const char ch){return isStart(ch) or (ch >= '0' and ch <= '9');};
    if (name.empty() or not isStart(name.front())) return false;
    if (not std::ranges::all_of(name, isChar)) return false;
    return std::ranges::find(keywords, name) == keywords.end();
}

//! The nesting depth of a value, counted up to limit+1 (no deeper recursion)
std::size_t valueDepth(const Expr::Value &value, const std::size_t limit)
{
    std::size_t depth = 0;
    const auto deeper = [&](const Expr::Value &v){depth = std::max(depth, (limit == 0)? 1 : valueDepth(v, limit-1)+1);};
    if (const auto *list = std::get_if<Expr::List>(&value.data)) for (const auto &v : *list) deeper(v);
    if (const auto *dict = std::get_if<Expr::Dict>(&value.data)) for (const auto &[k, v] : *dict) {deeper(k); deeper(v);}
    return depth;
}

void checkName(const std::string &where, const std::string &key)
{
    if (not isValidName(key)) throw Pothos::Exception(where, "invalid constant name '" + key + "'");
}

} //namespace

/***********************************************************************
 * Evaluator implementation
 **********************************************************************/
struct EvalEnvironment::Impl
{
    //! Evaluate with the constants under a shared lock (they are copied out by the evaluator)
    Expr::Value evaluate(const std::string &expr)
    {
        try
        {
            const auto tree = Expr::parse(expr);
            std::shared_lock lock(mutex);
            return Expr::evaluate(*tree, [this](const std::string_view name) -> const Expr::Value *
            {
                const auto it = constants.find(name);
                return (it == constants.end())? nullptr : &it->second;
            });
        }
        catch (const Expr::ExprError &ex)
        {
            throw Pothos::Exception("EvalEnvironment::eval",
                std::string(ex.what()) + " at column " + std::to_string(ex.pos+1) + " in: " + expr);
        }
        catch (const Pothos::Exception &) {throw;}
        catch (const std::exception &ex) //for example std::bad_alloc: also with the expression
        {
            throw Pothos::Exception("EvalEnvironment::eval", std::string(ex.what()) + " in: " + expr);
        }
    }

    void define(const std::string &key, Expr::Value value)
    {
        //a value made of constants can nest deeper than one expression: the same limit
        if (valueDepth(value, Expr::MaxTreeDepth) > Expr::MaxTreeDepth)
        {
            throw Pothos::Exception("EvalEnvironment::define", "the value is nested too deeply");
        }
        std::unique_lock lock(mutex);
        constants.insert_or_assign(key, std::move(value));
    }

    std::shared_mutex mutex;
    std::map<std::string, Expr::Value, std::less<>> constants;
};

std::shared_ptr<EvalEnvironment> EvalEnvironment::make(void)
{
    return std::make_shared<EvalEnvironment>();
}

EvalEnvironment::EvalEnvironment(void):
    _impl(std::make_shared<Impl>())
{
    return;
}

Pothos::Object EvalEnvironment::eval(const std::string &expr)
{
    return Expr::toObject(_impl->evaluate(expr));
}

void EvalEnvironment::registerConstantExpr(const std::string &key, const std::string &expr)
{
    const std::string where("EvalEnvironment::registerConstantExpr(" + key + ")");
    checkName(where, key);
    try
    {
        _impl->define(key, _impl->evaluate(expr));
    }
    catch (const Pothos::Exception &ex)
    {
        throw Pothos::Exception(where, ex.message());
    }
}

void EvalEnvironment::registerConstantObj(const std::string &key, const Pothos::Object &obj)
{
    const std::string where("EvalEnvironment::registerConstantObj(" + key + ")");
    checkName(where, key);
    try
    {
        _impl->define(key, Expr::fromObject(obj));
    }
    catch (const Pothos::Exception &ex)
    {
        throw Pothos::Exception(where, ex.message());
    }
    catch (const std::exception &ex)
    {
        throw Pothos::Exception(where, ex.what());
    }
}

void EvalEnvironment::unregisterConstant(const std::string &key)
{
    std::unique_lock lock(_impl->mutex);
    _impl->constants.erase(key);
}

#include <Pothos/Managed.hpp>

static auto managedEvalEnvironment = Pothos::ManagedClass()
    .registerConstructor<Pothos::Util::EvalEnvironment>()
    .registerStaticMethod(POTHOS_FCN_TUPLE(Pothos::Util::EvalEnvironment, make))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::Util::EvalEnvironment, eval))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::Util::EvalEnvironment, registerConstantExpr))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::Util::EvalEnvironment, registerConstantObj))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::Util::EvalEnvironment, unregisterConstant))
    .commit("Pothos/Util/EvalEnvironment");
