// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "Util/PropertyFile.hpp"
#include "Util/StringUtils.hpp"
#include <Pothos/Exception.hpp>
#include <fstream>
#include <istream>
#include <sstream>

using namespace Pothos::Util::Detail;

namespace {

constexpr int Eof = std::char_traits<char>::eof();

//! One value character: escapes decoded, continuation lines joined, 0 at end of line
int readChar(std::istream &is)
{
    while (true)
    {
        int c = is.get();
        if (c == '\\')
        {
            c = is.get();
            switch (c)
            {
            case 't': return '\t';
            case 'r': return '\r';
            case 'n': return '\n';
            case 'f': return '\f';
            case '\r':
                if (is.peek() == '\n') is.get();
                continue;
            case '\n': continue;
            default: return c;
            }
        }
        if (c == '\n' or c == '\r') return 0;
        return c;
    }
}

} //namespace

PropertyMap Pothos::Util::Detail::parseProperties(std::istream &is)
{
    PropertyMap map;
    while (true)
    {
        int c = is.get();
        while (c != Eof and isSpace(char(c))) c = is.get();
        if (c == Eof) break;
        if (c == '#' or c == '!')
        {
            while (c != Eof and c != '\n' and c != '\r') c = is.get();
            continue;
        }
        std::string key;
        while (c != Eof and c != '=' and c != ':' and c != '\r' and c != '\n')
        {
            key += char(c);
            c = is.get();
        }
        std::string value;
        if (c == '=' or c == ':')
        {
            c = readChar(is);
            while (c != Eof and c != 0)
            {
                value += char(c);
                c = readChar(is);
            }
        }
        map[std::string(trim(key))] = std::string(trim(value));
    }
    return map;
}

std::string Pothos::Util::Detail::formatProperties(const PropertyMap &map)
{
    std::string out;
    for (const auto &[key, value] : map)
    {
        out += key + ": ";
        for (const char ch : value)
        {
            switch (ch)
            {
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\\': out += "\\\\"; break;
            default: out += ch;
            }
        }
        out += "\n";
    }
    return out;
}

PropertyMap Pothos::Util::Detail::loadPropertyFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (not file) throw Pothos::FileException("loadPropertyFile", "cannot open " + path.string());
    return parseProperties(file);
}

void Pothos::Util::Detail::savePropertyFile(const std::filesystem::path &path, const PropertyMap &map)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (not file) throw Pothos::FileException("savePropertyFile", "cannot open " + path.string());
    file << formatProperties(map);
    file.flush();
    if (not file) throw Pothos::FileException("savePropertyFile", "cannot write " + path.string());
}
