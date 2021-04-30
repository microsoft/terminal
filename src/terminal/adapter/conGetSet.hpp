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

#include "../../types/inc/IInputEvent.hpp"
#include "../../buffer/out/LineRendition.hpp"
#include "../../buffer/out/TextAttribute.hpp"
#include "../../inc/conattrs.hpp"

#include <deque>
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    class ConGetSet
    {
    public:
        virtual ~ConGetSet() = default;
        virtual bool GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const = 0;
        virtual bool SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) = 0;
        virtual bool SetConsoleCursorPosition(const COORD position) = 0;

        virtual bool PrivateIsVtInputEnabled() const = 0;

        virtual bool PrivateGetTextAttributes(TextAttribute& attrs) const = 0;
        virtual bool PrivateSetTextAttributes(const TextAttribute& attrs) = 0;

        virtual bool PrivateSetCurrentLineRendition(const LineRendition lineRendition) = 0;
        virtual bool PrivateResetLineRenditionRange(const size_t startRow, const size_t endRow) = 0;
        virtual SHORT PrivateGetLineWidth(const size_t row) const = 0;

        virtual bool PrivateWriteConsoleInputW(std::deque<std::unique_ptr<IInputEvent>>& events,
                                               size_t& eventsWritten) = 0;
        virtual bool SetConsoleWindowInfo(const bool absolute,
                                          const SMALL_RECT& window) = 0;
        virtual bool PrivateSetCursorKeysMode(const bool applicationMode) = 0;
        virtual bool PrivateSetKeypadMode(const bool applicationMode) = 0;
        virtual bool PrivateEnableWin32InputMode(const bool win32InputMode) = 0;

        virtual bool PrivateSetAnsiMode(const bool ansiMode) = 0;
        virtual bool PrivateSetScreenMode(const bool reverseMode) = 0;
        virtual bool PrivateSetAutoWrapMode(const bool wrapAtEOL) = 0;

        virtual bool PrivateShowCursor(const bool show) = 0;
        virtual bool PrivateAllowCursorBlinking(const bool enable) = 0;

        virtual bool PrivateSetScrollingRegion(const SMALL_RECT& scrollMargins) = 0;
        virtual bool PrivateWarningBell() = 0;
        virtual bool PrivateGetLineFeedMode() const = 0;
        virtual bool PrivateLineFeed(const bool withReturn) = 0;
        virtual bool PrivateReverseLineFeed() = 0;
        virtual bool SetConsoleTitleW(const std::wstring_view title) = 0;
        virtual bool PrivateUseAlternateScreenBuffer() = 0;
        virtual bool PrivateUseMainScreenBuffer() = 0;

        virtual bool PrivateEnableVT200MouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableUTF8ExtendedMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableSGRExtendedMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableButtonEventMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableAnyEventMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableAlternateScroll(const bool enabled) = 0;
        virtual bool PrivateEraseAll() = 0;
        virtual bool GetUserDefaultCursorStyle(CursorType& style) = 0;
        virtual bool SetCursorStyle(const CursorType style) = 0;
        virtual bool SetCursorColor(const COLORREF color) = 0;
        virtual bool PrivateWriteConsoleControlInput(const KeyEvent key) = 0;
        virtual bool PrivateRefreshWindow() = 0;

        virtual bool SetConsoleOutputCP(const unsigned int codepage) = 0;
        virtual bool GetConsoleOutputCP(unsigned int& codepage) = 0;

        virtual bool PrivateSuppressResizeRepaint() = 0;
        virtual bool IsConsolePty() const = 0;

        virtual bool DeleteLines(const size_t count) = 0;
        virtual bool InsertLines(const size_t count) = 0;

        virtual bool MoveToBottom() const = 0;

        virtual bool PrivateGetColorTableEntry(const size_t index, COLORREF& value) const = 0;
        virtual bool PrivateSetColorTableEntry(const size_t index, const COLORREF value) const = 0;
        virtual bool PrivateSetDefaultForeground(const COLORREF value) const = 0;
        virtual bool PrivateSetDefaultBackground(const COLORREF value) const = 0;

        virtual bool PrivateFillRegion(const COORD startPosition,
                                       const size_t fillLength,
                                       const wchar_t fillChar,
                                       const bool standardFillAttrs) = 0;

        virtual bool PrivateScrollRegion(const SMALL_RECT scrollRect,
                                         const std::optional<SMALL_RECT> clipRect,
                                         const COORD destinationOrigin,
                                         const bool standardFillAttrs) = 0;

        virtual bool PrivateAddHyperlink(const std::wstring_view uri, const std::wstring_view params) const = 0;
        virtual bool PrivateEndHyperlink() const = 0;
    };
}
