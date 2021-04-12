/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- SummonWindowSelectionArgs.h

Abstract: TODO!

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

        WINRT_PROPERTY(winrt::hstring, WindowName);
        WINRT_PROPERTY(bool, FoundMatch, true);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(SummonWindowSelectionArgs);
}
