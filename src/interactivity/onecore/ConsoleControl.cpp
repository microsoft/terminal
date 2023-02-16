// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleControl.hpp"

#include <ntcsrdll.h>
#include <csrmsg.h>

using namespace Microsoft::Console::Interactivity::OneCore;

#pragma region IConsoleControl Members

[[nodiscard]] NTSTATUS ConsoleControl::NotifyConsoleApplication(_In_ DWORD /*dwProcessId*/) noexcept
{
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ConsoleControl::SetForeground(_In_ HANDLE /*hProcess*/, _In_ BOOL /*fForeground*/) noexcept
{
    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    USER_API_MSG m{};
    const auto a = &m.u.EndTask;

    RtlZeroMemory(a, sizeof(*a));
    a->ProcessId = ULongToHandle(dwProcessId); // This is actually a PID, even though the struct expects a HANDLE.
    a->ConsoleEventCode = dwEventType;
    a->ConsoleFlags = ulCtrlFlags;

    return CsrClientCallServer(reinterpret_cast<PCSR_API_MSG>(&m),
                               nullptr,
                               CSR_MAKE_API_NUMBER(USERSRV_SERVERDLL_INDEX, UserpEndTask),
                               sizeof(*a));
}

[[nodiscard]] NTSTATUS ConsoleControl::SetWindowOwner(HWND, DWORD, DWORD) noexcept
{
    return STATUS_SUCCESS;
}

#pragma endregion
