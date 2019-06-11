/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ProcessHandle.h

Abstract:
- This file defines the handles that were given to a particular client process ID when it connected.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

#include "ObjectHandle.h"
#include "WaitQueue.h"
#include "ProcessPolicy.h"

#include <memory>
#include <wil\resource.h>

class ConsoleProcessHandle
{
public:
    std::unique_ptr<ConsoleWaitQueue> const pWaitBlockQueue;
    std::unique_ptr<ConsoleHandleData> pInputHandle;
    std::unique_ptr<ConsoleHandleData> pOutputHandle;

    bool fRootProcess;

    DWORD const dwProcessId;
    DWORD const dwThreadId;

    const ConsoleProcessPolicy GetPolicy() const;

    CD_CONNECTION_INFORMATION GetConnectionInformation() const;

private:
    ConsoleProcessHandle(const DWORD dwProcessId,
                         const DWORD dwThreadId,
                         const ULONG ulProcessGroupId);
    ~ConsoleProcessHandle() = default;
    ConsoleProcessHandle(const ConsoleProcessHandle&) = delete;
    ConsoleProcessHandle(ConsoleProcessHandle&&) = delete;
    ConsoleProcessHandle& operator=(const ConsoleProcessHandle&) & = delete;
    ConsoleProcessHandle& operator=(ConsoleProcessHandle&&) & = delete;

    ULONG _ulTerminateCount;
    ULONG const _ulProcessGroupId;
    wil::unique_handle const _hProcess;

    const ConsoleProcessPolicy _policy;

    friend class ConsoleProcessList; // ensure List manages lifetimes and not other classes.
};
