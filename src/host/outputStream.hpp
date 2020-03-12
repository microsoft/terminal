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

#include "..\terminal\adapter\adaptDefaults.hpp"
#include "..\types\inc\IInputEvent.hpp"
#include "..\inc\conattrs.hpp"
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

#include "..\terminal\adapter\conGetSet.hpp"

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

    bool SetConsoleCursorPosition(const COORD position) override;

    bool GetConsoleCursorInfo(CONSOLE_CURSOR_INFO& cursorInfo) const override;
    bool SetConsoleCursorInfo(const CONSOLE_CURSOR_INFO& cursorInfo) override;

    bool SetConsoleTextAttribute(const WORD attr) override;

    bool PrivateSetLegacyAttributes(const WORD attr,
                                    const bool foreground,
                                    const bool background,
                                    const bool meta) override;

    bool PrivateSetDefaultAttributes(const bool foreground,
                                     const bool background) override;

    bool SetConsoleXtermTextAttribute(const int xtermTableEntry,
                                      const bool isForeground) override;

    bool SetConsoleRGBTextAttribute(const COLORREF rgbColor,
                                    const bool isForeground) override;

    bool PrivateBoldText(const bool bolded) override;
    bool PrivateGetExtendedTextAttributes(ExtendedAttributes& attrs) override;
    bool PrivateSetExtendedTextAttributes(const ExtendedAttributes attrs) override;
    bool PrivateGetTextAttributes(TextAttribute& attrs) const override;
    bool PrivateSetTextAttributes(const TextAttribute& attrs) override;

    bool PrivateWriteConsoleInputW(std::deque<std::unique_ptr<IInputEvent>>& events,
                                   size_t& eventsWritten) override;

    bool SetConsoleWindowInfo(bool const absolute,
                              const SMALL_RECT& window) override;

    bool PrivateSetCursorKeysMode(const bool applicationMode) override;
    bool PrivateSetKeypadMode(const bool applicationMode) override;

    bool PrivateSetScreenMode(const bool reverseMode) override;
    bool PrivateSetAutoWrapMode(const bool wrapAtEOL) override;

    bool PrivateShowCursor(const bool show) noexcept override;
    bool PrivateAllowCursorBlinking(const bool enable) override;

    bool PrivateSetScrollingRegion(const SMALL_RECT& scrollMargins) override;

    bool PrivateWarningBell() override;

    bool PrivateGetLineFeedMode() const override;
    bool PrivateLineFeed(const bool withReturn) override;
    bool PrivateReverseLineFeed() override;

    bool SetConsoleTitleW(const std::wstring_view title) override;

    bool PrivateUseAlternateScreenBuffer() override;

    bool PrivateUseMainScreenBuffer() override;

    bool PrivateHorizontalTabSet();
    bool PrivateForwardTab(const size_t numTabs) override;
    bool PrivateBackwardsTab(const size_t numTabs) override;
    bool PrivateTabClear(const bool clearAll) override;
    bool PrivateSetDefaultTabStops() override;

    bool PrivateEnableVT200MouseMode(const bool enabled) override;
    bool PrivateEnableUTF8ExtendedMouseMode(const bool enabled) override;
    bool PrivateEnableSGRExtendedMouseMode(const bool enabled) override;
    bool PrivateEnableButtonEventMouseMode(const bool enabled) override;
    bool PrivateEnableAnyEventMouseMode(const bool enabled) override;
    bool PrivateEnableAlternateScroll(const bool enabled) override;
    bool PrivateEraseAll() override;

    bool PrivateGetConsoleScreenBufferAttributes(WORD& attributes) override;

    bool PrivatePrependConsoleInput(std::deque<std::unique_ptr<IInputEvent>>& events,
                                    size_t& eventsWritten) override;

    bool SetCursorStyle(CursorType const style) override;
    bool SetCursorColor(COLORREF const color) override;

    bool PrivateRefreshWindow() override;

    bool PrivateSuppressResizeRepaint() override;

    bool PrivateWriteConsoleControlInput(const KeyEvent key) override;

    bool GetConsoleOutputCP(unsigned int& codepage) override;

    bool IsConsolePty(bool& isPty) const override;

    bool DeleteLines(const size_t count) override;
    bool InsertLines(const size_t count) override;

    bool MoveToBottom() const override;

    bool PrivateSetColorTableEntry(const short index, const COLORREF value) const noexcept override;

    bool PrivateSetDefaultForeground(const COLORREF value) const noexcept override;

    bool PrivateSetDefaultBackground(const COLORREF value) const noexcept override;

    bool PrivateFillRegion(const COORD startPosition,
                           const size_t fillLength,
                           const wchar_t fillChar,
                           const bool standardFillAttrs) noexcept override;

    bool PrivateScrollRegion(const SMALL_RECT scrollRect,
                             const std::optional<SMALL_RECT> clipRect,
                             const COORD destinationOrigin,
                             const bool standardFillAttrs) noexcept override;

    bool PrivateIsVtInputEnabled() const override;

private:
    Microsoft::Console::IIoProvider& _io;
};
