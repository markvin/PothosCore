// Copyright (c) 2014-2017 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/Object/Containers.hpp>
#include <Pothos/Util/EvalEnvironment.hpp>
#include <atomic>
#include <complex>
#include <cstdint>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

POTHOS_TEST_BLOCK("/util/tests", test_eval_expression)
{
    //check that the following does not throw
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto EvalEnvironment = env->findProxy("Pothos/Util/EvalEnvironment");
    auto evalEnv = EvalEnvironment();

    //booleans
    const auto resultT = evalEnv.call<Pothos::Object>("eval", "true");
    POTHOS_TEST_TRUE(resultT.convert<bool>());

    const auto resultF = evalEnv.call<Pothos::Object>("eval", "false");
    POTHOS_TEST_FALSE(resultF.convert<bool>());

    //simple expression
    const auto result = evalEnv.call<Pothos::Object>("eval", "1 + 2");
    POTHOS_TEST_EQUAL(int(result), 3);

    //a pothos type
    //const auto result2 = evalEnv.call<Pothos::Object>("eval", "DType(\"int32\")");
    //POTHOS_TEST_TRUE((const Pothos::DType &)result2 == Pothos::DType(typeid(int)));

    //test string w/ escape quote
    const auto result3 = evalEnv.call<Pothos::Object>("eval", "\"hello \\\" world\"");
    POTHOS_TEST_EQUAL(result3.convert<std::string>(), "hello \" world");
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_list_expression)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto evalEnv = env->findProxy("Pothos/Util/EvalEnvironment")();

    //the empty test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[]");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 0);
    }

    //a simple test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[1, 2, 3]");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(vec[0], 1);
        POTHOS_TEST_EQUAL(vec[1], 2);
        POTHOS_TEST_EQUAL(vec[2], 3);
    }

    //array math
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "(2 * [1, 2, 3]) + [3, 2, 1]");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(vec[0], 2*1 + 3);
        POTHOS_TEST_EQUAL(vec[1], 2*2 + 2);
        POTHOS_TEST_EQUAL(vec[2], 2*3 + 1);
    }

    //a trailing comma test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[1, ]");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 1);
        POTHOS_TEST_EQUAL(vec[0], 1);
    }

    //a quote test (including commas and escapes)
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[\"comma, \\\"comma, comma, \", \"chameleon\"]");
        const std::vector<std::string> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 2);
        POTHOS_TEST_EQUAL(vec[0], "comma, \"comma, comma, ");
        POTHOS_TEST_EQUAL(vec[1], "chameleon");
    }

    //a nested test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[1, [\"hello\", \"world\"], 3]");
        const Pothos::ObjectVector vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(int(vec[0]), 1);
        const std::vector<std::string> vec_1 = vec[1];
        POTHOS_TEST_EQUAL(vec_1.size(), 2);
        POTHOS_TEST_EQUAL(vec_1[0], "hello");
        POTHOS_TEST_EQUAL(vec_1[1], "world");
        POTHOS_TEST_EQUAL(int(vec[2]), 3);
    }

    //an embedded function with commas
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "[pow(2, 3)]");
        const Pothos::ObjectVector vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 1);
        POTHOS_TEST_CLOSE(double(vec[0]), 8, 0.001);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_map_expression)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto evalEnv = env->findProxy("Pothos/Util/EvalEnvironment")();

    //the empty test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "{}");
        const Pothos::ObjectMap map = result;
        POTHOS_TEST_EQUAL(map.size(), 0);
    }

    //a simple test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "{\"hello\" : 1, \"world\" : 2}");
        const Pothos::ObjectMap map = result;
        POTHOS_TEST_EQUAL(map.size(), 2);
        POTHOS_TEST_EQUAL(int(map.at(Pothos::Object("hello"))), 1);
        POTHOS_TEST_EQUAL(int(map.at(Pothos::Object("world"))), 2);
    }

    //a trailing comma test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "{1:2, }");
        const Pothos::ObjectMap map = result;
        POTHOS_TEST_EQUAL(map.size(), 1);
        POTHOS_TEST_EQUAL(int(map.at(Pothos::Object(1))), 2);
    }

    //a nested test
    {
        const auto result = evalEnv.call<Pothos::Object>("eval", "{\"hello\" : 1, \"world\" : [1, 2, 3]}");
        const Pothos::ObjectMap map = result;
        POTHOS_TEST_EQUAL(map.size(), 2);
        POTHOS_TEST_EQUAL(int(map.at(Pothos::Object("hello"))), 1);
        const std::vector<int> vec_1 = map.at(Pothos::Object("world"));
        POTHOS_TEST_EQUAL(vec_1.size(), 3);
        POTHOS_TEST_EQUAL(vec_1[0], 1);
        POTHOS_TEST_EQUAL(vec_1[1], 2);
        POTHOS_TEST_EQUAL(vec_1[2], 3);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_with_constants)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto evalEnv = env->findProxy("Pothos/Util/EvalEnvironment")();

    //simple test
    {
        evalEnv.call<Pothos::Object>("registerConstantExpr", "x", "1");
        evalEnv.call<Pothos::Object>("registerConstantExpr", "y", "2");
        const auto result = evalEnv.call<Pothos::Object>("eval", "x + y");
        POTHOS_TEST_EQUAL(int(result), 3);
    }

    //array math
    {
        evalEnv.call<Pothos::Object>("registerConstantExpr", "arr", "[1, 2, 3]");
        const auto result = evalEnv.call<Pothos::Object>("eval", "2*arr");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(vec[0], 2);
        POTHOS_TEST_EQUAL(vec[1], 4);
        POTHOS_TEST_EQUAL(vec[2], 6);
    }

    //nested lists
    {
        evalEnv.call<Pothos::Object>("registerConstantExpr", "nested", "[1, [\"hello\", \"world\"], 3]");
        const auto result = evalEnv.call<Pothos::Object>("eval", "nested");
        const Pothos::ObjectVector vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(int(vec[0]), 1);
        const std::vector<std::string> vec_1 = vec[1];
        POTHOS_TEST_EQUAL(vec_1.size(), 2);
        POTHOS_TEST_EQUAL(vec_1[0], "hello");
        POTHOS_TEST_EQUAL(vec_1[1], "world");
        POTHOS_TEST_EQUAL(int(vec[2]), 3);
    }

    //nested dict
     {
        evalEnv.call<Pothos::Object>("registerConstantExpr", "nested", "{\"hello\" : 1, \"world\" : [1, 2, 3]}");
        const auto result = evalEnv.call<Pothos::Object>("eval", "nested");
        const Pothos::ObjectMap map = result;
        POTHOS_TEST_EQUAL(map.size(), 2);
        POTHOS_TEST_EQUAL(int(map.at(Pothos::Object("hello"))), 1);
        const std::vector<int> vec_1 = map.at(Pothos::Object("world"));
        POTHOS_TEST_EQUAL(vec_1.size(), 3);
        POTHOS_TEST_EQUAL(vec_1[0], 1);
        POTHOS_TEST_EQUAL(vec_1[1], 2);
        POTHOS_TEST_EQUAL(vec_1[2], 3);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_constant_obj)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto evalEnv = env->findProxy("Pothos/Util/EvalEnvironment")();

    //short type
    {
        const auto arg = short(123);
        evalEnv.call<Pothos::Object>("registerConstantObj", "v0", arg);
        const auto result = evalEnv.call<Pothos::Object>("eval", "v0");
        POTHOS_TEST_EQUAL(short(result), arg);
    }

    //float type
    {
        const auto arg = float(-10.0);
        evalEnv.call<Pothos::Object>("registerConstantObj", "v1", arg);
        const auto result = evalEnv.call<Pothos::Object>("eval", "v1");
        POTHOS_TEST_EQUAL(float(result), arg);
    }

    //complex float
    {
        const auto arg = std::complex<float>(11.0, -32.0);
        evalEnv.call<Pothos::Object>("registerConstantObj", "v2", arg);
        const auto result = evalEnv.call<Pothos::Object>("eval", "v2");
        POTHOS_TEST_EQUAL(result.convert<std::complex<float>>(), arg);
    }

    //long long type
    {
        const auto arg = (long long)(17179869184ll);
        evalEnv.call<Pothos::Object>("registerConstantObj", "v3", arg);
        const auto result = evalEnv.call<Pothos::Object>("eval", "v3");
        POTHOS_TEST_EQUAL(result.convert<long long>(), arg);
    }

    //numeric vector
    {
        auto arg = std::vector<int>();
        arg.push_back(1);
        arg.push_back(2);
        arg.push_back(3);
        evalEnv.call<Pothos::Object>("registerConstantObj", "v4", arg);
        const auto result = evalEnv.call<Pothos::Object>("eval", "v4");
        const std::vector<int> vec = result;
        POTHOS_TEST_EQUAL(vec.size(), 3);
        POTHOS_TEST_EQUAL(vec[0], 1);
        POTHOS_TEST_EQUAL(vec[1], 2);
        POTHOS_TEST_EQUAL(vec[2], 3);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_octal_constant)
{
    const std::string octalStr = "0o315";
    const int octalVal = 0315;

    auto env = Pothos::ProxyEnvironment::make("managed");
    auto EvalEnvironment = env->findProxy("Pothos/Util/EvalEnvironment");
    auto evalEnv = EvalEnvironment();

    auto result = evalEnv.call<Pothos::Object>("eval", octalStr);
    POTHOS_TEST_EQUAL(octalVal, result.convert<int>());
}

namespace {

using json = nlohmann::json;

//! Does an evaluated object match the JSON value it was printed from?
bool matchesJson(const Pothos::Object &obj, const json &j)
{
    if (j.is_null()) return not obj;
    if (j.is_boolean()) return obj.type() == typeid(bool) and obj.extract<bool>() == j.get<bool>();
    if (j.is_number_integer()) return obj.type() == typeid(long long) and obj.extract<long long>() == j.get<long long>();
    if (j.is_number_float()) return obj.type() == typeid(double) and obj.extract<double>() == j.get<double>();
    if (j.is_string()) return obj.type() == typeid(std::string) and obj.extract<std::string>() == j.get<std::string>();
    if (j.is_array())
    {
        if (obj.type() != typeid(Pothos::ProxyVector)) return false;
        const auto &vec = obj.extract<Pothos::ProxyVector>();
        if (vec.size() != j.size()) return false;
        for (size_t i = 0; i < vec.size(); i++)
        {
            if (not matchesJson(vec[i].toObject(), j[i])) return false;
        }
        return true;
    }
    if (obj.type() != typeid(Pothos::ProxyMap)) return false;
    const auto &map = obj.extract<Pothos::ProxyMap>();
    if (map.size() != j.size()) return false;
    for (const auto &pair : map)
    {
        const auto key = pair.first.toObject().extract<std::string>();
        if (not j.contains(key) or not matchesJson(pair.second.toObject(), j[key])) return false;
    }
    return true;
}

std::string evalErrorMessage(Pothos::Util::EvalEnvironment &env, const std::string &expr)
{
    try
    {
        env.eval(expr);
    }
    catch (const Pothos::Exception &ex)
    {
        return ex.displayText();
    }
    throw std::runtime_error("no exception for " + expr);
}

} //namespace

POTHOS_TEST_BLOCK("/util/tests", test_eval_json_superset)
{
    Pothos::Util::EvalEnvironment env;
    const std::vector<json> values{
        json(-3), json(0), json(1e-05), json(-2.5), json(1.7976931348623157e308), json(5e-324),
        json(true), json(false), json(nullptr), json(std::numeric_limits<long long>::min()),
        json(std::numeric_limits<long long>::max()),
        json("tab\tquote\"\xc3\xa9 back\\slash /"), json("\x01\x1f"), json(""),
        json::parse(R"([1, [2.5, "x"], {}])"),
        json::parse(R"({"a": {"b": [true, null]}, "c": "d"})"),
        json::array(), json::object(),
    };
    for (const auto &j : values)
    {
        std::cout << "  " << j.dump() << std::endl;
        POTHOS_TEST_TRUE(matchesJson(env.eval(j.dump()), j));
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_json_uint64)
{
    //JSON integers above INT64_MAX evaluate to double
    Pothos::Util::EvalEnvironment env;
    const auto obj = env.eval(json(std::numeric_limits<std::uint64_t>::max()).dump());
    POTHOS_TEST_TRUE(obj.type() == typeid(double));
    POTHOS_TEST_EQUAL(obj.extract<double>(), 18446744073709551615.0);
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_unknown_name_throws)
{
    Pothos::Util::EvalEnvironment env;
    for (const auto expr : {"int32", "hello", "float32", "complex_float32"})
    {
        POTHOS_TEST_THROWS(env.eval(expr), Pothos::Exception);
        const auto msg = evalErrorMessage(env, expr);
        POTHOS_TEST_TRUE(msg.find("unknown name") != std::string::npos);
        POTHOS_TEST_TRUE(msg.find("column 1") != std::string::npos);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_topology_doc_example)
{
    Pothos::Util::EvalEnvironment env;
    env.registerConstantExpr("fiz", "3");
    POTHOS_TEST_EQUAL(env.eval("-fiz").convert<int>(), -3);
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_object_roundtrip)
{
    Pothos::Util::EvalEnvironment env;

    env.registerConstantObj("strs", Pothos::Object(std::vector<std::string>{"a", "b"}));
    POTHOS_TEST_EQUAL(env.eval("strs[1] + strs[0]").extract<std::string>(), "ba");

    Pothos::ProxyMap map;
    auto proxyEnv = Pothos::ProxyEnvironment::make("managed");
    map[proxyEnv->makeProxy(std::string("k"))] = proxyEnv->makeProxy(5);
    env.registerConstantObj("m", Pothos::Object(map));
    POTHOS_TEST_EQUAL(env.eval("m['k'] * 2").convert<int>(), 10);

    env.registerConstantObj("big", Pothos::Object(std::numeric_limits<std::uint64_t>::max()));
    POTHOS_TEST_TRUE(env.eval("big").type() == typeid(double));
    env.registerConstantObj("small", Pothos::Object(std::uint64_t(7)));
    POTHOS_TEST_TRUE(env.eval("small").type() == typeid(long long));

    env.registerConstantObj("c", Pothos::Object(std::complex<float>(1.0f, -2.0f)));
    POTHOS_TEST_TRUE(env.eval("conj(c)").extract<std::complex<double>>() == std::complex<double>(1.0, 2.0));

    env.registerConstantObj("flag", Pothos::Object(true));
    POTHOS_TEST_TRUE(env.eval("flag").type() == typeid(bool));

    env.registerConstantObj("x", Pothos::Object());
    POTHOS_TEST_TRUE(env.eval("x == None").extract<bool>());
    POTHOS_TEST_TRUE(not env.eval("x"));

    try
    {
        env.registerConstantObj("dt", Pothos::Object(Pothos::DType("int32")));
        POTHOS_TEST_TRUE(false);
    }
    catch (const Pothos::Exception &ex)
    {
        POTHOS_TEST_TRUE(ex.displayText().find("DType") != std::string::npos);
        POTHOS_TEST_TRUE(ex.displayText().find("dt") != std::string::npos);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_constant_name_rules)
{
    Pothos::Util::EvalEnvironment env;
    POTHOS_TEST_THROWS(env.registerConstantExpr("if", "1"), Pothos::Exception);
    POTHOS_TEST_THROWS(env.registerConstantExpr("1x", "1"), Pothos::Exception);
    POTHOS_TEST_THROWS(env.registerConstantExpr("a b", "1"), Pothos::Exception);
    POTHOS_TEST_THROWS(env.registerConstantObj("", Pothos::Object(1)), Pothos::Exception);
    env.unregisterConstant("missing");

    env.registerConstantExpr("x", "1");
    env.registerConstantExpr("x", "x + 1");
    POTHOS_TEST_EQUAL(env.eval("x").convert<int>(), 2);
    env.unregisterConstant("x");
    POTHOS_TEST_THROWS(env.eval("x"), Pothos::Exception);

    try
    {
        env.registerConstantExpr("y", "1 +");
        POTHOS_TEST_TRUE(false);
    }
    catch (const Pothos::Exception &ex)
    {
        POTHOS_TEST_TRUE(ex.displayText().find("y") != std::string::npos);
    }
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_concurrent)
{
    Pothos::Util::EvalEnvironment env;
    env.registerConstantExpr("x", "0");
    std::atomic<int> failures(0);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++)
    {
        threads.emplace_back([&env, &failures, t]
        {
            for (int i = 0; i < 2000; i++)
            {
                try
                {
                    if ((i+t) % 2 == 0) env.registerConstantExpr("x", std::to_string(i));
                    const auto result = env.eval("x * [1, 2, 3]");
                    if (result.extract<Pothos::ProxyVector>().size() != 3) failures++;
                }
                catch (...)
                {
                    failures++;
                }
            }
        });
    }
    for (auto &thread : threads) thread.join();
    POTHOS_TEST_EQUAL(failures.load(), 0);
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_error_message)
{
    Pothos::Util::EvalEnvironment env;
    const auto msg = evalErrorMessage(env, "1 + * 2");
    POTHOS_TEST_TRUE(msg.find("column 5") != std::string::npos);
    POTHOS_TEST_TRUE(msg.find("1 + * 2") != std::string::npos);
    POTHOS_TEST_THROWS(env.eval("   "), Pothos::Exception);
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_reserved_constant_names)
{
    //the literal names cannot become constants (they would hide the literals)
    Pothos::Util::EvalEnvironment env;
    for (const char *name : {"true", "false", "null", "True", "False", "None"})
    {
        POTHOS_TEST_THROWS(env.registerConstantExpr(name, "0"), Pothos::Exception);
    }
    env.registerConstantExpr("truth", "1");
    POTHOS_TEST_EQUAL(env.eval("truth").convert<int>(), 1);
}

POTHOS_TEST_BLOCK("/util/tests", test_eval_constant_depth)
{
    //values made of constants have a depth limit: an error, not a stack overflow
    Pothos::Util::EvalEnvironment env;
    env.registerConstantExpr("a0", "0");
    bool threw = false;
    for (int i = 1; i <= 2000 and not threw; i++)
    {
        try
        {
            env.registerConstantExpr("a" + std::to_string(i), "[a" + std::to_string(i-1) + "]");
        }
        catch (const Pothos::Exception &ex)
        {
            std::cout << "depth " << i << ": " << ex.displayText() << std::endl;
            threw = true;
        }
    }
    POTHOS_TEST_TRUE(threw);
}
