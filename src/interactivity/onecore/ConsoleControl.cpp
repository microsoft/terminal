// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleControl.hpp"

#include <ntcsrdll.h>
#include <csrmsg.h>

using namespace Microsoft::Console::Interactivity::OneCore;

#pragma region IConsoleControl Members

[[nodiscard]] NTSTATUS ConsoleControl::NotifyConsoleApplication(_In_ DWORD /*dwProcessId*/)
{
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ConsoleControl::SetForeground(_In_ HANDLE /*hProcess*/, _In_ BOOL /*fForeground*/)
{
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ConsoleControl::EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    USER_API_MSG m;
    PENDTASKMSG a = &m.u.EndTask;

    RtlZeroMemory(a, sizeof(*a));
    a->ProcessId = hProcessId;
    a->ConsoleEventCode = dwEventType;
    a->ConsoleFlags = ulCtrlFlags;

    return CsrClientCallServer((PCSR_API_MSG)&m,
                               NULL,
                               CSR_MAKE_API_NUMBER(USERSRV_SERVERDLL_INDEX, UserpEndTask),
                               sizeof(*a));
}

#pragma endregion
