// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <algorithm>
#include <cstring>
#include <memory>
#include <numeric>
#include <vector>

/***********************************************************************
 * Blocks whose ports reject each other's memory domain (but not the
 * generic domain of the copier): the topology connects them through /blocks/copier
 **********************************************************************/
namespace
{
    struct CopierTestSource : Pothos::Block
    {
        CopierTestSource(const std::vector<int> &data):
            _data(data)
        {
            this->setupOutput(0, typeid(int), "copier_test_a");
        }

        std::shared_ptr<Pothos::BufferManager> getOutputBufferManager(const std::string &, const std::string &domain)
        {
            if (domain == "copier_test_b") throw Pothos::PortDomainError(domain);
            return {};
        }

        void work(void)
        {
            auto output = this->output(0);
            const size_t n = std::min(output->elements(), _data.size() - _sent);
            if (n == 0) return;
            std::memcpy(output->buffer().as<int *>(), _data.data() + _sent, n*sizeof(int));
            output->produce(n);
            _sent += n;
        }

        const std::vector<int> _data;
        size_t _sent = 0;
    };

    struct CopierTestSink : Pothos::Block
    {
        CopierTestSink(void)
        {
            this->setupInput(0, typeid(int), "copier_test_b");
        }

        std::shared_ptr<Pothos::BufferManager> getInputBufferManager(const std::string &, const std::string &domain)
        {
            if (domain == "copier_test_a") throw Pothos::PortDomainError(domain);
            return {};
        }

        void work(void)
        {
            auto input = this->input(0);
            const auto in = input->buffer().as<const int *>();
            _received.insert(_received.end(), in, in + input->elements());
            input->consume(input->elements());
        }

        std::vector<int> _received;
    };
}

POTHOS_TEST_BLOCK("/blocks/tests", test_copier_domain_crossing)
{
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 0);

    auto source = std::make_shared<CopierTestSource>(data);
    auto sink = std::make_shared<CopierTestSink>();
    {
        Pothos::Topology topology;
        topology.connect(source, 0, sink, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }
    POTHOS_TEST_EQUAL(sink->_received.size(), data.size());
    POTHOS_TEST_EQUALV(sink->_received, data);
}
