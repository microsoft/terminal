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
    HANDLE hProcess;
    DWORD dwProcessID;
    ULONG ulTerminateCount;
};

class ConsoleProcessList
{
public:
    static const DWORD ROOT_PROCESS_ID = 0;

    [[nodiscard]] HRESULT AllocProcessData(const DWORD dwProcessId,
                                           const DWORD dwThreadId,
                                           const ULONG ulProcessGroupId,
                                           _In_opt_ ConsoleProcessHandle* const pParentProcessData,
                                           _Outptr_opt_ ConsoleProcessHandle** const ppProcessData);

    void FreeProcessData(_In_ ConsoleProcessHandle* const ProcessData);

    ConsoleProcessHandle* FindProcessInList(const DWORD dwProcessId) const;
    ConsoleProcessHandle* FindProcessByGroupId(_In_ ULONG ulProcessGroupId) const;

    [[nodiscard]] HRESULT GetTerminationRecordsByGroupId(const DWORD dwLimitingProcessId,
                                                         const bool fCtrlClose,
                                                         _Outptr_result_buffer_all_(*pcRecords) ConsoleProcessTerminationRecord** prgRecords,
                                                         _Out_ size_t* const pcRecords) const;

    ConsoleProcessHandle* GetFirstProcess() const;

    [[nodiscard]] HRESULT GetProcessList(_Inout_updates_(*pcProcessList) DWORD* const pProcessList,
                                         _Inout_ size_t* const pcProcessList) const;

    void ModifyConsoleProcessFocus(const bool fForeground);

    bool IsEmpty() const;

private:
    std::list<ConsoleProcessHandle*> _processes;

    void _ModifyProcessForegroundRights(const HANDLE hProcess, const bool fForeground) const;
};
