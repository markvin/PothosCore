// Copyright (c) 2016-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin.hpp>
#include "Util/FileSystem.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cassert>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/***********************************************************************
 * Load a JSON block description from file and register the descriptions.
 * return a list of registration paths and a list of paths for blocks.
 **********************************************************************/
static std::vector<Pothos::PluginPath> blockDescParser(std::istream &is, std::vector<Pothos::PluginPath> &blockPaths)
{
    std::vector<Pothos::PluginPath> entries;

    //parse the stream into a JSON array
    auto arrayOut = json::parse(is);
    if (arrayOut.is_object()) arrayOut = {arrayOut};
    for (const auto &obj : arrayOut)
    {
        const std::string JsonObjStr(obj.dump());

        std::vector<std::string> paths;
        paths.push_back(obj["path"]);
        for (const auto &alias : obj.value("aliases", json::array()))
        {
            paths.push_back(alias);
        }

        //register the block description for every path
        for (const auto &path : paths)
        {
            const auto pluginPath = Pothos::PluginPath("/blocks/docs", path);
            Pothos::PluginRegistry::add(pluginPath, JsonObjStr);
            entries.push_back(pluginPath);
            blockPaths.push_back(Pothos::PluginPath("/blocks", path));
        }
    }
    return entries;
}

/***********************************************************************
 * Load a JSON block description described by a config file section
 **********************************************************************/
static std::vector<Pothos::PluginPath> blockDescLoader(const std::map<std::string, std::string> &config)
{
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

    //open an input file stream
    std::ifstream ifs(jsonPath);

    std::vector<Pothos::PluginPath> blockPaths;
    return blockDescParser(ifs, blockPaths);
}

/***********************************************************************
 * loader registration
 **********************************************************************/
pothos_static_block(pothosFrameworkRegisterBlockDescLoader)
{
    Pothos::PluginRegistry::addCall("/framework/conf_loader/block_desc", &blockDescLoader);
}
