// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <regex>
#include <string>

namespace {

struct DotSource : Pothos::Block
{
    DotSource(const std::string &name)
    {
        this->setupOutput("out0");
        this->setName(name);
    }
};

struct DotSink : Pothos::Block
{
    DotSink(const std::string &name)
    {
        this->setupInput("in0");
        this->setName(name);
    }
};

//node ids are hashes of random block ids and indentation is cosmetic
std::string normalize(const std::string &markup)
{
    const auto noIds = std::regex_replace(markup, std::regex("[0-9]{6,}"), "N");
    return std::regex_replace(noIds, std::regex("\\s+"), "");
}

std::string dotMarkup(const std::string &sourceName, const std::string &sinkName)
{
    auto src = std::make_shared<DotSource>(sourceName);
    auto dst = std::make_shared<DotSink>(sinkName);
    Pothos::Topology topology;
    topology.connect(src, "out0", dst, "in0");
    return topology.toDotMarkup();
}

}

//reference output captured from the original (Poco XML) implementation;
//node order follows random block ids, so each part is checked on its own
POTHOS_TEST_BLOCK("/framework/tests", test_dot_markup_reference)
{
    const auto markup = normalize(dotMarkup("Src", "Dst"));
    POTHOS_TEST_EQUAL(markup.find("digraphflat_flows{rankdir=LR;node[shape=record,fontsize=10];"), 0u);
    POTHOS_TEST_TRUE(markup.find(R"(N[shape=none,label=<<tableborder="0"cellpadding="0"cellspacing="0"><tr><tdborder="0"><tableborder="0"cellspacing="0"><tr><tdbgcolor="azure"border="1">Src</td></tr></table></td><tdborder="0"><tableborder="0"cellspacing="0"><tr><tdbgcolor="#03C03C"border="1"port="__out__out0">out0</td></tr></table></td></tr></table>>];)") != std::string::npos);
    POTHOS_TEST_TRUE(markup.find(R"(N[shape=none,label=<<tableborder="0"cellpadding="0"cellspacing="0"><tr><tdborder="0"><tableborder="0"cellspacing="0"><tr><tdbgcolor="#779ECB"border="1"port="__in__in0">in0</td></tr></table></td><tdborder="0"><tableborder="0"cellspacing="0"><tr><tdbgcolor="azure"border="1">Dst</td></tr></table></td></tr></table>>];)") != std::string::npos);
    POTHOS_TEST_TRUE(markup.find("N:__out__out0->N:__in__in0;}") != std::string::npos);
}

POTHOS_TEST_BLOCK("/framework/tests", test_dot_markup_escaping)
{
    const auto markup = dotMarkup("Src", "a<b>&\"c\"");
    POTHOS_TEST_TRUE(markup.find("a&lt;b&gt;&amp;&quot;c&quot;") != std::string::npos);
    POTHOS_TEST_TRUE(markup.find("a<b>") == std::string::npos);
    POTHOS_TEST_EQUAL(markup.find("digraph"), 0u);
}
