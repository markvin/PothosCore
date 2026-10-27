///
/// \file Proxy/Containers.hpp
///
/// Common typedefs for stl containers of type Proxy.
///
/// \copyright
/// Copyright (c) 2013-2014 Josh Blum
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Proxy.hpp>
#include <vector>
#include <set>
#include <map>

namespace Pothos {

using ProxyVector = std::vector<Proxy>;

using ProxySet = std::set<Proxy>;

using ProxyMap = std::map<Proxy, Proxy>;

} // namespace Pothos
