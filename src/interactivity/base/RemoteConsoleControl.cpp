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

void RemoteConsoleControl::Control(ControlType, PVOID, DWORD) noexcept
{
    WI_ASSERT_FAIL();
}

void RemoteConsoleControl::NotifyWinEvent(DWORD, HWND, LONG, LONG) noexcept
{
    WI_ASSERT_FAIL();
}

#pragma region IConsoleControl Members

template<typename T>
void _SendTypedPacket(HANDLE pipe, ::Microsoft::Console::HostSignals signalCode, T& payload) noexcept
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
    LOG_IF_WIN32_BOOL_FALSE(WriteFile(pipe, &packet, sizeof(packet), &bytesWritten, nullptr));
}

void RemoteConsoleControl::NotifyConsoleApplication(_In_ DWORD dwProcessId) noexcept
{
    HostSignalNotifyAppData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = dwProcessId;

    return _SendTypedPacket(_pipe.get(), HostSignals::NotifyApp, data);
}

void RemoteConsoleControl::SetForeground(_In_ HANDLE hProcess, _In_ BOOL fForeground) noexcept
{
    // GH#13211 - Apparently this API doesn't need to be forwarded to conhost at
    // all. Instead, just perform the ConsoleControl operation here, in proc.
    // This lets us avoid all sorts of strange handle duplicating weirdness.
    _control.SetForeground(hProcess, fForeground);
}

void RemoteConsoleControl::EndTask(_In_ DWORD dwProcessId, _In_ DWORD dwEventType, _In_ ULONG ulCtrlFlags) noexcept
{
    HostSignalEndTaskData data{};
    data.sizeInBytes = sizeof(data);
    data.processId = dwProcessId;
    data.eventType = dwEventType;
    data.ctrlFlags = ulCtrlFlags;

    return _SendTypedPacket(_pipe.get(), HostSignals::EndTask, data);
}

void RemoteConsoleControl::SetWindowOwner(HWND hwnd, DWORD processId, DWORD threadId) noexcept
{
    // This call doesn't need to get forwarded to the root conhost. Just handle
    // it in-proc, to set the owner of OpenConsole
    _control.SetWindowOwner(hwnd, processId, threadId);
}

#pragma endregion
