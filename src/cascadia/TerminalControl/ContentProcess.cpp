// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ContentProcess.h"
#include "ContentProcess.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    ContentProcess::ContentProcess() {}

    bool ContentProcess::Initialize(Control::IControlSettings settings,
                                    TerminalConnection::ConnectionInformation connectionInfo)
    {
        auto conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectionInfo) };
        if (conn == nullptr)
        {
            return false;
        }
        _interactivity = winrt::make<implementation::ControlInteractivity>(settings, conn);
        return true;
    }

    Control::ControlInteractivity ContentProcess::GetInteractivity()
    {
        return _interactivity;
    }

    uint64_t ContentProcess::RequestSwapChainHandle(const uint64_t pid)
    {
        auto ourPid = GetCurrentProcessId();
        HANDLE ourHandle = reinterpret_cast<HANDLE>(_interactivity.Core().SwapChainHandle());
        if (pid == ourPid)
        {
            return reinterpret_cast<uint64_t>(ourHandle);
        }

        wil::unique_handle hWindowProcess{ OpenProcess(PROCESS_ALL_ACCESS,
                                                       FALSE,
                                                       static_cast<DWORD>(pid)) };
        if (hWindowProcess.get() == nullptr)
        {
            const auto gle = GetLastError();
            gle;
            // TODO! tracelog an error here
            return 0;
        }

        HANDLE theirHandle{ nullptr };
        BOOL success = DuplicateHandle(GetCurrentProcess(),
                                       ourHandle,
                                       hWindowProcess.get(),
                                       &theirHandle,
                                       0,
                                       FALSE,
                                       DUPLICATE_SAME_ACCESS);
        if (!success)
        {
            const auto gle = GetLastError();
            gle;
            // TODO! tracelog an error here
            return 0;
        }
        return reinterpret_cast<uint64_t>(theirHandle);
    }

}
