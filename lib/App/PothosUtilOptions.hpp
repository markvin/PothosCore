// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//The command line options of PothosUtil (internal, not installed)

#pragma once
#include "App/Options.hpp"
#include <vector>

namespace Pothos {
namespace App {

//! The options of PothosUtil and their help (shared with the self-tests)
std::vector<OptionSpec> pothosUtilOptions(void);

} //namespace App
} //namespace Pothos
