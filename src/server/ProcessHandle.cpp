// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ProcessHandle.h"

#include "../host/globals.h"
#include "../host/telemetry.hpp"

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
    _shimPolicy(_hProcess.get()),
    _processCreationTime{}
{
    if (nullptr != _hProcess.get())
    {
        Telemetry::Instance().LogProcessConnected(_hProcess.get());
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
const FILETIME ConsoleProcessHandle::GetProcessCreationTime() const
{
    if (_processCreationTime.dwHighDateTime == 0 && _processCreationTime.dwLowDateTime == 0 && _hProcess != nullptr)
    {
        FILETIME ftDummyTime = { 0 };

        ::GetProcessTimes(_hProcess.get(),
                          &_processCreationTime,
                          &ftDummyTime,
                          &ftDummyTime,
                          &ftDummyTime);
    }

    return _processCreationTime;
}
