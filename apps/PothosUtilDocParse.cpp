// Copyright (c) 2014-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include "Util/FileSystem.hpp"
#include <Pothos/Util/BlockDescription.hpp>
#include <Pothos/Plugin.hpp>
#include <format>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

/***********************************************************************
 * generate a cpp source that adds the json string to the registry
 **********************************************************************/
static void jsonArrayToCppStaticBlock(const Pothos::Util::BlockDescriptionParser &parser, std::ostream &os, const std::string &target)
{
    os << "#include <Pothos/Framework.hpp>\n";
    os << "#include <Pothos/Plugin.hpp>\n";
    os << std::format("pothos_static_block(registerPothosBlockDocs{})\n", target);
    os << "{\n";
    for (const auto &factory : parser.listFactories())
    {
        //create escaped string of json
        std::string escaped;
        for (const auto &ch : parser.getJSONObject(factory))
        {
            escaped += std::format("\\x{:02X}", int(ch));
        }

        //register the block description at the specified path
        const auto pluginPath = Pothos::PluginPath("/blocks/docs", factory);
        os << std::format("    Pothos::PluginRegistry::add(\"{}\", std::string(\"{}\"));\n", pluginPath.toString(), escaped);
    }
    os << "}\n";
}

void PothosUtilBase::docParse(const std::vector<std::string> &inputFilePaths)
{
    Pothos::Util::BlockDescriptionParser parser;

    //feed all input files into the parser
    for (const auto &inputFilePath : inputFilePaths)
    {
        parser.feedFilePath(inputFilePath);
    }

    //write to output (file if specified, otherwise stdout)
    const auto outputFilePath = this->config().getString("output", "");
    const size_t indentSpaces = 4;
    if (outputFilePath.empty())
    {
        std::cout << std::endl;
        std::cout << parser.getJSONArray(indentSpaces);
        std::cout << std::endl;
    }
    else
    {
        const auto outputFileName = Pothos::Util::Detail::pathUtf8(Pothos::Util::Detail::utf8Path(outputFilePath).stem());
        auto outputFileExt = Pothos::Util::Detail::pathUtf8(Pothos::Util::Detail::utf8Path(outputFilePath).extension());
        if (not outputFileExt.empty()) outputFileExt.erase(0, 1); //no dot, as before

        std::ofstream outputFile(Pothos::Util::Detail::utf8Path(outputFilePath));
        if (outputFileExt == "json") outputFile << parser.getJSONArray(indentSpaces);
        else if (outputFileExt == "cpp") jsonArrayToCppStaticBlock(parser, outputFile, outputFileName);
        else throw Pothos::Exception("PothosUtilBase::docParse()", "unsupported file extension: " + outputFilePath);
        outputFile << std::endl;
        outputFile.close();
    }
}
