/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IConsoleControl.hpp

Abstract:
- Defines methods that delegate the execution of privileged operations or notify
  Windows subsystems about console state.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

namespace Microsoft::Console::Interactivity
{
    enum class ControlType
    {
        ConsoleSetVDMCursorBounds,
        ConsoleNotifyConsoleApplication,
        ConsoleFullscreenSwitch,
        ConsoleSetCaretInfo,
        ConsoleSetReserveKeys,
        ConsoleSetForeground,
        ConsoleSetWindowOwner,
        ConsoleEndTask,
    };

    class IConsoleControl
    {
    public:
        virtual ~IConsoleControl() = default;
        virtual void Control(ControlType command, PVOID ptr, DWORD len) noexcept = 0;
        virtual void NotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) noexcept = 0;
        virtual void NotifyConsoleApplication(DWORD dwProcessId) noexcept = 0;
        virtual void SetForeground(HANDLE hProcess, BOOL fForeground) noexcept = 0;
        virtual void EndTask(DWORD dwProcessId, DWORD dwEventType, ULONG ulCtrlFlags) noexcept = 0;
        virtual void SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept = 0;
    };
}
