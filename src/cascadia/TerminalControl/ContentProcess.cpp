// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ContentProcess.h"
#include "ContentProcess.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    ContentProcess::ContentProcess() :
        _ourPID{ GetCurrentProcessId() } {}

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

    ContentProcess::~ContentProcess()
    {
        _DestructedHandlers();
    }

    Control::ControlInteractivity ContentProcess::GetInteractivity()
    {
        return _interactivity;
    }

    uint64_t ContentProcess::GetPID()
    {
        return _ourPID;
    }

    // Method Description:
    // - Duplicate the swap chain handle to the provided process.
    // - If the provided PID is our pid, then great - we don't need to do anything.
    // Arguments:
    // - callersPid: the PID of the process calling this method.
    // Return Value:
    // - The value of the swapchain handle in the callers process
    // Notes:
    // - This is BODGY! We're basically asking to marshal a HANDLE here. WinRT
    //   has no good mechanism for doing this, so we're doing it by casting the
    //   value to a uint64_t. In all reality, we _should_ be using a COM
    //   interface for this, because it can set up the security on these handles
    //   more appropriately. Fortunately, all we're dealing with is swapchains,
    //   so the security doesn't matter all that much.
    uint64_t ContentProcess::RequestSwapChainHandle(const uint64_t callersPid)
    {
        auto ourPid = GetCurrentProcessId();
        HANDLE ourHandle = reinterpret_cast<HANDLE>(_interactivity.Core().SwapChainHandle());
        if (callersPid == ourPid)
        {
            return reinterpret_cast<uint64_t>(ourHandle);
        }

        wil::unique_handle hWindowProcess{ OpenProcess(PROCESS_ALL_ACCESS,
                                                       FALSE,
                                                       static_cast<DWORD>(callersPid)) };
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

        // At this point, the handle is now in their process space, with value
        // theirHandle
        return reinterpret_cast<uint64_t>(theirHandle);
    }

}
