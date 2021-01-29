/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- WindowActivatedArgs.h

Abstract:
- This is a helper class for encapsulating all the information about when and
  where a window was activated. This will be used by the Monarch to determine
  who the most recent peasant is.

--*/
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
