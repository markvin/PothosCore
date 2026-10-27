// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Exception.hpp>
#include "App/Options.hpp"
#include "App/PothosUtilOptions.hpp"
#include <string>
#include <utility>
#include <vector>

using namespace Pothos::App;

namespace {

using Given = std::vector<std::pair<std::string, std::string>>;

const std::vector<OptionSpec> &testSpecs(void)
{
    static const std::vector<OptionSpec> specs{
        {"help", 'h', Arg::None, "", "display help"},
        {"a", 0, Arg::Required, "A", "required value"},
        {"b", 0, Arg::Required, "B", "required value"},
        {"c", 0, Arg::Required, "C", "required value"},
        {"opt", 0, Arg::Optional, "O", "optional value"},
        {"flag", 0, Arg::None, "", "a flag"},
        {"var", 0, Arg::Required, "name:value", "repeatable", true},
        {"n", 0, Arg::Required, "N", "a number"},
        {"secret", 0, Arg::None, "", "hidden", false, true},
    };
    return specs;
}

//! The real options of PothosUtil
const std::vector<OptionSpec> &coreSpecs(void)
{
    static const auto specs = pothosUtilOptions();
    return specs;
}

CommandLine parse(const std::vector<std::string> &args, const std::vector<OptionSpec> &specs = testSpecs())
{
    return parseCommandLine(specs, args);
}

} //namespace

POTHOS_TEST_BLOCK("/app/tests", test_app_parse_forms)
{
    const auto cl = parse({"--a=1", "--b", "2", "--c=x=y", "-h", "--flag", "pos1", "--", "--not-an-option"});
    POTHOS_TEST_TRUE(cl.given() == Given({{"a", "1"}, {"b", "2"}, {"c", "x=y"}, {"help", ""}, {"flag", ""}}));
    POTHOS_TEST_TRUE(cl.positional() == std::vector<std::string>({"pos1", "--not-an-option"}));
    POTHOS_TEST_TRUE(cl.has("flag"));
    POTHOS_TEST_TRUE(cl.hasOption("a"));
    POTHOS_TEST_TRUE(not cl.has("opt"));
    POTHOS_TEST_EQUAL(cl.getString("c"), "x=y");
    POTHOS_TEST_EQUAL(cl.getString("opt", "dflt"), "dflt");

    //an optional value is only taken with '='
    const auto optOnce = parse({"--opt", "next"});
    POTHOS_TEST_TRUE(optOnce.given() == Given({{"opt", ""}}));
    POTHOS_TEST_TRUE(optOnce.positional() == std::vector<std::string>({"next"}));

    //numbers
    const auto num = parse({"--n=42"});
    POTHOS_TEST_EQUAL(num.getUInt("n", 1), 42u);
    POTHOS_TEST_EQUAL(num.getInt("n", 1), 42);
    POTHOS_TEST_EQUAL(num.getDouble("n", 1.0), 42.0);
    POTHOS_TEST_EQUAL(parse({}).getUInt("n", 7), 7u);
    POTHOS_TEST_EQUAL(parse({"--n=0.25"}).getDouble("n", 0), 0.25);
}

POTHOS_TEST_BLOCK("/app/tests", test_app_parse_errors)
{
    for (const auto &args : std::vector<std::vector<std::string>>{
        {"--unknown"}, {"-x"}, {"--flag=1"}, {"--a"}, {"--flag", "--flag"}, {"--a=1", "--a=2"}, {"--=x"}})
    {
        POTHOS_TEST_THROWS(parse(args), Pothos::InvalidArgumentException);
    }
    for (const std::string bad : {"x", "-1", "1.5", "", "99999999999999999999"})
    {
        POTHOS_TEST_THROWS(parse({"--n=" + bad}).getUInt("n", 0), Pothos::InvalidArgumentException);
    }
    POTHOS_TEST_THROWS(parse({"--n=1x"}).getDouble("n", 0), Pothos::InvalidArgumentException);
    POTHOS_TEST_THROWS(parse({"--n=x"}).getInt("n", 0), Pothos::InvalidArgumentException);

    try
    {
        parse({"--unknown-thing"});
        POTHOS_TEST_TRUE(false);
    }
    catch (const Pothos::InvalidArgumentException &ex)
    {
        POTHOS_TEST_TRUE(ex.displayText().find("--unknown-thing") != std::string::npos);
    }

    const auto vars = parse({"--var=a:1", "--var", "b:2"});
    POTHOS_TEST_TRUE(vars.getAll("var") == std::vector<std::string>({"a:1", "b:2"}));
    POTHOS_TEST_EQUAL(vars.getString("var"), "b:2");
}

POTHOS_TEST_BLOCK("/app/tests", test_app_core_command_lines)
{
    const auto check = [](const std::vector<std::string> &args, const Given &expected, const std::vector<std::string> &positional = {})
    {
        const auto cl = parse(args, coreSpecs());
        return cl.given() == expected and cl.positional() == positional;
    };
    POTHOS_TEST_TRUE(check({"--require-active", "--proxy-server=tcp://[::]:0"}, {{"require-active", ""}, {"proxy-server", "tcp://[::]:0"}}));
    POTHOS_TEST_TRUE(check({"--load-module", "/p/m.so", "--success-code", "200"}, {{"load-module", "/p/m.so"}, {"success-code", "200"}}));
    POTHOS_TEST_TRUE(check({"--self-tests"}, {{"self-tests", ""}}));
    POTHOS_TEST_TRUE(check({"--self-tests=/util"}, {{"self-tests", "/util"}}));
    POTHOS_TEST_TRUE(check({"--self-test1=/util/tests/x"}, {{"self-test1", "/util/tests/x"}}));
    POTHOS_TEST_TRUE(check({"--doc-parse", "a.cpp", "b.cpp", "--output", "out.json"}, {{"doc-parse", ""}, {"output", "out.json"}}, {"a.cpp", "b.cpp"}));
    POTHOS_TEST_TRUE(check({"--generate-simd-dispatchers=in.json", "--simd-arches=sse2,avx2", "--output=h.hpp"},
        {{"generate-simd-dispatchers", "in.json"}, {"simd-arches", "sse2,avx2"}, {"output", "h.hpp"}}));
}

POTHOS_TEST_BLOCK("/app/tests", test_app_help)
{
    const auto help = formatHelp(testSpecs(), "PothosUtil OPTIONS", "A header.");
    POTHOS_TEST_TRUE(help.find("usage: PothosUtil OPTIONS") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("A header.") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("-h, --help") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("--a=A") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("--opt[=O]") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("display help") != std::string::npos);
    POTHOS_TEST_TRUE(help.find("secret") == std::string::npos);
}
