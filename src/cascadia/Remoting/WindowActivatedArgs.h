// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "WindowActivatedArgs.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowActivatedArgs : public WindowActivatedArgsT<WindowActivatedArgs>
    {
        GETSET_PROPERTY(uint64_t, PeasantID, 0);
        GETSET_PROPERTY(winrt::guid, DesktopID, {});
        GETSET_PROPERTY(winrt::Windows::Foundation::DateTime, ActivatedTime, {});

    public:
        WindowActivatedArgs(uint64_t peasantID, winrt::guid desktopID, winrt::Windows::Foundation::DateTime timestamp) :
            _PeasantID{ peasantID },
            _DesktopID{ desktopID },
            _ActivatedTime{ timestamp } {};
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowActivatedArgs);
}
