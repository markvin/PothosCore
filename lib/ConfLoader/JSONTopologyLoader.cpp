// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include <Pothos/Framework.hpp>
#include "Util/FileSystem.hpp"
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include "Util/FileSystem.hpp" //utf8Path, pathUtf8

using json = nlohmann::json;

/***********************************************************************
 * The topology factory loads args into a JSON description
 * and creates and returns an instance of the topology
 **********************************************************************/
static Pothos::Object opaqueJSONTopologyFactory(
    const std::string &jsonStr,
    const Pothos::Object *args,
    const size_t numArgs)
{
    //parse the json formatted string into a JSON object
    json topObj;
    try
    {
        topObj = json::parse(jsonStr);
    }
    catch (const std::exception &ex)
    {
        throw Pothos::DataFormatException(ex.what());
    }

    //apply the global variable overlays
    auto &globalsArray = topObj["globals"];
    if (numArgs > globalsArray.size())
    {
        throw Pothos::InvalidArgumentException("too many args passed to factory");
    }

    for (size_t i = 0; i < numArgs; i++)
    {
        if (not globalsArray[i].is_object())
        {
            throw Pothos::DataFormatException("global entry not a JSON object");
        }
        globalsArray[i]["value"] = args[i].toString();
    }

    //create the topology from the JSON string
    return Pothos::Object(Pothos::Topology::make(topObj.dump()));
}

/***********************************************************************
 * Load a JSON topology described by a config file section
 **********************************************************************/
static std::vector<Pothos::PluginPath> JSONTopologyLoader(const std::map<std::string, std::string> &config)
{
    std::vector<Pothos::PluginPath> entries;

    //config file path set by caller
    const auto confFilePathIt = config.find("confFilePath");
    if (confFilePathIt == config.end() or confFilePathIt->second.empty())
        throw Pothos::Exception("missing confFilePath");

    //determine JSON description file path
    const auto jsonIt = config.find("json");
    if (jsonIt == config.end() or jsonIt->second.empty())
        throw Pothos::Exception("JSON file not specified");
    const auto jsonPath = Pothos::Util::Detail::makeAbsolute(
        Pothos::Util::Detail::expandPath(jsonIt->second),
        Pothos::Util::Detail::utf8Path(confFilePathIt->second).parent_path());
    if (not std::filesystem::exists(jsonPath))
        throw Pothos::Exception(Pothos::Util::Detail::pathUtf8(jsonPath) + " does not exist");

    //get the plugin path
    const auto pathIt = config.find("path");
    if (pathIt == config.end() or pathIt->second.empty())
        throw Pothos::Exception("missing plugin path");
    const auto pluginPath = Pothos::PluginPath("/blocks", pathIt->second);

    //parse the file into a string
    std::ifstream ifs(jsonPath);
    const std::string json((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    //create an entry for the factory
    const auto factory = Pothos::Callable(&opaqueJSONTopologyFactory).bind(json, 0);
    Pothos::PluginRegistry::addCall(pluginPath, factory);

    entries.push_back(pluginPath);
    return entries;
}

/***********************************************************************
 * loader registration
 **********************************************************************/
pothos_static_block(pothosFrameworkRegisterJSONTopologyLoader)
{
    Pothos::PluginRegistry::addCall("/framework/conf_loader/json_topology", &JSONTopologyLoader);
}
