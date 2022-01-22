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

#include "../terminal/adapter/adaptDefaults.hpp"
#include "../types/inc/IInputEvent.hpp"
#include "../inc/conattrs.hpp"
#include "IIoProvider.hpp"

class SCREEN_INFORMATION;

// The WriteBuffer class provides helpers for writing text into the TextBuffer that is backing a particular console screen buffer.
class WriteBuffer : public Microsoft::Console::VirtualTerminal::AdaptDefaults
{
public:
    WriteBuffer(_In_ Microsoft::Console::IIoProvider& io);

    // Implement Adapter callbacks for default cases (non-escape sequences)
    void Print(const wchar_t wch) override;
    void PrintString(const std::wstring_view string) override;
    void Execute(const wchar_t wch) override;

    [[nodiscard]] NTSTATUS GetResult() { return _ntstatus; };

private:
    void _DefaultCase(const wchar_t wch);
    void _DefaultStringCase(const std::wstring_view string);

    Microsoft::Console::IIoProvider& _io;
    NTSTATUS _ntstatus;
};

#include "../terminal/adapter/conGetSet.hpp"

// The ConhostInternalGetSet is for the Conhost process to call the entrypoints for its own Get/Set APIs.
// Normally, these APIs are accessible from the outside of the conhost process (like by the process being "hosted") through
// the kernelbase/32 exposed public APIs and routed by the console driver (condrv) to this console host.
// But since we're trying to call them from *inside* the console host itself, we need to get in the way and route them straight to the
// v-table inside this process instance.
class ConhostInternalGetSet final : public Microsoft::Console::VirtualTerminal::ConGetSet
{
public:
    ConhostInternalGetSet(_In_ Microsoft::Console::IIoProvider& io);

    bool GetConsoleScreenBufferInfoEx(CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) const override;
    bool SetConsoleScreenBufferInfoEx(const CONSOLE_SCREEN_BUFFER_INFOEX& screenBufferInfo) override;

    bool SetCursorPosition(const COORD position) override;

    bool GetTextAttributes(TextAttribute& attrs) const override;
    bool SetTextAttributes(const TextAttribute& attrs) override;

    bool SetCurrentLineRendition(const LineRendition lineRendition) override;
    bool ResetLineRenditionRange(const size_t startRow, const size_t endRow) override;
    SHORT GetLineWidth(const size_t row) const override;

    bool WriteInput(std::deque<std::unique_ptr<IInputEvent>>& events, size_t& eventsWritten) override;

    bool SetWindowInfo(bool const absolute, const SMALL_RECT& window) override;

    bool SetInputMode(const Microsoft::Console::VirtualTerminal::TerminalInput::Mode mode, const bool enabled) override;
    bool SetParserMode(const Microsoft::Console::VirtualTerminal::StateMachine::Mode mode, const bool enabled) override;
    bool GetParserMode(const Microsoft::Console::VirtualTerminal::StateMachine::Mode mode) const override;
    bool SetRenderMode(const RenderSettings::Mode mode, const bool enabled) override;

    bool SetAutoWrapMode(const bool wrapAtEOL) override;

    bool SetCursorVisibility(const bool visible) override;
    bool EnableCursorBlinking(const bool enable) override;

    bool SetScrollingRegion(const SMALL_RECT& scrollMargins) override;

    bool WarningBell() override;

    bool GetLineFeedMode() const override;
    bool LineFeed(const bool withReturn) override;
    bool ReverseLineFeed() override;

    bool SetWindowTitle(const std::wstring_view title) override;

    bool UseAlternateScreenBuffer() override;

    bool UseMainScreenBuffer() override;

    bool EraseAll() override;
    bool ClearBuffer() override;

    bool GetUserDefaultCursorStyle(CursorType& style) override;
    bool SetCursorStyle(CursorType const style) override;

    bool RefreshWindow() override;

    bool SuppressResizeRepaint() override;

    bool WriteControlInput(const KeyEvent key) override;

    bool SetConsoleOutputCP(const unsigned int codepage) override;
    bool GetConsoleOutputCP(unsigned int& codepage) override;

    bool IsConsolePty() const override;

    bool DeleteLines(const size_t count) override;
    bool InsertLines(const size_t count) override;

    bool MoveToBottom() const override;

    COLORREF GetColorTableEntry(const size_t tableIndex) const override;
    bool SetColorTableEntry(const size_t tableIndex, const COLORREF color) override;
    void SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) override;

    bool FillRegion(const COORD startPosition,
                    const size_t fillLength,
                    const wchar_t fillChar,
                    const bool standardFillAttrs) override;

    bool ScrollRegion(const SMALL_RECT scrollRect,
                      const std::optional<SMALL_RECT> clipRect,
                      const COORD destinationOrigin,
                      const bool standardFillAttrs) override;

    bool IsVtInputEnabled() const override;

    bool AddHyperlink(const std::wstring_view uri, const std::wstring_view params) const override;
    bool EndHyperlink() const override;

    bool UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                        const SIZE cellSize,
                        const size_t centeringHint) override;

private:
    void _modifyLines(const size_t count, const bool insert);

    Microsoft::Console::IIoProvider& _io;
};
