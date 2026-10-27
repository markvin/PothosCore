// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include "App/Options.hpp"
#include <Pothos/Exception.hpp>
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <locale>
#include <type_traits>
#include <sstream>

using namespace Pothos::App;

namespace {

[[noreturn]] void fail(const std::string &message)
{
    throw Pothos::InvalidArgumentException("command line", message);
}

const OptionSpec *findLong(const std::vector<OptionSpec> &specs, const std::string_view name)
{
    const auto it = std::ranges::find(specs, name, &OptionSpec::name);
    return (it == specs.end())? nullptr : &*it;
}

const OptionSpec *findShort(const std::vector<OptionSpec> &specs, const char name)
{
    const auto it = std::ranges::find(specs, name, &OptionSpec::shortName);
    return (name == 0 or it == specs.end())? nullptr : &*it;
}

template <typename T>
T parseNumber(const std::string_view name, const std::string &value)
{
    T out{};
    bool ok = false;
    if constexpr (std::is_floating_point_v<T>)
    {
        #if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
        const auto result = std::from_chars(value.data(), value.data()+value.size(), out);
        ok = result.ec == std::errc() and result.ptr == value.data()+value.size();
        #else
        //no floating point from_chars (Apple libc++): classic locale stream
        std::istringstream is(value);
        is.imbue(std::locale::classic());
        is >> out;
        ok = not is.fail() and is.peek() == std::char_traits<char>::eof();
        #endif
    }
    else
    {
        const auto result = std::from_chars(value.data(), value.data()+value.size(), out);
        ok = result.ec == std::errc() and result.ptr == value.data()+value.size();
    }
    if (value.empty() or not ok) fail("option '--" + std::string(name) + "' expects a number, got '" + value + "'");
    return out;
}

//! Split text into lines of at most width characters at spaces (and at \n)
std::vector<std::string> wrap(const std::string &text, const std::size_t width)
{
    std::vector<std::string> lines;
    std::istringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph))
    {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word)
        {
            if (not line.empty() and line.size() + 1 + word.size() > width)
            {
                lines.push_back(line);
                line.clear();
            }
            line += (line.empty()? "" : " ") + word;
        }
        lines.push_back(line);
    }
    return lines;
}

} //namespace

const std::vector<std::pair<std::string, std::string>> &CommandLine::given(void) const
{
    return _given;
}

const std::vector<std::string> &CommandLine::positional(void) const
{
    return _positional;
}

bool CommandLine::has(const std::string_view name) const
{
    return std::ranges::any_of(_given, [name](const auto &kv){return kv.first == name;});
}

bool CommandLine::hasOption(const std::string_view name) const
{
    return this->has(name);
}

std::string CommandLine::getString(const std::string_view name, std::string def) const
{
    for (auto it = _given.rbegin(); it != _given.rend(); ++it)
    {
        if (it->first == name) return it->second;
    }
    return def;
}

std::vector<std::string> CommandLine::getAll(const std::string_view name) const
{
    std::vector<std::string> out;
    for (const auto &[key, value] : _given) if (key == name) out.push_back(value);
    return out;
}

int CommandLine::getInt(const std::string_view name, const int def) const
{
    return this->has(name)? parseNumber<int>(name, this->getString(name)) : def;
}

unsigned CommandLine::getUInt(const std::string_view name, const unsigned def) const
{
    if (not this->has(name)) return def;
    const auto value = this->getString(name);
    if (value.starts_with('-')) fail("option '--" + std::string(name) + "' expects a positive number, got '" + value + "'");
    return parseNumber<unsigned>(name, value);
}

double CommandLine::getDouble(const std::string_view name, const double def) const
{
    return this->has(name)? parseNumber<double>(name, this->getString(name)) : def;
}

CommandLine Pothos::App::parseCommandLine(const std::vector<OptionSpec> &specs, const std::vector<std::string> &args)
{
    CommandLine cl;
    const auto add = [&cl](const OptionSpec &spec, std::string value)
    {
        if (not spec.repeatable and cl.has(spec.name)) fail("option '--" + spec.name + "' given more than once");
        cl._given.emplace_back(spec.name, std::move(value));
    };

    for (std::size_t i = 0; i < args.size(); i++)
    {
        const auto &arg = args[i];
        if (arg == "--")
        {
            cl._positional.insert(cl._positional.end(), args.begin()+std::ptrdiff_t(i)+1, args.end());
            break;
        }
        if (arg.starts_with("--"))
        {
            const auto eq = arg.find('=');
            const auto name = arg.substr(2, (eq == std::string::npos)? std::string::npos : eq-2);
            const auto *spec = findLong(specs, name);
            if (name.empty() or spec == nullptr) fail("unknown option '" + arg.substr(0, eq) + "' (see --help)");
            if (eq != std::string::npos)
            {
                if (spec->arg == Arg::None) fail("option '--" + name + "' takes no value");
                add(*spec, arg.substr(eq+1));
            }
            else if (spec->arg == Arg::Required)
            {
                if (i+1 >= args.size()) fail("option '--" + name + "' requires a value");
                add(*spec, args[++i]);
            }
            else add(*spec, "");
            continue;
        }
        if (arg.size() == 2 and arg[0] == '-' and arg[1] != '-')
        {
            const auto *spec = findShort(specs, arg[1]);
            if (spec == nullptr) fail("unknown option '" + arg + "' (see --help)");
            if (spec->arg == Arg::Required)
            {
                if (i+1 >= args.size()) fail("option '" + arg + "' requires a value");
                add(*spec, args[++i]);
            }
            else add(*spec, "");
            continue;
        }
        if (arg.size() > 1 and arg[0] == '-') fail("unknown option '" + arg + "' (see --help)");
        cl._positional.push_back(arg);
    }
    return cl;
}

std::string Pothos::App::formatHelp(const std::vector<OptionSpec> &specs, const std::string_view usage, const std::string_view header)
{
    std::string out = "usage: " + std::string(usage) + "\n";
    if (not header.empty())
    {
        out += "\n";
        for (const auto &line : wrap(std::string(header), 78)) out += line + "\n";
    }
    out += "\n";
    for (const auto &spec : specs)
    {
        if (spec.hidden) continue;
        std::string left = "  ";
        left += (spec.shortName != 0)? std::string("-") + spec.shortName + ", " : std::string("    ");
        left += "--" + spec.name;
        if (spec.arg == Arg::Required) left += "=" + spec.argName;
        if (spec.arg == Arg::Optional) left += "[=" + spec.argName + "]";
        out += left + "\n";
        for (const auto &line : wrap(spec.help, 70)) out += "        " + line + "\n";
    }
    return out;
}
