/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleWindow.hpp

Abstract:
- OneCore implementation of the IConsoleWindow interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "..\inc\IConsoleWindow.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class ConsoleWindow sealed : public IConsoleWindow
    {
    public:
        // Inherited via IConsoleWindow
        BOOL EnableBothScrollBars();
        int UpdateScrollBar(bool isVertical, bool isAltBuffer, UINT pageSize, int maxSize, int viewportPosition);

        bool IsInFullscreen() const;
        void SetIsFullscreen(bool const fFullscreenEnabled);
        void ChangeViewport(const SMALL_RECT NewWindow);

        void CaptureMouse();
        BOOL ReleaseMouse();

        HWND GetWindowHandle() const;

        void SetOwner();

        BOOL GetCursorPosition(LPPOINT lpPoint);
        BOOL GetClientRectangle(LPRECT lpRect);
        int MapPoints(LPPOINT lpPoints, UINT cPoints);
        BOOL ConvertScreenToClient(LPPOINT lpPoint);

        BOOL SendNotifyBeep() const;

        BOOL PostUpdateScrollBars() const;
        BOOL PostUpdateTitleWithCopy(const PCWSTR pwszNewTitle) const;
        BOOL PostUpdateWindowSize() const;

        void UpdateWindowSize(COORD const coordSizeInChars);
        void UpdateWindowText();

        void HorizontalScroll(const WORD wScrollCommand, const WORD wAbsoluteChange);
        void VerticalScroll(const WORD wScrollCommand, const WORD wAbsoluteChange);

        [[nodiscard]] HRESULT SignalUia(_In_ EVENTID id);
        [[nodiscard]] HRESULT UiaSetTextAreaFocus();
        RECT GetWindowRect() const;
    };
}
