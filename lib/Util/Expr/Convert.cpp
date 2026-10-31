// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/Expr/Convert.hpp"
#include "Util/Expr/Parser.hpp" //MaxTreeDepth
#include <Pothos/Exception.hpp>
#include <Pothos/Framework/DType.hpp>
#include <Pothos/Object/Containers.hpp>
#include <Pothos/Proxy.hpp>
#include <limits>

using namespace Pothos::Util::Expr;

Pothos::Object Pothos::Util::Expr::toObject(const Value &value)
{
    if (const auto *list = std::get_if<List>(&value.data))
    {
        auto env = Pothos::ProxyEnvironment::make("managed");
        Pothos::ProxyVector vec;
        vec.reserve(list->size());
        for (const auto &item : *list) vec.push_back(env->convertObjectToProxy(toObject(item)));
        return Pothos::Object(std::move(vec));
    }
    if (const auto *dict = std::get_if<Dict>(&value.data))
    {
        auto env = Pothos::ProxyEnvironment::make("managed");
        Pothos::ProxyMap map;
        for (const auto &[key, val] : *dict)
        {
            map[env->convertObjectToProxy(toObject(key))] = env->convertObjectToProxy(toObject(val));
        }
        return Pothos::Object(std::move(map));
    }
    return std::visit([](const auto &v) -> Pothos::Object
    {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) return Pothos::Object();
        else if constexpr (std::is_same_v<T, std::int64_t>) return Pothos::Object(static_cast<long long>(v));
        else if constexpr (std::is_same_v<T, List> or std::is_same_v<T, Dict>) return Pothos::Object(); //handled above
        else return Pothos::Object(v);
    }, value.data);
}

//depth: the nesting of the containers, limited as the parser limits a tree
static Value fromObjectDepth(const Pothos::Object &object, const std::size_t depth)
{
    if (depth > MaxTreeDepth) throw Pothos::Exception("the value is nested too deeply");
    if (not object) return Value();
    const auto &type = object.type();
    if (type == typeid(std::string)) return Value(object.extract<std::string>());
    if (type == typeid(bool)) return Value(object.extract<bool>());

    //numbers by their data type
    bool isNumber = false;
    Pothos::DType dtype;
    try
    {
        dtype = Pothos::DType(type);
        isNumber = not dtype.isCustom() and dtype.dimension() == 1;
    }
    catch (const Pothos::Exception &) {}
    if (isNumber)
    {
        if (dtype.isComplex()) return Value(object.convert<std::complex<double>>());
        if (dtype.isFloat()) return Value(object.convert<double>());
        if (dtype.isInteger() and (dtype.isSigned() or dtype.elemSize() < sizeof(std::int64_t)))
        {
            return Value(object.convert<long long>());
        }
        if (dtype.isInteger())
        {
            const auto u = object.convert<unsigned long long>();
            if (u <= static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max())) return Value(static_cast<std::int64_t>(u));
            return Value(static_cast<double>(u));
        }
    }

    if (object.canConvert(typeid(Pothos::ProxyVector)))
    {
        const auto vec = object.convert<Pothos::ProxyVector>();
        List list;
        list.reserve(vec.size());
        for (const auto &item : vec) list.push_back(fromObjectDepth(item.toObject(), depth+1));
        return Value(std::move(list));
    }
    if (object.canConvert(typeid(Pothos::ProxyMap)))
    {
        const auto map = object.convert<Pothos::ProxyMap>();
        Dict dict;
        for (const auto &[key, val] : map) dict.emplace_back(fromObjectDepth(key.toObject(), depth+1), fromObjectDepth(val.toObject(), depth+1));
        return Value(std::move(dict));
    }

    throw Pothos::Exception("unsupported type " + object.getTypeString());
}

Value Pothos::Util::Expr::fromObject(const Pothos::Object &object)
{
    return fromObjectDepth(object, 0);
}
