/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ConsoleControl.hpp

Abstract:
- OneCore implementation of the IConsoleControl interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "../inc/IConsoleControl.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class ConsoleControl : public IConsoleControl
    {
    public:
        // IConsoleControl Members
        [[nodiscard]] NTSTATUS NotifyConsoleApplication(_In_ DWORD dwProcessId) noexcept override;
        [[nodiscard]] NTSTATUS SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground) noexcept override;
        [[nodiscard]] NTSTATUS EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) override;
        [[nodiscard]] NTSTATUS SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept override;
    };
}
