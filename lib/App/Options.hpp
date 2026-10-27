// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Command line options of PothosUtil (internal, not installed)

#pragma once
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Pothos {
namespace App {

enum class Arg
{
    None,     //!< --flag
    Required, //!< --opt=value or --opt value
    Optional, //!< --opt or --opt=value
};

struct OptionSpec
{
    std::string name;
    char shortName = 0;
    Arg arg = Arg::None;
    std::string argName;
    std::string help;
    bool repeatable = false;
    bool hidden = false; //!< not listed by formatHelp
};

//! The parsed command line
class CommandLine
{
public:
    //! The options in the order given (name, value)
    const std::vector<std::pair<std::string, std::string>> &given(void) const;

    //! Arguments that are not options (and everything after --)
    const std::vector<std::string> &positional(void) const;

    bool has(std::string_view name) const;
    bool hasOption(std::string_view name) const; //!< same as has()

    //! The last value given for an option, or def
    std::string getString(std::string_view name, std::string def = {}) const;

    //! Every value given for a repeatable option
    std::vector<std::string> getAll(std::string_view name) const;

    //! \throws Pothos::InvalidArgumentException when the value is not a number of that type
    int getInt(std::string_view name, int def) const;
    unsigned getUInt(std::string_view name, unsigned def) const;
    double getDouble(std::string_view name, double def) const;

    std::vector<std::pair<std::string, std::string>> _given;
    std::vector<std::string> _positional;
};

/*!
 * Parse arguments (without the program name).
 * \throws Pothos::InvalidArgumentException for an unknown option, a missing,
 * unexpected or repeated value
 */
CommandLine parseCommandLine(const std::vector<OptionSpec> &specs, const std::vector<std::string> &args);

//! Usage line, header and the visible options with their help, wrapped
std::string formatHelp(const std::vector<OptionSpec> &specs, std::string_view usage, std::string_view header);

} //namespace App
} //namespace Pothos
