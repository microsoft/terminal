// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleControl.hpp"

#include <ntcsrdll.h>
#include <csrmsg.h>

using namespace Microsoft::Console::Interactivity::OneCore;

#pragma region IConsoleControl Members

void ConsoleControl::NotifyWinEvent(DWORD /*event*/, HWND /*hwnd*/, LONG /*idObject*/, LONG /*idChild*/) noexcept
{
}

void ConsoleControl::NotifyConsoleApplication(_In_ DWORD /*dwProcessId*/) noexcept
{
}

void ConsoleControl::SetForeground(_In_ HANDLE /*hProcess*/, _In_ BOOL /*fForeground*/) noexcept
{
}

void ConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) noexcept
{
    USER_API_MSG m{};
    const auto a = &m.u.EndTask;

    RtlZeroMemory(a, sizeof(*a));
    a->ProcessId = ULongToHandle(dwProcessId); // This is actually a PID, even though the struct expects a HANDLE.
    a->ConsoleEventCode = dwEventType;
    a->ConsoleFlags = ulCtrlFlags;

#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
    LOG_IF_FAILED(CsrClientCallServer(reinterpret_cast<PCSR_API_MSG>(&m), nullptr, CSR_MAKE_API_NUMBER(USERSRV_SERVERDLL_INDEX, UserpEndTask), sizeof(*a)));
}

void ConsoleControl::SetWindowOwner(HWND, DWORD, DWORD) noexcept
{
}

#pragma endregion
