// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Eval.hpp"
#include "Util/Expr/Ops.hpp"
#include <algorithm>
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
        {
            List list;
            list.reserve(c.size());
            for (const auto &child : c) list.push_back(this->eval(*child));
            return Value(std::move(list));
        }
        case Node::Kind::Dict: return this->dict(node);
        case Node::Kind::Index: return this->index(node);
        case Node::Kind::Call: break;
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

    Value dict(const Node &node) const
    {
        Dict dict;
        for (std::size_t i = 0; i < node.children.size(); i += 2)
        {
            auto key = this->eval(*node.children[i]);
            if (key.is<List>() or key.is<Dict>())
            {
                throw ExprError("unhashable type: '" + std::string(key.typeName()) + "'", node.children[i]->pos);
            }
            auto value = this->eval(*node.children[i+1]);
            //a repeated key keeps its first position and takes the last value
            const auto it = std::ranges::find_if(dict, [&key](const auto &kv){return kv.first == key;});
            if (it != dict.end()) it->second = std::move(value);
            else dict.emplace_back(std::move(key), std::move(value));
        }
        return Value(std::move(dict));
    }

    Value index(const Node &node) const
    {
        const auto container = this->eval(*node.children[0]);
        const auto &indexNode = *node.children[1];
        const auto key = this->eval(indexNode);

        if (const auto *dict = std::get_if<Dict>(&container.data))
        {
            const auto it = std::ranges::find_if(*dict, [&key](const auto &kv){return kv.first == key;});
            if (it == dict->end()) throw ExprError("key " + key.repr() + " not found", indexNode.pos);
            return it->second;
        }

        const auto *list = std::get_if<List>(&container.data);
        const auto *str = std::get_if<std::string>(&container.data);
        if (list == nullptr and str == nullptr)
        {
            throw ExprError("'" + std::string(container.typeName()) + "' object is not subscriptable", node.pos);
        }
        if (not key.is<std::int64_t>() and not key.is<bool>())
        {
            throw ExprError(std::string(container.typeName()) + " indices must be integers, not '" +
                std::string(key.typeName()) + "'", indexNode.pos);
        }
        const auto size = static_cast<std::int64_t>((list != nullptr)? list->size() : str->size());
        auto i = key.is<bool>()? std::int64_t(std::get<bool>(key.data)) : std::get<std::int64_t>(key.data);
        if (i < 0) i += size;
        if (i < 0 or i >= size) throw ExprError(std::string(container.typeName()) + " index out of range", indexNode.pos);
        if (list != nullptr) return (*list)[static_cast<std::size_t>(i)];
        return Value(std::string(1, (*str)[static_cast<std::size_t>(i)]));
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
