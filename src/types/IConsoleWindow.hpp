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

#include "IUiaWindow.h"

// copied typedef from uiautomationcore.h
typedef int EVENTID;

namespace Microsoft::Console::Types
{
    class IConsoleWindow : public IUiaWindow
    {
    public:
        virtual ~IConsoleWindow() = 0;

        virtual BOOL EnableBothScrollBars() = 0;
        virtual int UpdateScrollBar(_In_ bool isVertical,
                                    _In_ bool isAltBuffer,
                                    _In_ UINT pageSize,
                                    _In_ int maxSize,
                                    _In_ int viewportPosition) = 0;

        virtual bool IsInFullscreen() const = 0;

        virtual void SetIsFullscreen(const bool fFullscreenEnabled) = 0;

        virtual void CaptureMouse() = 0;
        virtual BOOL ReleaseMouse() = 0;

        // Pass null.
        virtual void SetOwner() = 0;

        virtual BOOL GetCursorPosition(_Out_ LPPOINT lpPoint) = 0;
        virtual BOOL GetClientRectangle(_Out_ LPRECT lpRect) = 0;
        virtual int MapPoints(_Inout_updates_(cPoints) LPPOINT lpPoints, _In_ UINT cPoints) = 0;
        virtual BOOL ConvertScreenToClient(_Inout_ LPPOINT lpPoint) = 0;

        virtual BOOL SendNotifyBeep() const = 0;

        virtual BOOL PostUpdateScrollBars() const = 0;

        virtual BOOL PostUpdateWindowSize() const = 0;

        virtual void UpdateWindowSize(const COORD coordSizeInChars) = 0;
        virtual void UpdateWindowText() = 0;

        virtual void HorizontalScroll(const WORD wScrollCommand,
                                      const WORD wAbsoluteChange) = 0;
        virtual void VerticalScroll(const WORD wScrollCommand,
                                    const WORD wAbsoluteChange) = 0;
    };

    inline IConsoleWindow::~IConsoleWindow() {}
}
