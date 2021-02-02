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
    struct CompareWindowActivatedArgs
    {
        bool operator()(const Remoting::WindowActivatedArgs& lhs, const Remoting::WindowActivatedArgs& rhs) const
        {
            return lhs.ActivatedTime() < rhs.ActivatedTime();
        }
    };
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
        static bool compare(const WindowActivatedArgs& lhs, const WindowActivatedArgs& rhs) { return lhs._ActivatedTime < rhs._ActivatedTime; }
        static bool compare(const Remoting::WindowActivatedArgs& lhs, const Remoting::WindowActivatedArgs& rhs) { return lhs.ActivatedTime() < rhs.ActivatedTime(); }
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowActivatedArgs);
}
