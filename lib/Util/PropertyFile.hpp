// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//Property files in the format of Poco::Util::PropertyFileConfiguration
//(.conf files, the module loader cache). Internal, not installed.

#pragma once
#include <filesystem>
#include <iosfwd>
#include <map>
#include <string>

namespace Pothos {
namespace Util {
namespace Detail {

using PropertyMap = std::map<std::string, std::string>;

/*!
 * Parse "key = value" / "key: value" lines: # and ! start comments, keys and
 * values are trimmed, values support \t \r \n \f, \<any> for that character
 * and a trailing \ to continue on the next line. A repeated key keeps the last value.
 */
PropertyMap parseProperties(std::istream &is);

//! "key: value" lines, escaping \t \r \n \f and backslash in values
std::string formatProperties(const PropertyMap &map);

//! \throws Pothos::FileException naming the path
PropertyMap loadPropertyFile(const std::filesystem::path &path);

//! \throws Pothos::FileException naming the path
void savePropertyFile(const std::filesystem::path &path, const PropertyMap &map);

} //namespace Detail
} //namespace Util
} //namespace Pothos
