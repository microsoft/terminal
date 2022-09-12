/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- SummonWindowBehavior.h

Abstract:
- This is a helper class for encapsulating all the information about how a
  window should be summoned. Includes info like if we should switch desktops or
  monitors, if we should dropdown (and how fast), and if we should toggle to
  hidden if we're already visible. Used by the Monarch to tell a Peasant how it
  should behave.

--*/

#pragma once

#include "SummonWindowBehavior.g.h"

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
