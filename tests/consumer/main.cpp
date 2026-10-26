// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Object.hpp>
#include <Pothos/System/Version.hpp>
#include <iostream>

int main(void)
{
    const Pothos::Object obj(42);
    if (obj.convert<int>() != 42)
    {
        std::cerr << "Object conversion failed" << std::endl;
        return 1;
    }
    if (Pothos::System::getApiVersion().empty())
    {
        std::cerr << "API version is empty" << std::endl;
        return 1;
    }
    std::cout << "consumer OK, Pothos API " << Pothos::System::getApiVersion() << std::endl;
    return 0;
}
