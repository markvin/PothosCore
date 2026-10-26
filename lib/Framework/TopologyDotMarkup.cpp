// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "Framework/TopologyImpl.hpp"
#include <charconv>
#include <sstream>
#include <string_view>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//! Escape text and attribute values for a graphviz HTML-like label
static std::string htmlEscape(std::string_view s)
{
    std::string out;
    for (const char ch : s)
    {
        switch (ch)
        {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += ch;
        }
    }
    return out;
}

static std::string portInfoToElem(const json &portsInfo, const std::string &prefix)
{
    std::string out = "<td border=\"0\"><table border=\"0\" cellspacing=\"0\">";
    for (const auto &portInfo : portsInfo)
    {
        std::string name = portInfo["name"];
        const bool isSigSlot = portInfo["isSigSlot"];

        std::string bgcolor;
        if (prefix == "in"  and isSigSlot)     bgcolor = "#AEC6CF";
        if (prefix == "in"  and not isSigSlot) bgcolor = "#779ECB";
        if (prefix == "out" and isSigSlot)     bgcolor = "#77DD77";
        if (prefix == "out" and not isSigSlot) bgcolor = "#03C03C";
        const auto port = "__"+prefix+"__"+name;
        unsigned value = 0;
        const auto parsed = std::from_chars(name.data(), name.data()+name.size(), value);
        if (not name.empty() and parsed.ec == std::errc() and parsed.ptr == name.data()+name.size()) name = prefix+name;

        out += "<tr><td bgcolor=\"" + bgcolor + "\" border=\"1\" port=\"" + htmlEscape(port) + "\">";
        out += htmlEscape(name) + "</td></tr>";
    }
    out += "</table></td>";
    return out;
}

static json getConnectedPortInfos(
    const json &topObj,
    const std::string &blockId,
    const bool enbFilter,
    const bool isInput)
{
    const auto connsArray = topObj["connections"];
    const auto blocksObj = topObj["blocks"];
    const auto blockObj = blocksObj[blockId];

    //grab the raw ports info
    auto portsInfo = json::array();
    if (isInput and blockObj.count("inputs")) portsInfo = blockObj["inputs"];
    if (not isInput and blockObj.count("outputs")) portsInfo = blockObj["outputs"];

    //no filtering? return ASAP
    if (not enbFilter) return portsInfo;

    json filteredPortsInfo;
    for (const auto &portInfo : portsInfo)
    {
        for (const auto &conn : connsArray)
        {
            if (
                (not isInput and blockId == conn["srcId"].get<std::string>() and portInfo["name"].get<std::string>() == conn["srcName"].get<std::string>()) or
                (isInput and blockId == conn["dstId"].get<std::string>() and portInfo["name"].get<std::string>() == conn["dstName"].get<std::string>())
            )
            {
                filteredPortsInfo.push_back(portInfo);
                break;
            }
        }
    }
    return filteredPortsInfo;
}

std::string Pothos::Topology::toDotMarkup(const std::string &request)
{
    //parse request arguments
    const auto configObj = json::parse(request.empty()?"{}":request);
    const auto portConfig = configObj.value<std::string>("port", "connected");

    //get a JSON dump of the topology
    const auto topObj = json::parse(this->dumpJSON(request));
    const auto connsArray = topObj["connections"];
    const auto blocksObj = topObj["blocks"];

    std::ostringstream os;
    os << "digraph flat_flows {" << std::endl;
    os << "    rankdir=LR;" << std::endl;
    os << "    node [shape=record, fontsize=10];" << std::endl;

    for (auto it = blocksObj.begin(); it != blocksObj.end(); ++it)
    {
        const auto blockId = it.key();
        const auto blockObj = it.value();

        //form the html-like label
        std::string label = "<table border=\"0\" cellpadding=\"0\" cellspacing=\"0\"><tr>";

        const bool enbFilter = portConfig == "connected";
        const auto inputPorts = getConnectedPortInfos(topObj, blockId, enbFilter, true);
        const auto outputPorts = getConnectedPortInfos(topObj, blockId, enbFilter, false);

        if (not inputPorts.empty()) label += portInfoToElem(inputPorts, "in");
        {
            std::string name = blockObj["name"];
            if (name.empty()) name = "Empty Name";
            label += "<td border=\"0\"><table border=\"0\" cellspacing=\"0\"><tr>";
            label += "<td bgcolor=\"azure\" border=\"1\">" + htmlEscape(name) + "</td>";
            label += "</tr></table></td>";
        }
        if (not outputPorts.empty()) label += portInfoToElem(outputPorts, "out");
        label += "</tr></table>";

        //dot node entry
        os << "    ";
        os << std::hash<std::string>()(blockId);
        os << "[" << std::endl;
        os << "    shape=none," << std::endl;
        os << "    label=<" << std::endl;
        os << label << std::endl;
        os << "    >" << std::endl;
        os << "];" << std::endl;
    }

    for (const auto &conn : connsArray)
    {
        os << "    ";
        os << std::hash<std::string>()(conn["srcId"].get<std::string>());
        os << ":__out__" << conn["srcName"].get<std::string>();
        os << " -> ";
        os << std::hash<std::string>()(conn["dstId"].get<std::string>());
        os << ":__in__" << conn["dstName"].get<std::string>();
        os << ";" << std::endl;
    }

    os << "}" << std::endl;
    return os.str();
}
