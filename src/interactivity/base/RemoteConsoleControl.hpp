/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RemoteConsoleControl.hpp

Abstract:
- This module is used for remoting console control calls to a different host owner process.

Author(s):
- Michael Niksa (MiNiksa) 10-Jun-2021
--*/
#pragma once

#include "../inc/IConsoleControl.hpp"
#include "../win32/ConsoleControl.hpp"

namespace Microsoft::Console::Interactivity
{
    class RemoteConsoleControl final : public IConsoleControl
    {
    public:
        RemoteConsoleControl(HANDLE signalPipe);

        // IConsoleControl Members
        [[nodiscard]] NTSTATUS NotifyConsoleApplication(_In_ DWORD dwProcessId);
        [[nodiscard]] NTSTATUS SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground);
        [[nodiscard]] NTSTATUS EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags);
        [[nodiscard]] NTSTATUS SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId);

    private:
        wil::unique_handle _pipe;

        Win32::ConsoleControl _control;
    };
}
