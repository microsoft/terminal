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

#include "../terminal/adapter/ITerminalApi.hpp"
#include "../types/inc/IInputEvent.hpp"
#include "../inc/conattrs.hpp"
#include "IIoProvider.hpp"

// The ConhostInternalGetSet is for the Conhost process to call the entrypoints for its own Get/Set APIs.
// Normally, these APIs are accessible from the outside of the conhost process (like by the process being "hosted") through
// the kernelbase/32 exposed public APIs and routed by the console driver (condrv) to this console host.
// But since we're trying to call them from *inside* the console host itself, we need to get in the way and route them straight to the
// v-table inside this process instance.
class ConhostInternalGetSet final : public Microsoft::Console::VirtualTerminal::ITerminalApi
{
public:
    ConhostInternalGetSet(_In_ Microsoft::Console::IIoProvider& io);

    void ReturnResponse(const std::wstring_view response) override;

    Microsoft::Console::VirtualTerminal::StateMachine& GetStateMachine() override;
    TextBuffer& GetTextBuffer() override;
    til::rect GetViewport() const override;
    void SetViewportPosition(const til::point position) override;

    void SetTextAttributes(const TextAttribute& attrs) override;

    void SetSystemMode(const Mode mode, const bool enabled) override;
    bool GetSystemMode(const Mode mode) const override;

    void WarningBell() override;

    void SetWindowTitle(const std::wstring_view title) override;

    void UseAlternateScreenBuffer(const TextAttribute& attrs) override;

    void UseMainScreenBuffer() override;

    CursorType GetUserDefaultCursorStyle() const override;

    void ShowWindow(bool showOrHide) override;

    bool ResizeWindow(const til::CoordType width, const til::CoordType height) override;

    void SetConsoleOutputCP(const unsigned int codepage) override;
    unsigned int GetConsoleOutputCP() const override;

    void CopyToClipboard(const std::wstring_view content) override;
    void SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress) override;
    void SetWorkingDirectory(const std::wstring_view uri) override;
    void PlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration) override;

    bool IsConsolePty() const override;
    bool IsVtInputEnabled() const override;

    void NotifyAccessibilityChange(const til::rect& changedRect) override;
    void NotifyBufferRotation(const int delta) override;

    void MarkPrompt(const ScrollMark& mark) override;
    void MarkCommandStart() override;
    void MarkOutputStart() override;
    void MarkCommandFinish(std::optional<unsigned int> error) override;

    void InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength) override;

private:
    Microsoft::Console::IIoProvider& _io;
};
