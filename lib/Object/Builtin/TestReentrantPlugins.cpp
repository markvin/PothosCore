// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Object.hpp>
#include <Pothos/Plugin.hpp>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>

/***********************************************************************
 * Plugin callables that touch the plugin registry while they run:
 * the registries must not be locked while a plugin is called,
 * otherwise registering from inside the call deadlocks
 * (and a waiting writer deadlocks re-entrant readers on some platforms)
 **********************************************************************/
namespace
{
    struct Reentrant {int value;};
    struct ReentrantExtra {int value;};

    std::string extraToString(const ReentrantExtra &) {return "extra";}
    size_t extraHash(const ReentrantExtra &) {return 0;}
    int extraCompare(const ReentrantExtra &, const ReentrantExtra &) {return 0;}
    int extraConvert(const ReentrantExtra &x) {return x.value;}

    template <typename Fcn>
    void touchRegistry(const std::string &path, Fcn fcn)
    {
        Pothos::PluginRegistry::addCall(path, fcn);
        Pothos::PluginRegistry::remove(path);
    }

    std::string reentrantToString(const Reentrant &x)
    {
        touchRegistry("/object/tostring/test/reentrant_extra", &extraToString);
        return "reentrant" + std::to_string(x.value);
    }

    size_t reentrantHash(const Reentrant &x)
    {
        touchRegistry("/object/hash/test/reentrant_extra", &extraHash);
        return size_t(x.value);
    }

    int reentrantCompare(const Reentrant &a, const Reentrant &b)
    {
        touchRegistry("/object/compare/test/reentrant_extra", &extraCompare);
        return (a.value < b.value)? -1 : ((a.value > b.value)? 1 : 0);
    }

    int reentrantConvert(const Reentrant &x)
    {
        touchRegistry("/object/convert/test/reentrant_extra", &extraConvert);
        return x.value;
    }

    //run in a thread: a deadlocked call cannot hang the test runner
    bool finishesWithin(const std::function<void(void)> &fcn)
    {
        auto done = std::make_shared<std::promise<bool>>();
        auto result = done->get_future();
        std::thread([fcn, done]
        {
            try {fcn(); done->set_value(true);}
            catch (...) {done->set_value(false);}
        }).detach();
        if (result.wait_for(std::chrono::seconds(5)) != std::future_status::ready) return false;
        return result.get();
    }
}

POTHOS_TEST_BLOCK("/object/tests", test_reentrant_plugins)
{
    Pothos::PluginRegistry::addCall("/object/tostring/test/reentrant", &reentrantToString);
    Pothos::PluginRegistry::addCall("/object/hash/test/reentrant", &reentrantHash);
    Pothos::PluginRegistry::addCall("/object/compare/test/reentrant", &reentrantCompare);
    Pothos::PluginRegistry::addCall("/object/convert/test/reentrant", &reentrantConvert);

    const Pothos::Object a(Reentrant{1}), b(Reentrant{2});
    std::string str;
    size_t hash = 0;
    int cmp = 0, num = 0;
    POTHOS_TEST_TRUE(finishesWithin([&]{str = a.toString();}));
    POTHOS_TEST_EQUAL(str, "reentrant1");
    POTHOS_TEST_TRUE(finishesWithin([&]{hash = b.hashCode();}));
    POTHOS_TEST_EQUAL(hash, 2u);
    POTHOS_TEST_TRUE(finishesWithin([&]{cmp = a.compareTo(b);}));
    POTHOS_TEST_EQUAL(cmp, -1);
    POTHOS_TEST_TRUE(finishesWithin([&]{num = b.convert<int>();}));
    POTHOS_TEST_EQUAL(num, 2);

    Pothos::PluginRegistry::remove("/object/tostring/test/reentrant");
    Pothos::PluginRegistry::remove("/object/hash/test/reentrant");
    Pothos::PluginRegistry::remove("/object/compare/test/reentrant");
    Pothos::PluginRegistry::remove("/object/convert/test/reentrant");
}
