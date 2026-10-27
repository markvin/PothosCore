// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Eval.hpp"
#include "Util/Expr/Ops.hpp"
#include <limits>
#include <numbers>
#include <string>

using namespace Pothos::Util::Expr;

namespace {

//! Built-in constants, used when the lookup does not define the name
const Value *builtinConstant(const std::string_view name)
{
    static const std::pair<std::string_view, Value> constants[] = {
        {"True", Value(true)}, {"true", Value(true)},
        {"False", Value(false)}, {"false", Value(false)},
        {"None", Value()}, {"null", Value()},
        {"j", Value(std::complex<double>(0.0, 1.0))},
        {"pi", Value(std::numbers::pi)},
        {"e", Value(std::numbers::e)},
        {"inf", Value(std::numeric_limits<double>::infinity())},
        {"nan", Value(std::numeric_limits<double>::quiet_NaN())},
    };
    for (const auto &[key, value] : constants)
    {
        if (key == name) return &value;
    }
    return nullptr;
}

class Evaluator
{
public:
    Evaluator(const Lookup &lookup):
        _lookup(lookup)
    {
        return;
    }

    Value eval(const Node &node) const
    {
        const auto &c = node.children;
        switch (node.kind)
        {
        case Node::Kind::Literal: return node.literal;
        case Node::Kind::Name: return this->name(node);
        case Node::Kind::Unary: return unaryOp(node.op, this->eval(*c[0]), node.pos);
        case Node::Kind::Binary:
        {
            const auto a = this->eval(*c[0]);
            return binaryOp(node.op, a, this->eval(*c[1]), node.pos);
        }
        case Node::Kind::Compare:
        {
            auto left = this->eval(*c[0]);
            for (std::size_t i = 0; i < node.ops.size(); i++)
            {
                auto right = this->eval(*c[i+1]);
                if (not compareOp(node.ops[i], left, right, node.pos)) return Value(false);
                left = std::move(right);
            }
            return Value(true);
        }
        case Node::Kind::And:
        case Node::Kind::Or:
        {
            //short-circuit, the result is the deciding operand
            const bool stopOn = node.kind == Node::Kind::Or;
            Value v;
            for (const auto &child : c)
            {
                v = this->eval(*child);
                if (v.truthy() == stopOn) break;
            }
            return v;
        }
        case Node::Kind::Not: return Value(not this->eval(*c[0]).truthy());
        case Node::Kind::IfElse: return this->eval(*c[this->eval(*c[0]).truthy()? 1 : 2]);
        case Node::Kind::List:
        case Node::Kind::Dict:
        case Node::Kind::Call:
        case Node::Kind::Index: break;
        }
        throw ExprError("not implemented", node.pos);
    }

private:
    Value name(const Node &node) const
    {
        const Value *v = _lookup? _lookup(node.name) : nullptr;
        if (v == nullptr) v = builtinConstant(node.name);
        if (v == nullptr) throw ExprError("unknown name '" + node.name + "'", node.pos);
        return *v;
    }

    const Lookup &_lookup;
};

} //namespace

Value Pothos::Util::Expr::evaluate(const Node &node, const Lookup &lookup)
{
    return Evaluator(lookup).eval(node);
}

Value Pothos::Util::Expr::evaluate(const std::string_view expr, const Lookup &lookup)
{
    return evaluate(*parse(expr), lookup);
}
