/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IUiaWindow.hpp

Abstract:
- Defines the methods and properties of what makes a window generate a UIA Tree for accessibility

Author(s):
- Carlos Zamora (CaZamor) July 2019
--*/

#pragma once

// copied typedef from uiautomationcore.h
typedef int EVENTID;

namespace Microsoft::Console::Types
{
    class IUiaWindow
    {
    public:
        virtual void ChangeViewport(const SMALL_RECT NewWindow) = 0;
        virtual HWND GetWindowHandle() const = 0;
        [[nodiscard]] virtual HRESULT SignalUia(_In_ EVENTID id) = 0;
        [[nodiscard]] virtual HRESULT UiaSetTextAreaFocus() = 0;
        virtual RECT GetWindowRect() const noexcept = 0;
    };
}
