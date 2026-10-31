// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include <Pothos/Framework.hpp>
#include <Pothos/Exception.hpp>
#include "Util/FileSystem.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>

using json = nlohmann::json;

void PothosUtilBase::runTopology(void)
{
    Pothos::ScopedInit init;

    //sanity check the file
    const auto path = this->config().getString("run-topology");
    const auto extension = Pothos::Util::Detail::utf8Path(path).extension();
    if (extension == ".pothos" or extension == ".pth")
    {
        throw Pothos::DataFormatException("Cannot load "+path+"!\n"
            "Please export the design to the JSON topology format.");
    }
    std::ifstream ifs(Pothos::Util::Detail::utf8Path(Pothos::Util::Detail::expandPath(path)));
    if (not ifs) throw Pothos::FileException("Can't open "+path+" for reading!");

    //parse the json formatted string into a JSON object
    json topObj;
    try
    {
        topObj = json::parse(ifs);
    }
    catch (const std::exception &ex)
    {
        throw Pothos::DataFormatException(ex.what());
    }

    //apply the global variable overlays
    if (topObj.count("globals") == 0) topObj["globals"] = json::array();
    auto &globalsArray = topObj["globals"];
    for (const auto &pair : _vars)
    {
        //look for a match in the existing globals and override its value
        for (auto &globalVarObj : globalsArray)
        {
            if (globalVarObj.value<std::string>("name", "") == pair.first)
            {
                globalVarObj["value"] = pair.second;
                goto nextVar;
            }
        }

        //otherwise add to the end of the globals array
        {
            std::cout << "add name " << pair.first << std::endl;
            json globalVarObj;
            globalVarObj["name"] = pair.first;
            globalVarObj["value"] = pair.second;
            globalsArray.push_back(globalVarObj);
        }

        nextVar: continue;
    }

    //create the topology from the JSON string
    std::cout << ">>> Create Topology: " << path << std::endl;
    auto topology = Pothos::Topology::make(topObj.dump());

    //commit the topology and wait for specified time for CTRL+C
    if (this->config().has("idle-time"))
    {
        const auto idleTime = this->config().getDouble("idle-time", 0.0);
        double timeout = 0.0;
        std::string timeoutMsg = "no timeout";
        if (this->config().has("run-duration"))
        {
            timeout = this->config().getDouble("run-duration", 0.0);
            timeoutMsg = "timeout " + std::to_string(timeout) + " seconds";
        }
        std::cout << ">>> Running topology until idle with " << timeoutMsg << std::endl;
        topology->commit();
        if (not topology->waitInactive(idleTime, timeout))
        {
            throw Pothos::RuntimeException("Topology::waitInactive() reached timeout");
        }
    }
    else if (this->config().has("run-duration"))
    {
        const auto runDuration = this->config().getDouble("run-duration", 0.0);
        std::cout << ">>> Running topology for " << runDuration << " seconds" << std::endl;
        topology->commit();
        std::this_thread::sleep_for(std::chrono::milliseconds(long(runDuration*1000)));
    }
    else
    {
        std::cout << ">>> Running topology, press CTRL+C to exit" << std::endl;
        topology->commit();
        this->waitForTerminationRequest();
    }

    //dump the stats to file if specified
    if (this->config().has("output"))
    {
        const auto statsFile = this->config().getString("output");
        std::cout << ">>> Dumping stats: " << statsFile << std::endl;
        std::ofstream ofs(Pothos::Util::Detail::utf8Path(Pothos::Util::Detail::expandPath(statsFile)));
        ofs << topology->queryJSONStats() << std::endl;
    }
}
