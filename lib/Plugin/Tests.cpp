// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <Pothos/Testing.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <thread>

POTHOS_TEST_BLOCK("/plugin/tests", test_plugin_path)
{
    Pothos::PluginPath nullPath;
    POTHOS_TEST_TRUE(nullPath == Pothos::PluginPath());
    POTHOS_TEST_EQUAL(nullPath.toString(), "/");
    POTHOS_TEST_EQUAL(nullPath.listNodes().size(), 0);

    Pothos::PluginPath goodPath("/foo-bar/my_module");
    POTHOS_TEST_EQUAL(goodPath.listNodes().size(), 2);
    POTHOS_TEST_EQUAL(goodPath.listNodes()[0], "foo-bar");
    POTHOS_TEST_EQUAL(goodPath.listNodes()[1], "my_module");

    POTHOS_TEST_THROWS(Pothos::PluginPath("/foo-bar//my_module"), Pothos::PluginPathError);
    POTHOS_TEST_THROWS(Pothos::PluginPath("/foo-bar/my_module "), Pothos::PluginPathError);
    POTHOS_TEST_THROWS(Pothos::PluginPath("foo-bar/my_module"), Pothos::PluginPathError);
    POTHOS_TEST_THROWS(Pothos::PluginPath("/foo bar/my_module"), Pothos::PluginPathError);
}

POTHOS_TEST_BLOCK("/plugin/tests", test_plugin_registry)
{
    Pothos::PluginRegistry::add(Pothos::Plugin("/tests/t0"));
    Pothos::PluginRegistry::add(Pothos::Plugin("/tests/t1"));
    POTHOS_TEST_TRUE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/t0")));
    POTHOS_TEST_TRUE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/t1")));
    POTHOS_TEST_FALSE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/t2")));

    std::vector<std::string> expectedList, testsList;
    expectedList.push_back("t0");
    expectedList.push_back("t1");
    testsList = Pothos::PluginRegistry::list(Pothos::PluginPath("/tests"));
    POTHOS_TEST_EQUALV(testsList, expectedList);

    Pothos::PluginRegistry::remove(Pothos::PluginPath("/tests/t1"));
    POTHOS_TEST_FALSE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/t1")));
    expectedList.erase(std::find(expectedList.begin(), expectedList.end(), "t1"));
    testsList = Pothos::PluginRegistry::list(Pothos::PluginPath("/tests"));
    POTHOS_TEST_EQUALV(testsList, expectedList);

    Pothos::PluginRegistry::add(Pothos::Plugin("/tests/foo/t2"));
    POTHOS_TEST_TRUE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/foo")));
    POTHOS_TEST_TRUE(Pothos::PluginRegistry::exists(Pothos::PluginPath("/tests/foo/t2")));
    expectedList.push_back("foo");
    testsList = Pothos::PluginRegistry::list(Pothos::PluginPath("/tests"));
    POTHOS_TEST_EQUALV(testsList, expectedList);

    POTHOS_TEST_THROWS(Pothos::PluginRegistry::add(Pothos::Plugin("/tests/t0")), Pothos::PluginRegistryError);
    POTHOS_TEST_THROWS(Pothos::PluginRegistry::get(Pothos::PluginPath("/tests")), Pothos::PluginRegistryError);
    POTHOS_TEST_THROWS(Pothos::PluginRegistry::remove(Pothos::PluginPath("/tests/foo")), Pothos::PluginRegistryError);
}

//the paths of a dump tree, depth first
static void dumpPaths(const Pothos::PluginRegistryInfoDump &dump, std::vector<std::string> &paths)
{
    paths.push_back(dump.pluginPath);
    for (const auto &sub : dump.subInfo) dumpPaths(sub, paths);
}

POTHOS_TEST_BLOCK("/plugin/tests", test_plugin_registry_missing_paths)
{
    //reads of a missing path give the old results and do not change the tree
    std::vector<std::string> before, after;
    dumpPaths(Pothos::PluginRegistry::dump(), before);

    const Pothos::PluginPath missing("/tests/missing/a/b");
    POTHOS_TEST_THROWS(Pothos::PluginRegistry::get(missing), Pothos::PluginRegistryError);
    POTHOS_TEST_TRUE(Pothos::PluginRegistry::empty(missing));
    POTHOS_TEST_FALSE(Pothos::PluginRegistry::exists(missing));
    POTHOS_TEST_EQUAL(Pothos::PluginRegistry::list(missing).size(), 0u);

    dumpPaths(Pothos::PluginRegistry::dump(), after);
    POTHOS_TEST_EQUALV(before, after);
}

POTHOS_TEST_BLOCK("/plugin/tests", test_plugin_registry_concurrent_reads)
{
    //readers of missing paths while a writer adds and removes a plugin
    //(TSan reports the race when the readers change the tree)
    std::atomic<bool> stop(false);
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; t++) readers.emplace_back([t, &stop]
    {
        for (int i = 0; i < 2000 and not stop; i++)
        {
            const Pothos::PluginPath path("/tests/race/r" + std::to_string(t) + "/n" + std::to_string(i));
            Pothos::PluginRegistry::exists(path);
            Pothos::PluginRegistry::list(path);
            try {Pothos::PluginRegistry::get(path);}
            catch (const Pothos::PluginRegistryError &) {}
        }
    });
    const Pothos::PluginPath pluginPath("/tests/race/plugin");
    for (int i = 0; i < 500; i++)
    {
        Pothos::PluginRegistry::add(Pothos::Plugin(pluginPath));
        Pothos::PluginRegistry::remove(pluginPath);
    }
    stop = true;
    for (auto &reader : readers) reader.join();
    POTHOS_TEST_FALSE(Pothos::PluginRegistry::exists(pluginPath));
}
