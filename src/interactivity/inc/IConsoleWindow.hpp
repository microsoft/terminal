/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IConsoleWindow.hpp

Abstract:
- Defines the methods and properties of what makes a window into a console
  window.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

// copied typedef from uiautomationcore.h
typedef int EVENTID;

namespace Microsoft::Console::Types
{
    class IConsoleWindow
    {
    public:
        virtual ~IConsoleWindow() = default;

        virtual bool IsInFullscreen() const = 0;

        virtual void SetIsFullscreen(const bool fFullscreenEnabled) = 0;

        virtual void ChangeViewport(const til::inclusive_rect& NewWindow) = 0;

        virtual void CaptureMouse() = 0;
        virtual BOOL ReleaseMouse() = 0;

        virtual HWND GetWindowHandle() const = 0;

        // Pass null.
        virtual void SetOwner() = 0;

        virtual BOOL GetCursorPosition(_Out_ til::point* lpPoint) = 0;
        virtual BOOL GetClientRectangle(_Out_ til::rect* lpRect) = 0;
        virtual BOOL MapRect(_Inout_ til::rect* lpRect) = 0;
        virtual BOOL ConvertScreenToClient(_Inout_ til::point* lpPoint) = 0;

        virtual BOOL SendNotifyBeep() const = 0;

        virtual BOOL PostUpdateScrollBars() const = 0;

        virtual BOOL PostUpdateWindowSize() const = 0;

        virtual void UpdateWindowSize(const til::size coordSizeInChars) = 0;
        virtual void UpdateWindowText() = 0;

        virtual void HorizontalScroll(const WORD wScrollCommand,
                                      const WORD wAbsoluteChange) = 0;
        virtual void VerticalScroll(const WORD wScrollCommand,
                                    const WORD wAbsoluteChange) = 0;

        [[nodiscard]] virtual HRESULT UiaSetTextAreaFocus() = 0;
        virtual til::rect GetWindowRect() const noexcept = 0;
    };
}
