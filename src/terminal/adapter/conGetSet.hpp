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

        virtual void PrintString(const std::wstring_view string) = 0;

        virtual void GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const = 0;
        virtual void SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) = 0;
        virtual void SetCursorPosition(const COORD position) = 0;

        virtual bool IsVtInputEnabled() const = 0;

        virtual TextAttribute GetTextAttributes() const = 0;
        virtual void SetTextAttributes(const TextAttribute& attrs) = 0;

        virtual void SetCurrentLineRendition(const LineRendition lineRendition) = 0;
        virtual void ResetLineRenditionRange(const size_t startRow, const size_t endRow) = 0;
        virtual SHORT GetLineWidth(const size_t row) const = 0;

        virtual void WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten) = 0;
        virtual void SetWindowInfo(const bool absolute, const SMALL_RECT& window) = 0;

        virtual bool SetInputMode(const TerminalInput::Mode mode, const bool enabled) = 0;
        virtual void SetParserMode(const StateMachine::Mode mode, const bool enabled) = 0;
        virtual bool GetParserMode(const StateMachine::Mode mode) const = 0;
        virtual void SetRenderMode(const RenderSettings::Mode mode, const bool enabled) = 0;

        virtual void SetAutoWrapMode(const bool wrapAtEOL) = 0;

        virtual void SetCursorVisibility(const bool visible) = 0;
        virtual bool EnableCursorBlinking(const bool enable) = 0;

        virtual void SetScrollingRegion(const SMALL_RECT& scrollMargins) = 0;
        virtual void WarningBell() = 0;
        virtual bool GetLineFeedMode() const = 0;
        virtual void LineFeed(const bool withReturn) = 0;
        virtual void ReverseLineFeed() = 0;
        virtual void SetWindowTitle(const std::wstring_view title) = 0;
        virtual void UseAlternateScreenBuffer() = 0;
        virtual void UseMainScreenBuffer() = 0;

        virtual void EraseAll() = 0;
        virtual void ClearBuffer() = 0;
        virtual CursorType GetUserDefaultCursorStyle() const = 0;
        virtual void SetCursorStyle(const CursorType style) = 0;
        virtual void WriteControlInput(const KeyEvent key) = 0;
        virtual void RefreshWindow() = 0;

        virtual void SetConsoleOutputCP(const unsigned int codepage) = 0;
        virtual unsigned int GetConsoleOutputCP() const = 0;

        virtual bool ResizeWindow(const size_t width, const size_t height) = 0;
        virtual void SuppressResizeRepaint() = 0;
        virtual bool IsConsolePty() const = 0;

        virtual void DeleteLines(const size_t count) = 0;
        virtual void InsertLines(const size_t count) = 0;

        virtual void MoveToBottom() = 0;

        virtual COLORREF GetColorTableEntry(const size_t tableIndex) const = 0;
        virtual bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) = 0;
        virtual void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) = 0;

        virtual void FillRegion(const COORD startPosition,
                                const size_t fillLength,
                                const wchar_t fillChar,
                                const bool standardFillAttrs) = 0;

        virtual void ScrollRegion(const SMALL_RECT scrollRect,
                                  const std::optional<SMALL_RECT> clipRect,
                                  const COORD destinationOrigin,
                                  const bool standardFillAttrs) = 0;

        virtual void AddHyperlink(const std::wstring_view uri, const std::wstring_view params) const = 0;
        virtual void EndHyperlink() const = 0;

        virtual void UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                                    const SIZE cellSize,
                                    const size_t centeringHint) = 0;
    };
}
