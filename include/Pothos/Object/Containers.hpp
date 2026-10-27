///
/// \file Object/Containers.hpp
///
/// Common typedefs for stl containers of type Object.
///
/// \copyright
/// Copyright (c) 2013-2014 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Object.hpp>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace Pothos {

using ObjectVector = std::vector<Object>;

using ObjectSet = std::set<Object>;

using ObjectMap = std::map<Object, Object>;

using ObjectKwargs = std::map<std::string, Object>;

} // namespace Pothos
