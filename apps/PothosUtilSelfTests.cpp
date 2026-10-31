// Copyright (c) 2013-2020 Josh Blum
//                    2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include <Pothos/Testing.hpp>
#include <Pothos/Plugin/Loader.hpp>
#include <Pothos/System/Paths.hpp>
#include "Platform/Process.hpp"
#include "Util/StringUtils.hpp"
#include "Util/FileSystem.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <future>
#include <cctype>
#include <algorithm> //max

struct SelfTestResults
{
    std::vector<std::string> testsPassed;
    std::vector<std::string> testsFailed;
};

static std::string collectVerbose(Pothos::Platform::Pipe pipe)
{
    size_t maxLen = 0;
    Pothos::Platform::PipeInputStream is(pipe);
    std::vector<std::string> lines;
    try
    {
        std::string line;
        while (is.good())
        {
            std::getline(is, line);
            while (not line.empty() and std::isspace(line.back()))
            {
                line.pop_back();
            }
            if (line.empty()) continue;
            maxLen = std::max(maxLen, line.length());
            lines.push_back(line);
        }
    }
    catch (...){}
    std::ostringstream ss;
    ss << " +-" << std::string(maxLen, '-') << "-+" << std::endl;
    for (const auto &line : lines)
    {
        const size_t padLen = maxLen-line.length();
        ss << " | " << line << std::string(padLen, ' ') << " |" << std::endl;
    }
    ss << " +-" << std::string(maxLen, '-') << "-+" << std::endl;
    return ss.str();
}

static bool spawnSelfTestOneProcess(const std::string &path, size_t numTrials)
{
    const bool multipleTrials = (numTrials > 1);
    const int success = 200;

    std::cout << "Testing " << path << "... ";
    if(multipleTrials) std::cout << std::endl;
    else std::cout << std::flush;

    //create args
    std::vector<std::string> args;
    args.push_back("--self-test1");
    args.push_back(path);
    args.push_back("--success-code");
    args.push_back(std::to_string(success));
    args.push_back("--num-trials");
    args.push_back(std::to_string(numTrials));

    size_t numOk = 0;

    for(size_t trialNum = 0; trialNum < numTrials; ++trialNum)
    {
        //launch
        Pothos::Platform::Pipe outPipe; //no fwd stdio
        Pothos::Platform::ProcessOptions opts;
        opts.args = args;
        opts.out = &outPipe;
        opts.err = &outPipe;
        auto ph = Pothos::Platform::Process::launch(Pothos::Util::Detail::utf8Path(Pothos::System::getPothosUtilExecutablePath()), opts);

        std::future<std::string> verboseFuture(std::async(std::launch::async, &collectVerbose, outPipe));
        const bool ok = (ph.wait() == success);
        if(multipleTrials)
        {
            std::cout << " * Trial " << (trialNum+1) << ": " << std::flush;
        }
        std::cout << ((ok)? "success!" : "FAIL!") << std::endl;

        //the reader sees end of stream once the child has exited
        verboseFuture.wait();
        outPipe.close();
        if (not ok) std::cout << verboseFuture.get();
        else numOk++;
    }

    return numOk != 0;
}

static void runPluginSelfTestsR(const Pothos::PluginPath &path, SelfTestResults &results, const std::string &glob, size_t numTrials)
{
    //run the test found at path
    if (not Pothos::PluginRegistry::empty(path) and Pothos::Util::Detail::globMatch(glob, path.toString()))
    {
        auto plugin = Pothos::PluginRegistry::get(path);
        if (plugin.getObject().type() == typeid(std::shared_ptr<Pothos::TestingBase>))
        {
            if (spawnSelfTestOneProcess(path.toString(), numTrials))
            {
                results.testsPassed.push_back(path.toString());
            }
            else
            {
                results.testsFailed.push_back(path.toString());
            }
        }
    }
    //iterate on the subtree stuff
    auto nodes = Pothos::PluginRegistry::list(path);
    for (auto it = nodes.begin(); it != nodes.end(); it++)
    {
        runPluginSelfTestsR(path.join(*it), results, glob, numTrials);
    }
}

void PothosUtilBase::selfTestOne(const std::string &, const std::string &path)
{
    Pothos::ScopedInit init;

    const auto numTrials = this->config().getUInt("num-trials", 1);
    const bool multipleTrials = (numTrials > 1);

    auto plugin = Pothos::PluginRegistry::get(path);
    auto test = plugin.getObject().extract<std::shared_ptr<Pothos::TestingBase>>();

    if(!multipleTrials)
    {
        std::cout << "Testing " << plugin.getPath().toString() << "..." << std::endl;
    }

    try
    {
        for(size_t trialNum = 0; trialNum < numTrials; ++trialNum)
        {
            if(multipleTrials)
            {
                std::cout << "--------------------------------------------" << std::endl
                          <<  "Testing " << plugin.getPath().toString() << " (trial "
                          << (trialNum+1) << ")..." << std::endl << std::endl;
            }

            test->runTests();
            std::cout << "success!" << std::endl;
        }
    }
    catch(...)
    {
        throw;
    }
}

void PothosUtilBase::selfTests(const std::string &, const std::string &path)
{
    Pothos::ScopedInit init;

    const auto numTrials = this->config().getUInt("num-trials", 1);

    SelfTestResults results;
    if (path.find('*') == std::string::npos)
    {
        const std::string glob("*"); //not globing, match all
        runPluginSelfTestsR(path.empty()? "/" : path, results, glob, numTrials);
    }
    else
    {
        const std::string &glob(path); //path is a glob rule
        runPluginSelfTestsR("/", results, glob, numTrials);
    }
    std::cout << std::endl;

    const size_t totalTests = results.testsPassed.size() + results.testsFailed.size();
    if (results.testsFailed.empty())
    {
        std::cout << "All " << totalTests << " tests passed!" << std::endl;
    }
    else
    {
        std::cout << "Failed " << results.testsFailed.size() << " out of " << totalTests << " tests" << std::endl;
        for (auto it = results.testsFailed.begin(); it != results.testsFailed.end(); it++)
        {
            std::cout << "  FAIL: " << *it << std::endl;
        }
        std::cout << std::endl;
        throw Pothos::Exception("Failures occurred in self test suite.");
    }
    std::cout << std::endl;
}
