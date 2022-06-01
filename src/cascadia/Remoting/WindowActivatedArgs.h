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

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct CompareWindowActivatedArgs
    {
        bool operator()(const Remoting::WindowActivatedArgs& lhs, const Remoting::WindowActivatedArgs& rhs) const
        {
            return lhs.ActivatedTime() > rhs.ActivatedTime();
        }
    };
    struct WindowActivatedArgs : public WindowActivatedArgsT<WindowActivatedArgs>
    {
        WINRT_PROPERTY(uint64_t, PeasantID, 0);
        WINRT_PROPERTY(winrt::guid, DesktopID);
        WINRT_PROPERTY(winrt::Windows::Foundation::DateTime, ActivatedTime, {});
        WINRT_PROPERTY(uint64_t, Hwnd, 0);

    public:
        WindowActivatedArgs(uint64_t peasantID,
                            uint64_t hwnd,
                            winrt::guid desktopID,
                            winrt::Windows::Foundation::DateTime timestamp) :
            _PeasantID{ peasantID },
            _Hwnd{ hwnd },
            _DesktopID{ desktopID },
            _ActivatedTime{ timestamp } {};

        WindowActivatedArgs(uint64_t peasantID,
                            winrt::guid desktopID,
                            winrt::Windows::Foundation::DateTime timestamp) :
            WindowActivatedArgs(peasantID, 0, desktopID, timestamp){};

        WindowActivatedArgs(const Remoting::WindowActivatedArgs& other) :
            _PeasantID{ other.PeasantID() },
            _Hwnd{ other.Hwnd() },
            _DesktopID{ other.DesktopID() },
            _ActivatedTime{ other.ActivatedTime() } {};
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowActivatedArgs);
}
