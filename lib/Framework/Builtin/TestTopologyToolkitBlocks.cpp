// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Plugin.hpp>
#include <iostream>
#include <string>

/***********************************************************************
 * Blocks whose ports only work within their own memory domain:
 * a connection between them needs a copier block from the blocks toolkit
 **********************************************************************/
namespace
{
    struct DomainSource : Pothos::Block
    {
        DomainSource(void)
        {
            this->setupOutput(0, typeid(int), "test_domain_a");
        }

        std::shared_ptr<Pothos::BufferManager> getOutputBufferManager(const std::string &, const std::string &domain)
        {
            if (domain == "test_domain_a") return {};
            throw Pothos::PortDomainError(domain);
        }

        void work(void) {}
    };

    struct DomainSink : Pothos::Block
    {
        DomainSink(void)
        {
            this->setupInput(0, typeid(int), "test_domain_b");
        }

        std::shared_ptr<Pothos::BufferManager> getInputBufferManager(const std::string &, const std::string &domain)
        {
            if (domain == "test_domain_b") return {};
            throw Pothos::PortDomainError(domain);
        }

        void work(void) {}
    };
}

POTHOS_TEST_BLOCK("/framework/tests/topology", test_domain_crossing_needs_blocks)
{
    //the copier of the blocks toolkit is not available (restored afterwards);
    //the block /blocks/copier lives at the plugin path /blocks/blocks/copier
    const auto copierPath = Pothos::PluginPath("/blocks/blocks/copier");
    Pothos::Plugin copier;
    if (Pothos::PluginRegistry::exists(copierPath)) copier = Pothos::PluginRegistry::remove(copierPath);

    auto source = std::make_shared<DomainSource>();
    auto sink = std::make_shared<DomainSink>();
    Pothos::Topology topology;
    topology.connect(source, 0, sink, 0);

    std::string error;
    try {topology.commit();}
    catch (const Pothos::TopologyConnectError &ex) {error = ex.displayText();}
    catch (const Pothos::Exception &ex) {error = "wrong exception: " + ex.displayText();}
    topology.disconnectAll();
    if (copier.getObject()) Pothos::PluginRegistry::add(copier);

    std::cout << "commit error: " << error << std::endl;
    POTHOS_TEST_TRUE(error.find("/blocks/copier") != std::string::npos);
    POTHOS_TEST_TRUE(error.find("PothosBlocks") != std::string::npos);
    POTHOS_TEST_TRUE(error.find("in this process") != std::string::npos);
    POTHOS_TEST_TRUE(error.find("wrong exception") == std::string::npos);
}
