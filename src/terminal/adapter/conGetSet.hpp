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
        virtual ~ConGetSet() = default;
        virtual bool GetConsoleCursorInfo(CONSOLE_CURSOR_INFO& cursorInfo) const = 0;
        virtual bool GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const = 0;
        virtual bool SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) = 0;
        virtual bool SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO& cursorInfo) = 0;
        virtual bool SetConsoleCursorPosition(const COORD position) = 0;
        virtual bool SetConsoleTextAttribute(const WORD attr) = 0;

        virtual bool PrivateIsVtInputEnabled() const = 0;

        virtual bool PrivateSetLegacyAttributes(const WORD attr,
                                                const bool foreground,
                                                const bool background,
                                                const bool meta) = 0;

        virtual bool PrivateSetDefaultAttributes(const bool foreground, const bool background) = 0;

        virtual bool SetConsoleXtermTextAttribute(const int xtermTableEntry,
                                                  const bool isForeground) = 0;
        virtual bool SetConsoleRGBTextAttribute(const COLORREF rgbColor, const bool isForeground) = 0;
        virtual bool PrivateBoldText(const bool bolded) = 0;
        virtual bool PrivateGetExtendedTextAttributes(ExtendedAttributes& attrs) = 0;
        virtual bool PrivateSetExtendedTextAttributes(const ExtendedAttributes attrs) = 0;
        virtual bool PrivateGetTextAttributes(TextAttribute& attrs) const = 0;
        virtual bool PrivateSetTextAttributes(const TextAttribute& attrs) = 0;

        virtual bool PrivateWriteConsoleInputW(std::deque<std::unique_ptr<IInputEvent>>& events,
                                               size_t& eventsWritten) = 0;
        virtual bool SetConsoleWindowInfo(const bool absolute,
                                          const SMALL_RECT& window) = 0;
        virtual bool PrivateSetCursorKeysMode(const bool applicationMode) = 0;
        virtual bool PrivateSetKeypadMode(const bool applicationMode) = 0;

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
        virtual bool PrivateHorizontalTabSet() = 0;
        virtual bool PrivateForwardTab(const size_t numTabs) = 0;
        virtual bool PrivateBackwardsTab(const size_t numTabs) = 0;
        virtual bool PrivateTabClear(const bool clearAll) = 0;
        virtual bool PrivateSetDefaultTabStops() = 0;

        virtual bool PrivateEnableVT200MouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableUTF8ExtendedMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableSGRExtendedMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableButtonEventMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableAnyEventMouseMode(const bool enabled) = 0;
        virtual bool PrivateEnableAlternateScroll(const bool enabled) = 0;
        virtual bool PrivateEraseAll() = 0;
        virtual bool SetCursorStyle(const CursorType style) = 0;
        virtual bool SetCursorColor(const COLORREF color) = 0;
        virtual bool PrivateGetConsoleScreenBufferAttributes(WORD& attributes) = 0;
        virtual bool PrivatePrependConsoleInput(std::deque<std::unique_ptr<IInputEvent>>& events,
                                                size_t& eventsWritten) = 0;
        virtual bool PrivateWriteConsoleControlInput(const KeyEvent key) = 0;
        virtual bool PrivateRefreshWindow() = 0;

        virtual bool GetConsoleOutputCP(unsigned int& codepage) = 0;

        virtual bool PrivateSuppressResizeRepaint() = 0;
        virtual bool IsConsolePty(bool& isPty) const = 0;

        virtual bool DeleteLines(const size_t count) = 0;
        virtual bool InsertLines(const size_t count) = 0;

        virtual bool MoveToBottom() const = 0;

        virtual bool PrivateSetColorTableEntry(const short index, const COLORREF value) const = 0;
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
    };
}
