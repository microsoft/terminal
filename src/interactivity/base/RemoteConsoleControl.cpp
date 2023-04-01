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
        const auto gle = ::GetLastError();
        NT_RETURN_NTSTATUS(static_cast<NTSTATUS>(NTSTATUS_FROM_WIN32(gle)));
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
    // GH#13211 - Apparently this API doesn't need to be forwarded to conhost at
    // all. Instead, just perform the ConsoleControl operation here, in proc.
    // This lets us avoid all sorts of strange handle duplicating weirdness.
    return _control.SetForeground(hProcess, fForeground);
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags)
{
    HostSignalEndTaskData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = dwProcessId;
    data.eventType = dwEventType;
    data.ctrlFlags = ulCtrlFlags;

    return _SendTypedPacket(_pipe.get(), HostSignals::EndTask, data);
}

[[nodiscard]] NTSTATUS RemoteConsoleControl::SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId)
{
    // This call doesn't need to get forwarded to the root conhost. Just handle
    // it in-proc, to set the owner of OpenConsole
    return _control.SetWindowOwner(hwnd, processId, threadId);
}

#pragma endregion
