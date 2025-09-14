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
        void NotifyWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) noexcept override;
        void NotifyConsoleApplication(_In_ DWORD dwProcessId) noexcept override;
        void SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground) noexcept override;
        void EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) noexcept override;
        void SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept override;
    };
}
