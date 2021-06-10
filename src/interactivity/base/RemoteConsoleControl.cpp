// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "RemoteConsoleControl.hpp"

#include "../../inc/HostSignals.hpp"

using namespace Microsoft::Console::Interactivity;

RemoteConsoleControl::RemoteConsoleControl(HANDLE signalPipe) :
    _pipe{ signalPipe }
{

}

#pragma region IConsoleControl Members

[[nodiscard]] NTSTATUS RemoteConsoleControl::NotifyConsoleApplication(_In_ DWORD dwProcessId)
{
    DWORD dwWritten = 0;

    unsigned short code = HOST_SIGNAL_NOTIFY_APP;
    if (!WriteFile(_pipe.get(), &code, sizeof(code), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    HOST_SIGNAL_NOTIFY_APP_DATA data;
    data.cbSize = sizeof(data);
    data.dwProcessId = dwProcessId;

    if (!WriteFile(_pipe.get(), &data, sizeof(data), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground)
{
    DWORD dwWritten = 0;

    unsigned short code = HOST_SIGNAL_SET_FOREGROUND;
    if (!WriteFile(_pipe.get(), &code, sizeof(code), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    HOST_SIGNAL_SET_FOREGROUND_DATA data;
    data.cbSize = sizeof(data);
    data.dwProcessId = HandleToULong(hProcess);
    data.fForeground = fForeground;

    if (!WriteFile(_pipe.get(), &data, sizeof(data), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    DWORD dwWritten = 0;

    unsigned short code = HOST_SIGNAL_END_TASK;
    if (!WriteFile(_pipe.get(), &code, sizeof(code), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    HOST_SIGNAL_END_TASK_DATA data;
    data.cbSize = sizeof(data);
    data.dwProcessId = HandleToULong(hProcessId);
    data.dwEventType = dwEventType;
    data.ulCtrlFlags = ulCtrlFlags;

    if (!WriteFile(_pipe.get(), &data, sizeof(data), &dwWritten, nullptr))
    {
        return NTSTATUS_FROM_WIN32(::GetLastError());
    }

    return STATUS_SUCCESS;
}

#pragma endregion
