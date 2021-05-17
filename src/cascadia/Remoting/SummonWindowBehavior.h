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
        WINRT_PROPERTY(bool, ToggleVisibility, true);
        WINRT_PROPERTY(uint32_t, DropdownDuration, 0);
        WINRT_PROPERTY(Remoting::MonitorBehavior, ToMonitor, Remoting::MonitorBehavior::ToCurrent);

    public:
        SummonWindowBehavior(const Remoting::SummonWindowBehavior& other) :
            _MoveToCurrentDesktop{ other.MoveToCurrentDesktop() },
            _ToMonitor{ other.ToMonitor() },
            _DropdownDuration{ other.DropdownDuration() },
            _ToggleVisibility{ other.ToggleVisibility() } {};
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(SummonWindowBehavior);
}
