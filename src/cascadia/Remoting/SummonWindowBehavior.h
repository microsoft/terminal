/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- SummonWindowBehavior.h

Abstract:
- A helper class for encapsulating the various properties that control _how_ a
  window should be summoned, when it is summoned. This will be created from the
  properties in a GlobalSummonArgs, and passed into a SummonWindowSelectionArgs.
  When the window is summoned, the Monarch will use these args in the
  SummonWindowSelectionArgs to tell the peasant how to be summoned.

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
