// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ProcessHandle.h"

#include "../host/globals.h"
#include "../host/telemetry.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;
// Routine Description:
// - Constructs an instance of the ConsoleProcessHandle Class
// - NOTE: Can throw if allocation fails or if there is a console policy we do not understand.
// - NOTE: Not being able to open the process by ID isn't a failure. It will be logged and continued.
ConsoleProcessHandle::ConsoleProcessHandle(const DWORD dwProcessId,
                                           const DWORD dwThreadId,
                                           const ULONG ulProcessGroupId) :
    pWaitBlockQueue(std::make_unique<ConsoleWaitQueue>()),
    pInputHandle(nullptr),
    pOutputHandle(nullptr),
    fRootProcess(false),
    dwProcessId(dwProcessId),
    dwThreadId(dwThreadId),
    _ulTerminateCount(0),
    _ulProcessGroupId(ulProcessGroupId),
    _hProcess(LOG_LAST_ERROR_IF_NULL(OpenProcess(MAXIMUM_ALLOWED,
                                                 FALSE,
                                                 dwProcessId))),
    _policy(ConsoleProcessPolicy::s_CreateInstance(_hProcess.get())),
    _shimPolicy(ConsoleShimPolicy::s_CreateInstance(_hProcess.get())),
    _processCreationTime(0)
{
    if (nullptr != _hProcess.get())
    {
        Telemetry::Instance().LogProcessConnected(_hProcess.get());
    }

    // GH#13211 - If we're running as the delegation console (someone handed off
    // to us), then we need to make sure the original conhost has access to this
    // process handle as well. Otherwise, the future calls to
    // ConsoleControl(SetForeground,...) won't work, because the literal handle
    // value doesn't exist in the original conhost process space.
    // * g.handoffInboxConsoleHandle is only set when we've been delegated to
    // * We can't just pass something like the PID, because the OS conhost
    //   already expects a literal handle value via the HostSignalInputThread.
    //   If we changed that in the OpenConsole version, then there'll surely be
    //   the chance for a mispatch between the OS conhost and the delegated
    //   console.
    if (const auto& conhost{ ServiceLocator::LocateGlobals().handoffInboxConsoleHandle })
    {
        LOG_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                                _hProcess.get(),
                                                conhost.get(),
                                                _hProcessInConhost.put(),
                                                0 /*dwDesiredAccess, ignored*/,
                                                false,
                                                DUPLICATE_SAME_ACCESS));
    }
}

ConsoleProcessHandle::~ConsoleProcessHandle()
{
    // Close out the handle in the origin conhost.
    const auto& conhost{ ServiceLocator::LocateGlobals().handoffInboxConsoleHandle };
    if (_hProcessInConhost && conhost)
    {
        LOG_IF_WIN32_BOOL_FALSE(DuplicateHandle(conhost.get(),
                                                _hProcessInConhost.get(),
                                                nullptr /* hTargetProcessHandle */,
                                                nullptr /* lpTargetHandle, ignored */,
                                                0 /* dwDesiredAccess, ignored */,
                                                false,
                                                DUPLICATE_CLOSE_SOURCE));
    }
}

// Routine Description:
// - Creates a CD_CONNECTION_INFORMATION (packet) that communicates the
//   process, input and output handles to the driver as transformed by the
//   DeviceComm's handle exchanger.
// Arguments:
// - deviceComm: IDeviceComm implementation with which to exchange handles.
CD_CONNECTION_INFORMATION ConsoleProcessHandle::GetConnectionInformation(IDeviceComm* deviceComm) const
{
    CD_CONNECTION_INFORMATION result = { 0 };
    result.Process = deviceComm->PutHandle(this);
    result.Input = deviceComm->PutHandle(pInputHandle.get());
    result.Output = deviceComm->PutHandle(pOutputHandle.get());
    return result;
}

// Routine Description:
// - Retrieves the policies set on this particular process handle
// - This specifies restrictions that may apply to the calling console client application
const ConsoleProcessPolicy ConsoleProcessHandle::GetPolicy() const
{
    return _policy;
}

// Routine Description:
// - Retrieves the policies set on this particular process handle
// - This specifies compatibility shims that we might need to make for certain applications.
const ConsoleShimPolicy ConsoleProcessHandle::GetShimPolicy() const
{
    return _shimPolicy;
}

// Routine Description:
// - Retrieves the raw process handle
const HANDLE ConsoleProcessHandle::GetRawHandle() const
{
    return _hProcess.get();
}

// Routine Description:
// - Retrieves the process creation time (currently used in telemetry traces)
// - The creation time is lazily populated on first call
const ULONG64 ConsoleProcessHandle::GetProcessCreationTime() const
{
    if (_processCreationTime == 0 && _hProcess != nullptr)
    {
        FILETIME ftCreationTime, ftDummyTime = { 0 };
        ULARGE_INTEGER creationTime = { 0 };

        if (::GetProcessTimes(_hProcess.get(),
                              &ftCreationTime,
                              &ftDummyTime,
                              &ftDummyTime,
                              &ftDummyTime))
        {
            creationTime.HighPart = ftCreationTime.dwHighDateTime;
            creationTime.LowPart = ftCreationTime.dwLowDateTime;
        }

        _processCreationTime = creationTime.QuadPart;
    }

    return _processCreationTime;
}
