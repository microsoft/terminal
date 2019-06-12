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

#include "..\inc\IConsoleControl.hpp"

#pragma hdrstop

namespace Microsoft::Console::Interactivity::OneCore
{
    class ConsoleControl sealed : public IConsoleControl
    {
    public:
        // IConsoleControl Members
        [[nodiscard]] NTSTATUS NotifyConsoleApplication(_In_ DWORD dwProcessId);
        [[nodiscard]] NTSTATUS SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground);
        [[nodiscard]] NTSTATUS EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags);
    };
}
