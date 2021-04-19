/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- SummonWindowBehavior.h

Abstract:
- TODO!

--*/

#pragma once

#include "SummonWindowBehavior.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct SummonWindowBehavior : public SummonWindowBehaviorT<SummonWindowBehavior>
    {
    public:
        SummonWindowBehavior() = default;
        WINRT_PROPERTY(bool, MoveToCurrentDesktop, true);

    public:
        SummonWindowBehavior(const Remoting::SummonWindowBehavior& other) :
            _MoveToCurrentDesktop{ other.MoveToCurrentDesktop() } {};
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(SummonWindowBehavior);
}
