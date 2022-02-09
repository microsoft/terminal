/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- outputStream.hpp

Abstract:
- Classes to process text written into the console on the attached application's output stream (usually STDOUT).

Author:
- Michael Niksa <miniksa> July 27 2015
--*/

#pragma once

#include "../terminal/adapter/conGetSet.hpp"
#include "../types/inc/IInputEvent.hpp"
#include "../inc/conattrs.hpp"
#include "IIoProvider.hpp"

// The ConhostInternalGetSet is for the Conhost process to call the entrypoints for its own Get/Set APIs.
// Normally, these APIs are accessible from the outside of the conhost process (like by the process being "hosted") through
// the kernelbase/32 exposed public APIs and routed by the console driver (condrv) to this console host.
// But since we're trying to call them from *inside* the console host itself, we need to get in the way and route them straight to the
// v-table inside this process instance.
class ConhostInternalGetSet final : public Microsoft::Console::VirtualTerminal::ConGetSet
{
public:
    ConhostInternalGetSet(_In_ Microsoft::Console::IIoProvider& io);

    void PrintString(const std::wstring_view string) override;

    void GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const override;
    void SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) override;

    void SetCursorPosition(const COORD position) override;

    TextAttribute GetTextAttributes() const override;
    void SetTextAttributes(const TextAttribute& attrs) override;

    void SetCurrentLineRendition(const LineRendition lineRendition) override;
    void ResetLineRenditionRange(const size_t startRow, const size_t endRow) override;
    SHORT GetLineWidth(const size_t row) const override;

    void WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten) override;

    void SetWindowInfo(bool const absolute, const SMALL_RECT& window) override;

    bool SetInputMode(const Microsoft::Console::VirtualTerminal::TerminalInput::Mode mode, const bool enabled) override;
    void SetParserMode(const Microsoft::Console::VirtualTerminal::StateMachine::Mode mode, const bool enabled) override;
    bool GetParserMode(const Microsoft::Console::VirtualTerminal::StateMachine::Mode mode) const override;
    void SetRenderMode(const RenderSettings::Mode mode, const bool enabled) override;

    void SetAutoWrapMode(const bool wrapAtEOL) override;

    void SetCursorVisibility(const bool visible) override;
    bool EnableCursorBlinking(const bool enable) override;

    void SetScrollingRegion(const SMALL_RECT& scrollMargins) override;

    void WarningBell() override;

    bool GetLineFeedMode() const override;
    void LineFeed(const bool withReturn) override;
    void ReverseLineFeed() override;

    void SetWindowTitle(const std::wstring_view title) override;

    void UseAlternateScreenBuffer() override;

    void UseMainScreenBuffer() override;

    void EraseAll() override;
    void ClearBuffer() override;

    CursorType GetUserDefaultCursorStyle() const override;
    void SetCursorStyle(CursorType const style) override;

    void RefreshWindow() override;
    bool ResizeWindow(const size_t width, const size_t height) override;
    void SuppressResizeRepaint() override;

    void WriteControlInput(const KeyEvent key) override;

    void SetConsoleOutputCP(const unsigned int codepage) override;
    unsigned int GetConsoleOutputCP() const override;

    bool IsConsolePty() const override;

    void DeleteLines(const size_t count) override;
    void InsertLines(const size_t count) override;

    void MoveToBottom() override;

    COLORREF GetColorTableEntry(const size_t tableIndex) const override;
    bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) override;
    void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) override;

    void FillRegion(const COORD startPosition,
                    const size_t fillLength,
                    const wchar_t fillChar,
                    const bool standardFillAttrs) override;

    void ScrollRegion(const SMALL_RECT scrollRect,
                      const std::optional<SMALL_RECT> clipRect,
                      const COORD destinationOrigin,
                      const bool standardFillAttrs) override;

    bool IsVtInputEnabled() const override;

    void AddHyperlink(const std::wstring_view uri, const std::wstring_view params) const override;
    void EndHyperlink() const override;

    void UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                        const SIZE cellSize,
                        const size_t centeringHint) override;

private:
    void _modifyLines(const size_t count, const bool insert);

    Microsoft::Console::IIoProvider& _io;
};
