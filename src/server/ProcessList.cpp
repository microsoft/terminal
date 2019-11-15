// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ProcessList.h"

#include "..\host\conwinuserrefs.h"
#include "..\host\globals.h"
#include "..\host\telemetry.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;

// Routine Description:
// - Allocates and stores in a list the process information given.
// - Will not create a new entry in the list given information matches a known process. Will instead return existing entry.
// Arguments:
// - dwProcessId - Process ID of connecting client
// - dwThreadId - Thread ID of connecting client
// - ulProcessGroupId - Process Group ID from connecting client (sometimes referred to as parent)
// - pParentProcessData - Used to specify parent while locating appropriate scope of sending control messages
// - ppProcessData - Filled on exit with a pointer to the process handle information. Optional.
//                 - If not used, return code will specify whether this process is known to the list or not.
// Return Value:
// - S_OK if the process was recorded in the list successfully or already existed.
// - E_FAIL if we're running into an LPC port conflict by nature of the process chain.
// - E_OUTOFMEMORY if there wasn't space to allocate a handle or push it into the list.
[[nodiscard]] HRESULT ConsoleProcessList::AllocProcessData(const DWORD dwProcessId,
                                                           const DWORD dwThreadId,
                                                           const ULONG ulProcessGroupId,
                                                           _In_opt_ ConsoleProcessHandle* const pParentProcessData,
                                                           _Outptr_opt_ ConsoleProcessHandle** const ppProcessData)
{
    FAIL_FAST_IF(!(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked()));

    ConsoleProcessHandle* pProcessData = FindProcessInList(dwProcessId);
    if (nullptr != pProcessData)
    {
        // In the GenerateConsoleCtrlEvent it's OK for this process to already have a ProcessData object. However, the other case is someone
        // connecting to our LPC port and they should only do that once, so we fail subsequent connection attempts.
        if (nullptr == pParentProcessData)
        {
            return E_FAIL;
            // TODO: MSFT: 9574803 - This fires all the time. Did it always do that?
            //RETURN_HR(E_FAIL);
        }
        else
        {
            if (nullptr != ppProcessData)
            {
                *ppProcessData = pProcessData;
            }
            RETURN_HR(S_OK);
        }
    }

    try
    {
        pProcessData = new ConsoleProcessHandle(dwProcessId,
                                                dwThreadId,
                                                ulProcessGroupId);

        // Some applications, when reading the process list through the GetConsoleProcessList API, are expecting
        // the returned list of attached process IDs to be from newest to oldest.
        // As such, we have to put the newest process into the head of the list.
        _processes.push_front(pProcessData);

        if (nullptr != ppProcessData)
        {
            *ppProcessData = pProcessData;
        }
    }
    CATCH_RETURN();

    RETURN_HR(S_OK);
}

// Routine Description:
// - This routine frees any per-process data allocated by the console.
// Arguments:
// - pProcessData - Pointer to the per-process data structure.
// Return Value:
// - <none>
void ConsoleProcessList::FreeProcessData(_In_ ConsoleProcessHandle* const pProcessData)
{
    FAIL_FAST_IF(!(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked()));

    // Assert that the item exists in the list. If it doesn't exist, the end/last will be returned.
    FAIL_FAST_IF(!(_processes.cend() != std::find(_processes.cbegin(), _processes.cend(), pProcessData)));

    _processes.remove(pProcessData);

    delete pProcessData;
}

// Routine Description:
// - Locates a process handle in this list.
// - NOTE: Calling FindProcessInList(0) means you want the root process.
// Arguments:
// - dwProcessId - ID of the process to search for or ROOT_PROCESS_ID to find the root process.
// Return Value:
// - Pointer to the process handle information or nullptr if no match was found.
ConsoleProcessHandle* ConsoleProcessList::FindProcessInList(const DWORD dwProcessId) const
{
    auto it = _processes.cbegin();

    while (it != _processes.cend())
    {
        ConsoleProcessHandle* const pProcessHandleRecord = *it;

        if (ROOT_PROCESS_ID != dwProcessId)
        {
            if (pProcessHandleRecord->dwProcessId == dwProcessId)
            {
                return pProcessHandleRecord;
            }
        }
        else
        {
            if (pProcessHandleRecord->fRootProcess)
            {
                return pProcessHandleRecord;
            }
        }

        it = std::next(it);
    }

    return nullptr;
}

// Routine Description:
// - Locates a process handle by the group ID reference.
// Arguments:
// - ulProcessGroupId - Group to search for in the list
// Return Value:
// - Pointer to first matching process handle with given group ID. nullptr if no match was found.
ConsoleProcessHandle* ConsoleProcessList::FindProcessByGroupId(_In_ ULONG ulProcessGroupId) const
{
    auto it = _processes.cbegin();

    while (it != _processes.cend())
    {
        ConsoleProcessHandle* const pProcessHandleRecord = *it;
        if (pProcessHandleRecord->_ulProcessGroupId == ulProcessGroupId)
        {
            return pProcessHandleRecord;
        }

        it = std::next(it);
    }

    return nullptr;
}

// Routine Description:
// - Retrieves the entire list of process IDs that is known to this list.
// - Requires caller to allocate space. If not enough space, pcProcessList will be filled with count of array necessary.
// Arguments:
// - pProcessList - Pointer to buffer to store process IDs. Caller allocated.
// - pcProcessList - On the way in, the length of the buffer given. On the way out, the amount of the buffer used.
//                 - If buffer was insufficient, filled with necessary buffer size (in elements) on the way out.
// Return Value:
// - S_OK if buffer was filled successfully and resulting count of items is in pcProcessList.
// - E_NOT_SUFFICIENT_BUFFER if the buffer given was too small. Refer to pcProcessList for size requirement.
[[nodiscard]] HRESULT ConsoleProcessList::GetProcessList(_Inout_updates_(*pcProcessList) DWORD* const pProcessList,
                                                         _Inout_ size_t* const pcProcessList) const
{
    HRESULT hr = S_OK;

    size_t const cProcesses = _processes.size();

    // If we can fit inside the given list space, copy out the data.
    if (cProcesses <= *pcProcessList)
    {
        size_t cFilled = 0;

        // Loop over the list of processes and fill in the caller's buffer.
        auto it = _processes.cbegin();
        while (it != _processes.cend() && cFilled < *pcProcessList)
        {
            pProcessList[cFilled] = (*it)->dwProcessId;
            cFilled++;
            it = std::next(it);
        }
    }
    else
    {
        hr = E_NOT_SUFFICIENT_BUFFER;
    }

    // Return how many items were copied (or how many values we would need to fit).
    *pcProcessList = cProcesses;

    return hr;
}

// Routine Description
// - Retrieves TERMINATION_RECORD structures for all processes known in the list (limited if necessary by parameter for group ID)
// - This is designed to copy the data so the global lock can be released while sending control information to attached processes.
// Arguments:
// - dwLimitingProcessId - Optional (0 if unused). Will restrict the return to only processes containing this group ID.
// - fCtrlClose - True if we're about to send a Ctrl Close command to the process. Will increment termination attempt count.
// - prgRecords - Pointer to callee allocated array of termination records. CALLER MUST FREE.
// - pcRecords - Length of records in prgRecords.
// Return Value:
// - S_OK if prgRecords was filled successfully or if no records were found that matched.
// - E_OUTOFMEMORY in a low memory situation.
[[nodiscard]] HRESULT ConsoleProcessList::GetTerminationRecordsByGroupId(const DWORD dwLimitingProcessId,
                                                                         const bool fCtrlClose,
                                                                         _Outptr_result_buffer_all_(*pcRecords) ConsoleProcessTerminationRecord** prgRecords,
                                                                         _Out_ size_t* const pcRecords) const
{
    *pcRecords = 0;

    try
    {
        std::deque<std::unique_ptr<ConsoleProcessTerminationRecord>> TermRecords;

        // Dig through known processes looking for a match
        auto it = _processes.cbegin();
        while (it != _processes.cend())
        {
            ConsoleProcessHandle* const pProcessHandleRecord = *it;

            // If no limit was specified OR if we have a match, generate a new termination record.
            if (0 == dwLimitingProcessId ||
                pProcessHandleRecord->_ulProcessGroupId == dwLimitingProcessId)
            {
                std::unique_ptr<ConsoleProcessTerminationRecord> pNewRecord = std::make_unique<ConsoleProcessTerminationRecord>();

                // If the duplicate failed, the best we can do is to skip including the process in the list and hope it goes away.
                LOG_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                                        pProcessHandleRecord->_hProcess.get(),
                                                        GetCurrentProcess(),
                                                        &pNewRecord->hProcess,
                                                        0,
                                                        0,
                                                        DUPLICATE_SAME_ACCESS));

                pNewRecord->dwProcessID = pProcessHandleRecord->dwProcessId;

                // If we're hard closing the window, increment the counter.
                if (fCtrlClose)
                {
                    pProcessHandleRecord->_ulTerminateCount++;
                }

                pNewRecord->ulTerminateCount = pProcessHandleRecord->_ulTerminateCount;

                TermRecords.push_back(std::move(pNewRecord));
            }

            it = std::next(it);
        }

        // From all found matches, convert to C-style array to return
        size_t const cchRetVal = TermRecords.size();
        ConsoleProcessTerminationRecord* pRetVal = new ConsoleProcessTerminationRecord[cchRetVal];

        for (size_t i = 0; i < cchRetVal; i++)
        {
            pRetVal[i] = *TermRecords.at(i);
        }

        *prgRecords = pRetVal;
        *pcRecords = cchRetVal;
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Gets the first process in the list.
// - Used for reassigning a new root process.
// TODO: MSFT 9450737 - encapsulate root process logic. https://osgvsowi/9450737
// Arguments:
// - <none>
// Return Value:
// - Pointer to the first item in the list or nullptr if there are no items.
ConsoleProcessHandle* ConsoleProcessList::GetFirstProcess() const
{
    if (!_processes.empty())
    {
        return _processes.front();
    }

    return nullptr;
}

// Routine Description:
// - Requests that the OS change the process priority for the console and all attached client processes
// Arguments:
// - fForeground - True if console is in foreground and related processes should be prioritied. False if they can be backgrounded/deprioritized.
// Return Value:
// - <none>
// - NOTE: Will attempt to request a change, but it's non fatal if it doesn't work. Failures will be logged to debug channel.
void ConsoleProcessList::ModifyConsoleProcessFocus(const bool fForeground)
{
    auto it = _processes.cbegin();
    while (it != _processes.cend())
    {
        ConsoleProcessHandle* const pProcessHandle = *it;

        if (pProcessHandle->_hProcess != nullptr)
        {
            _ModifyProcessForegroundRights(pProcessHandle->_hProcess.get(), fForeground);
        }

        it = std::next(it);
    }

    // Do this for conhost.exe itself, too.
    _ModifyProcessForegroundRights(GetCurrentProcess(), fForeground);
}

// Routine Description:
// - Specifies that there are no remaining processes
// TODO: This should not be exposed, most likely. Whomever is calling it should join this class.
// Arguments:
// - <none>
// Return Value:
// - True if the list is empty. False if we have known processes.
bool ConsoleProcessList::IsEmpty() const
{
    return _processes.empty();
}

// Routine Description:
// - Requests the OS allow the console to set one of its child processes as the foreground window
// Arguments:
// - hProcess - Handle to the process to modify
// - fForeground - True if we're allowed to set it as the foreground window. False otherwise.
// Return Value:
// - <none>
void ConsoleProcessList::_ModifyProcessForegroundRights(const HANDLE hProcess, const bool fForeground) const
{
    LOG_IF_NTSTATUS_FAILED(ServiceLocator::LocateConsoleControl()->SetForeground(hProcess, fForeground));
}
