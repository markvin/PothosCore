// Copyright (c) 2020 Nicholas Corgan
// SPDX-License-Identifier: BSL-1.0

#include "PothosUtil.hpp"
#include "Util/FileSystem.hpp"

#include <Pothos/Plugin.hpp>
#include <Pothos/Proxy.hpp>

#include <filesystem>
#include "Util/StringUtils.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

/***********************************************************************
 * Generate a C++ header with SIMD dispatchers for a given Pothos module
 **********************************************************************/
void PothosUtilBase::generateSIMDDispatchers(const std::string&, const std::string& jsonPath)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto simdDispatcher = env->findProxy("Pothos/Util/SIMDDispatcher");

    const auto simdArchesStr = this->config().getString("simd-arches");
    const auto outputFilePath = this->config().getString("output");

    const auto simdArchesTokenizer = Pothos::Util::Detail::split(simdArchesStr, ",", true, false);
    std::vector<std::string> simdArches(simdArchesTokenizer.begin(), simdArchesTokenizer.end());

    const auto fileSize = std::filesystem::file_size(Pothos::Util::Detail::utf8Path(jsonPath));

    std::ifstream ifile(Pothos::Util::Detail::utf8Path(jsonPath), std::ios::in);
    std::string jsonInput;
    jsonInput.resize(fileSize);
    ifile.read((char*)jsonInput.data(), fileSize);

    const auto outputHeaderString = simdDispatcher.call<std::string>("generateAndDumpHeader", jsonInput, simdArches);

    std::ofstream ofile(Pothos::Util::Detail::utf8Path(outputFilePath), std::ios::out);
    ofile << outputHeaderString;
}