// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <iostream>

bool apiCompatSpinLock(void);
bool apiCompatTemplates(void);

int main(void)
{
    if (not apiCompatSpinLock())
    {
        std::cerr << "SpinLock sample failed" << std::endl;
        return 1;
    }
    if (not apiCompatTemplates())
    {
        std::cerr << "Templates sample failed" << std::endl;
        return 1;
    }
    std::cout << "api_compat OK" << std::endl;
    return 0;
}
