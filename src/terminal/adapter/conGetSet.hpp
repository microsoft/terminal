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

#include "../input/terminalInput.hpp"
#include "../parser/stateMachine.hpp"
#include "../../types/inc/IInputEvent.hpp"
#include "../../buffer/out/LineRendition.hpp"
#include "../../buffer/out/TextAttribute.hpp"
#include "../../renderer/inc/RenderSettings.hpp"
#include "../../inc/conattrs.hpp"

#include <deque>
#include <memory>

namespace Microsoft::Console::VirtualTerminal
{
    class ConGetSet
    {
        using RenderSettings = Microsoft::Console::Render::RenderSettings;

    public:
        virtual ~ConGetSet() = default;
        virtual bool GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const = 0;
        virtual bool SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) = 0;
        virtual bool SetCursorPosition(const COORD position) = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual bool GetTextAttributes(TextAttribute& attrs) const = 0;
        virtual bool SetTextAttributes(const TextAttribute& attrs) = 0;

        virtual bool SetCurrentLineRendition(const LineRendition lineRendition) = 0;
        virtual bool ResetLineRenditionRange(const size_t startRow, const size_t endRow) = 0;
        virtual SHORT GetLineWidth(const size_t row) const = 0;

        virtual bool WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten) = 0;
        virtual bool SetWindowInfo(const bool absolute, const SMALL_RECT& window) = 0;

        virtual bool SetInputMode(const TerminalInput::Mode mode, const bool enabled) = 0;
        virtual bool SetParserMode(const StateMachine::Mode mode, const bool enabled) = 0;
        virtual bool GetParserMode(const StateMachine::Mode mode) const = 0;
        virtual bool SetRenderMode(const RenderSettings::Mode mode, const bool enabled) = 0;

        virtual bool SetAutoWrapMode(const bool wrapAtEOL) = 0;

        virtual bool SetCursorVisibility(const bool visible) = 0;
        virtual bool EnableCursorBlinking(const bool enable) = 0;

        virtual bool SetScrollingRegion(const SMALL_RECT& scrollMargins) = 0;
        virtual bool WarningBell() = 0;
        virtual bool GetLineFeedMode() const = 0;
        virtual bool LineFeed(const bool withReturn) = 0;
        virtual bool ReverseLineFeed() = 0;
        virtual bool SetWindowTitle(const std::wstring_view title) = 0;
        virtual bool UseAlternateScreenBuffer() = 0;
        virtual bool UseMainScreenBuffer() = 0;

        virtual bool EraseAll() = 0;
        virtual bool ClearBuffer() = 0;
        virtual bool GetUserDefaultCursorStyle(CursorType& style) = 0;
        virtual bool SetCursorStyle(const CursorType style) = 0;
        virtual bool WriteControlInput(const KeyEvent key) = 0;
        virtual bool RefreshWindow() = 0;

        virtual bool SetConsoleOutputCP(const unsigned int codepage) = 0;
        virtual bool GetConsoleOutputCP(unsigned int& codepage) = 0;

        virtual bool SuppressResizeRepaint() = 0;
        virtual bool IsConsolePty() const = 0;

        virtual bool DeleteLines(const size_t count) = 0;
        virtual bool InsertLines(const size_t count) = 0;

        virtual bool MoveToBottom() const = 0;

        virtual COLORREF GetColorTableEntry(const size_t tableIndex) const = 0;
        virtual bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) = 0;
        virtual void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) = 0;

        virtual bool FillRegion(const COORD startPosition,
                                const size_t fillLength,
                                const wchar_t fillChar,
                                const bool standardFillAttrs) = 0;

        virtual bool ScrollRegion(const SMALL_RECT scrollRect,
                                  const std::optional<SMALL_RECT> clipRect,
                                  const COORD destinationOrigin,
                                  const bool standardFillAttrs) = 0;

        virtual bool AddHyperlink(const std::wstring_view uri, const std::wstring_view params) const = 0;
        virtual bool EndHyperlink() const = 0;

        virtual bool UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                                    const SIZE cellSize,
                                    const size_t centeringHint) = 0;
    };
}
