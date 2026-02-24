/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ProcessList.h

Abstract:
- This file defines a list of process handles maintained by an instance of a console server

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in handle.h
--*/

#pragma once

#include "ProcessHandle.h"

// this structure is used to store relevant information from the console for ctrl processing so we can do it without
// holding the console lock.
struct ConsoleProcessTerminationRecord
{
    // Unfortunately the reason for this was lost in time, but presumably a process
    // handle is held so that we can refer to a process via PID (dwProcessID) without
    // holding the console lock and fearing that the PID might get reused by the OS.
    wil::unique_handle hProcess;
    DWORD dwProcessID;
    ULONG ulTerminateCount;
};

class ConsoleProcessList
{
public:
    [[nodiscard]] HRESULT AllocProcessData(const DWORD dwProcessId,
                                           const DWORD dwThreadId,
                                           const ULONG ulProcessGroupId,
                                           _Outptr_opt_ ConsoleProcessHandle** const ppProcessData);

    void FreeProcessData(_In_ ConsoleProcessHandle* const ProcessData);

    ConsoleProcessHandle* FindProcessInList(const DWORD dwProcessId) const;
    ConsoleProcessHandle* FindProcessByGroupId(_In_ ULONG ulProcessGroupId) const;
    ConsoleProcessHandle* GetRootProcess() const;
    ConsoleProcessHandle* GetOldestProcess() const;

    [[nodiscard]] HRESULT GetTerminationRecordsByGroupId(const DWORD dwLimitingProcessId,
                                                         const bool fCtrlClose,
                                                         std::vector<ConsoleProcessTerminationRecord>& termRecords) const;

    [[nodiscard]] HRESULT GetProcessList(_Inout_updates_(*pcProcessList) DWORD* const pProcessList,
                                         _Inout_ size_t* const pcProcessList) const;

    void ModifyConsoleProcessFocus(const bool fForeground);

    bool IsEmpty() const;

private:
    std::vector<ConsoleProcessHandle*> _processes;

    void _ModifyProcessForegroundRights(const HANDLE hProcess, const bool fForeground) const;
};
