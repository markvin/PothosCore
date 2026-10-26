// Copyright (c) 2013-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Plugin/Static.hpp>
#include <Pothos/Exception.hpp>
#include <Pothos/System/Logger.hpp>

void Pothos::Detail::safeInit(const std::string &clientAbi, const std::string &name, InitFcn init)
{
    if (clientAbi != Pothos::System::getAbiVersion())
    {
        POTHOS_LOG_ERROR("Pothos.StaticBlock.safeInit", "ABI mismatch when loading {}\n"
            "Client expected ABI {} but got {} from the library.",
            name, clientAbi, Pothos::System::getAbiVersion());
        return;
    }
    POTHOS_EXCEPTION_TRY
    {
        init();
    }
    POTHOS_EXCEPTION_CATCH(const Exception &ex)
    {
        POTHOS_LOG_ERROR("Pothos.StaticBlock.safeInit", "loading {}, exception {}", name, ex.displayText());
    }
}
