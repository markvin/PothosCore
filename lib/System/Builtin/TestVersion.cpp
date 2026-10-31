// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/System/Version.hpp>
#include <string>

//pothos2 is version 2.0: the ABI, the API and the library version agree,
//and they do not collide with the 0.x line of upstream (module dir, soname)
POTHOS_TEST_BLOCK("/system/tests", test_version_numbers)
{
    POTHOS_TEST_EQUAL(std::string(POTHOS_ABI_VERSION), "2.0-0");
    POTHOS_TEST_EQUAL(Pothos::System::getAbiVersion(), POTHOS_ABI_VERSION);
    POTHOS_TEST_EQUAL(Pothos::System::getApiVersion(), "2.0.0");
    const auto libVersion = Pothos::System::getLibVersion();
    POTHOS_TEST_EQUAL(libVersion.substr(0, 6), "2.0.0-");
}
