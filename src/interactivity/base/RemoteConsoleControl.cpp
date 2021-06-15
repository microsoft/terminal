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

template<typename T>
[[nodiscard]] NTSTATUS _SendTypedPacket(HANDLE pipe, ::Microsoft::Console::HostSignals signalCode, T& payload)
{
    // To ensure it's a happy wire format, pack it tight at 1.
#pragma pack(push, 1)
    struct HostSignalPacket
    {
        ::Microsoft::Console::HostSignals code;
        T data;
    };
#pragma pack(pop)

    HostSignalPacket packet;
    packet.code = signalCode;
    packet.data = payload;

    DWORD bytesWritten = 0;
    if (!WriteFile(pipe, &packet, sizeof(packet), &bytesWritten, nullptr))
    {
        NT_RETURN_NTSTATUS(static_cast<NTSTATUS>(NTSTATUS_FROM_WIN32(::GetLastError())));
    }

    if (bytesWritten != sizeof(packet))
    {
        NT_RETURN_NTSTATUS(static_cast<NTSTATUS>(NTSTATUS_FROM_WIN32(E_UNEXPECTED)));
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::NotifyConsoleApplication(_In_ DWORD dwProcessId)
{
    HostSignalNotifyAppData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = dwProcessId;

    return _SendTypedPacket(_pipe.get(), HostSignals::NotifyApp, data);
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground)
{
    HostSignalSetForegroundData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = HandleToULong(hProcess);
    data.isForeground = fForeground;

    return _SendTypedPacket(_pipe.get(), HostSignals::SetForeground, data);
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::EndTask(_In_ HANDLE hProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    HostSignalEndTaskData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = HandleToULong(hProcessId);
    data.eventType = dwEventType;
    data.ctrlFlags = ulCtrlFlags;

    return _SendTypedPacket(_pipe.get(), HostSignals::EndTask, data);
}

#pragma endregion
