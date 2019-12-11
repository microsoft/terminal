/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- conGetSet.hpp

Abstract:
- This serves as an abstraction layer for the adapters to connect to the console API functions.
- The abstraction allows for the substitution of the functions for internal/external to Conhost.exe use as well as easy testing.

Author(s):
- Michael Niksa (MiNiksa) 30-July-2015
--*/

#pragma once

#include "..\..\types\inc\IInputEvent.hpp"
#include "..\..\buffer\out\TextAttribute.hpp"
#include "..\..\inc\conattrs.hpp"

#include <deque>
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    class ConGetSet
    {
    public:
        virtual BOOL GetConsoleCursorInfo(_In_ CONSOLE_CURSOR_INFO* const pConsoleCursorInfo) const = 0;
        virtual BOOL GetConsoleScreenBufferInfoEx(_Out_ CONSOLE_SCREEN_BUFFER_INFOEX* const pConsoleScreenBufferInfoEx) const = 0;
        virtual BOOL SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX* const pConsoleScreenBufferInfoEx) = 0;
        virtual BOOL SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO* const pConsoleCursorInfo) = 0;
        virtual BOOL SetConsoleCursorPosition(const COORD coordCursorPosition) = 0;
        virtual BOOL FillConsoleOutputCharacterW(const WCHAR wch,
                                                 const DWORD nLength,
                                                 const COORD dwWriteCoord,
                                                 size_t& numberOfCharsWritten) noexcept = 0;
        virtual BOOL FillConsoleOutputAttribute(const WORD wAttribute,
                                                const DWORD nLength,
                                                const COORD dwWriteCoord,
                                                size_t& numberOfAttrsWritten) noexcept = 0;
        virtual BOOL SetConsoleTextAttribute(const WORD wAttr) = 0;

        virtual BOOL PrivateSetLegacyAttributes(const WORD wAttr,
                                                const bool fForeground,
                                                const bool fBackground,
                                                const bool fMeta) = 0;

        virtual BOOL PrivateSetDefaultAttributes(const bool fForeground, const bool fBackground) = 0;

        virtual BOOL SetConsoleXtermTextAttribute(const int iXtermTableEntry,
                                                  const bool fIsForeground) = 0;
        virtual BOOL SetConsoleRGBTextAttribute(const COLORREF rgbColor, const bool fIsForeground) = 0;
        virtual BOOL PrivateBoldText(const bool bolded) = 0;
        virtual BOOL PrivateGetExtendedTextAttributes(ExtendedAttributes* const pAttrs) = 0;
        virtual BOOL PrivateSetExtendedTextAttributes(const ExtendedAttributes attrs) = 0;
        virtual BOOL PrivateGetTextAttributes(TextAttribute* const pAttrs) const = 0;
        virtual BOOL PrivateSetTextAttributes(const TextAttribute& attrs) = 0;

        virtual BOOL PrivateWriteConsoleInputW(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                               _Out_ size_t& eventsWritten) = 0;
        virtual BOOL ScrollConsoleScreenBufferW(const SMALL_RECT* pScrollRectangle,
                                                _In_opt_ const SMALL_RECT* pClipRectangle,
                                                _In_ COORD dwDestinationOrigin,
                                                const CHAR_INFO* pFill) = 0;
        virtual BOOL SetConsoleWindowInfo(const BOOL bAbsolute,
                                          const SMALL_RECT* const lpConsoleWindow) = 0;
        virtual BOOL PrivateSetCursorKeysMode(const bool fApplicationMode) = 0;
        virtual BOOL PrivateSetKeypadMode(const bool fApplicationMode) = 0;

        virtual BOOL PrivateShowCursor(const bool show) = 0;
        virtual BOOL PrivateAllowCursorBlinking(const bool fEnable) = 0;

        virtual BOOL PrivateSetScrollingRegion(const SMALL_RECT* const psrScrollMargins) = 0;
        virtual BOOL PrivateReverseLineFeed() = 0;
        virtual BOOL SetConsoleTitleW(const std::wstring_view title) = 0;
        virtual BOOL PrivateUseAlternateScreenBuffer() = 0;
        virtual BOOL PrivateUseMainScreenBuffer() = 0;
        virtual BOOL PrivateHorizontalTabSet() = 0;
        virtual BOOL PrivateForwardTab(const SHORT sNumTabs) = 0;
        virtual BOOL PrivateBackwardsTab(const SHORT sNumTabs) = 0;
        virtual BOOL PrivateTabClear(const bool fClearAll) = 0;
        virtual BOOL PrivateSetDefaultTabStops() = 0;

        virtual BOOL PrivateEnableVT200MouseMode(const bool fEnabled) = 0;
        virtual BOOL PrivateEnableUTF8ExtendedMouseMode(const bool fEnabled) = 0;
        virtual BOOL PrivateEnableSGRExtendedMouseMode(const bool fEnabled) = 0;
        virtual BOOL PrivateEnableButtonEventMouseMode(const bool fEnabled) = 0;
        virtual BOOL PrivateEnableAnyEventMouseMode(const bool fEnabled) = 0;
        virtual BOOL PrivateEnableAlternateScroll(const bool fEnabled) = 0;
        virtual BOOL PrivateEraseAll() = 0;
        virtual BOOL SetCursorStyle(const CursorType cursorType) = 0;
        virtual BOOL SetCursorColor(const COLORREF cursorColor) = 0;
        virtual BOOL PrivateGetConsoleScreenBufferAttributes(_Out_ WORD* const pwAttributes) = 0;
        virtual BOOL PrivatePrependConsoleInput(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                _Out_ size_t& eventsWritten) = 0;
        virtual BOOL PrivateWriteConsoleControlInput(_In_ KeyEvent key) = 0;
        virtual BOOL PrivateRefreshWindow() = 0;

        virtual BOOL GetConsoleOutputCP(_Out_ unsigned int* const puiOutputCP) = 0;

        virtual BOOL PrivateSuppressResizeRepaint() = 0;
        virtual BOOL IsConsolePty(_Out_ bool* const pIsPty) const = 0;

        virtual BOOL MoveCursorVertically(const short lines) = 0;

        virtual BOOL DeleteLines(const unsigned int count) = 0;
        virtual BOOL InsertLines(const unsigned int count) = 0;

        virtual BOOL MoveToBottom() const = 0;

        virtual BOOL PrivateSetColorTableEntry(const short index, const COLORREF value) const = 0;
        virtual BOOL PrivateSetDefaultForeground(const COLORREF value) const = 0;
        virtual BOOL PrivateSetDefaultBackground(const COLORREF value) const = 0;
    };
}
