#pragma once

#include "ScratchClass.g.h"
namespace winrt::ScratchWinRTServer::implementation
{
    struct ScratchClass : public ScratchClassT<ScratchClass>
    {
        ScratchClass();
        hstring DoTheThing()
        {
            return L"Hello there";
        }
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct ScratchClass : ScratchClassT<ScratchClass, implementation::ScratchClass>
    {
    };
}
