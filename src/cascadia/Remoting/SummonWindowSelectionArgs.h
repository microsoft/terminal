/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- SummonWindowSelectionArgs.h

Abstract:
- This is a helper class for determining which window a should be summoned when
  a global hotkey is pressed. Parameters from a GlobalSummon action will be
  filled in here. The Monarch will use these to find the window that matches
  these args, and Summon() that Peasant.
- When the monarch finds a match, it will set FoundMatch to true. If it doesn't,
  then the Monarch window might need to create a new window matching these args
  instead.
--*/

#pragma once

#include "SummonWindowSelectionArgs.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct SummonWindowSelectionArgs : public SummonWindowSelectionArgsT<SummonWindowSelectionArgs>
    {
    public:
        SummonWindowSelectionArgs() = default;
        SummonWindowSelectionArgs(winrt::hstring name) :
            _WindowName{ name } {};

        WINRT_PROPERTY(winrt::hstring, WindowName);

        WINRT_PROPERTY(bool, FoundMatch, false);
        WINRT_PROPERTY(bool, OnCurrentDesktop, false);
        WINRT_PROPERTY(SummonWindowBehavior, SummonBehavior);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(SummonWindowSelectionArgs);
}
