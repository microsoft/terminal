// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ProcessList.h"

#include "../host/globals.h"
#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity;

// Routine Description:
// - Allocates and stores the given process information in a list.
// - Will not create a new entry in the list given information matches a known process. Will instead return existing entry.
// Arguments:
// - dwProcessId - Process ID of connecting client
// - dwThreadId - Thread ID of connecting client
// - ulProcessGroupId - Process Group ID from connecting client (sometimes referred to as parent)
// - pParentProcessData - Used to specify parent while locating appropriate scope of sending control messages
// - ppProcessData - Filled on exit with a pointer to the process handle information. Optional.
//                 - If not used, return code will specify whether or not this process is known to the list.
// Return Value:
// - S_OK if the process was recorded in the list successfully or already existed.
// - S_FALSE if we're running into an LPC port conflict by nature of the process chain.
// - E_OUTOFMEMORY if there wasn't space to allocate a handle or push it into the list.
[[nodiscard]] HRESULT ConsoleProcessList::AllocProcessData(const DWORD dwProcessId,
                                                           const DWORD dwThreadId,
                                                           const ULONG ulProcessGroupId,
                                                           _Outptr_opt_ ConsoleProcessHandle** const ppProcessData)
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    if (FindProcessInList(dwProcessId))
    {
        return S_FALSE;
    }

    std::unique_ptr<ConsoleProcessHandle> pProcessData;
    try
    {
        pProcessData = std::make_unique<ConsoleProcessHandle>(dwProcessId, dwThreadId, ulProcessGroupId);
        _processes.emplace_back(pProcessData.get());
    }
    CATCH_RETURN();

    wil::assign_to_opt_param(ppProcessData, pProcessData.release());

    return S_OK;
}

// Routine Description:
// - This routine frees any per-process data allocated by the console.
// Arguments:
// - pProcessData - Pointer to the per-process data structure.
// Return Value:
// - <none>
void ConsoleProcessList::FreeProcessData(_In_ ConsoleProcessHandle* const pProcessData)
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    const auto it = std::ranges::find(_processes, pProcessData);
    if (it != _processes.end())
    {
        _processes.erase(it);
        _PublishForegroundBoostClients();
        LOG_IF_FAILED(_UpdateWindowProcessPolicies());
        delete pProcessData;
    }
    else
    {
        // The pointer not existing in the process list would be similar to a heap corruption,
        // as the only code allowed to allocate a `ConsoleProcessHandle` is us, in AllocProcessData().
        // An assertion here would indicate a double-free or similar.
        assert(false);
    }
}

// Routine Description:
// - Locates a process handle in this list.
// Arguments:
// - dwProcessId - ID of the process to search for.
// Return Value:
// - Pointer to the process handle information or nullptr if no match was found.
ConsoleProcessHandle* ConsoleProcessList::FindProcessInList(const DWORD dwProcessId) const
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    for (const auto& p : _processes)
    {
        if (p->dwProcessId == dwProcessId)
        {
            return p;
        }
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
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    for (const auto& p : _processes)
    {
        if (p->_ulProcessGroupId == ulProcessGroupId)
        {
            return p;
        }
    }

    return nullptr;
}

// Routine Description:
// - Locates the root process handle in this list.
// Return Value:
// - Pointer to the process handle information or nullptr if no match was found.
ConsoleProcessHandle* ConsoleProcessList::GetRootProcess() const
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    for (const auto& p : _processes)
    {
        if (p->fRootProcess)
        {
            return p;
        }
    }

    return nullptr;
}

// Routine Description:
// - Gets the first process in the list.
// - Used for reassigning a new root process.
// TODO: MSFT 9450737 - encapsulate root process logic. https://osgvsowi/9450737
// Arguments:
// - <none>
// Return Value:
// - Pointer to the first item in the list or nullptr if there are no items.
ConsoleProcessHandle* ConsoleProcessList::GetOldestProcess() const
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    if (!_processes.empty())
    {
        return _processes.front();
    }

    return nullptr;
}

// Routine Description:
// - Retrieves the entire list of process IDs that is known to this list.
// Arguments:
// - pProcessList - Pointer to buffer to store process IDs. Caller allocated.
// - pcProcessList - On the way in, the length of the buffer given. On the way out, the amount of the buffer used.
//                 - If buffer was insufficient, filled with necessary buffer size (in elements) on the way out.
// Return Value:
// - S_OK if buffer was filled successfully and resulting count of items is in pcProcessList.
// - E_NOT_SUFFICIENT_BUFFER if the buffer given was too small. Refer to pcProcessList for size requirement.
[[nodiscard]] HRESULT ConsoleProcessList::GetProcessList(_Inout_updates_(*pcProcessList) DWORD* pProcessList,
                                                         _Inout_ size_t* const pcProcessList) const
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    if (*pcProcessList < _processes.size())
    {
        *pcProcessList = _processes.size();
        return E_NOT_SUFFICIENT_BUFFER;
    }

    // Some applications, when reading the process list through the GetConsoleProcessList API,
    // are expecting the returned list of attached process IDs to be from newest to oldest.
    // As such, we have to put the newest process into the head of the list.
    auto it = _processes.crbegin();
    const auto end = _processes.crend();

    for (; it != end; ++it)
    {
        *pProcessList++ = (*it)->dwProcessId;
    }

    *pcProcessList = _processes.size();
    return S_OK;
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
                                                                         _Out_ std::vector<ConsoleProcessTerminationRecord>& termRecords) const
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    try
    {
        termRecords.clear();

        // The caller (ProcessCtrlEvents) expects them in newest-to-oldest order,
        // because that's how Windows has historically always dispatched these events.
        auto it = _processes.crbegin();
        const auto end = _processes.crend();

        // Dig through known processes looking for a match
        for (; it != end; ++it)
        {
            const auto p = *it;
            // If no limit was specified OR if we have a match, generate a new termination record.
            if (!dwLimitingProcessId ||
                p->_ulProcessGroupId == dwLimitingProcessId)
            {
                // If we're hard closing the window, increment the counter.
                if (fCtrlClose)
                {
                    p->_ulTerminateCount++;
                }

                wil::unique_handle process;
                // If the duplicate failed, the best we can do is to skip including the process in the list and hope it goes away.
                LOG_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(),
                                                        p->_hProcess.get(),
                                                        GetCurrentProcess(),
                                                        &process,
                                                        0,
                                                        0,
                                                        DUPLICATE_SAME_ACCESS));

                termRecords.emplace_back(ConsoleProcessTerminationRecord{
                    .hProcess = std::move(process),
                    .dwProcessID = p->dwProcessId,
                    .ulTerminateCount = p->_ulTerminateCount,
                });
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Requests that the OS change the process priority for the console and all attached client processes
// Arguments:
// - fForeground - True if console is in foreground and related processes should be prioritized. False if they can be backgrounded/deprioritized.
// Return Value:
// - <none>
// - NOTE: Will attempt to request a change, but it's non fatal if it doesn't work. Failures will be logged to debug channel.
void ConsoleProcessList::ModifyConsoleProcessFocus(const bool fForeground)
{
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    for (const auto& pProcessHandle : _processes)
    {
        if (pProcessHandle->_hProcess)
        {
            _ModifyProcessForegroundRights(pProcessHandle->_hProcess.get(), fForeground);
        }
    }

    // Do this for conhost.exe itself, too.
    _ModifyProcessForegroundRights(GetCurrentProcess(), fForeground);

    // Connecting clients must be registered with USER before applying window policies.
    _PublishForegroundBoostClients();
    LOG_IF_FAILED(_UpdateWindowProcessPolicies());
}

HRESULT ConsoleProcessList::ConfigureWindowProcessPolicies(HWND window,
                                                           WindowProcessPolicyCallback associateProcesses,
                                                           WindowProcessPolicyCallback boostProcesses) noexcept
{
    const auto& console = ServiceLocator::LocateGlobals().getConsoleInformation();
    assert(console.IsConsoleLocked());
    RETURN_HR_IF(E_INVALIDARG, !window || !associateProcesses || !boostProcesses);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED), console.IsInVtIoMode());

    _policyWindow = window;
    _associateWindowProcesses = associateProcesses;
    _boostWindowProcesses = boostProcesses;
    return _UpdateWindowProcessPolicies();
}

HRESULT ConsoleProcessList::_UpdateWindowProcessPolicies() const noexcept
try
{
    const auto& console = ServiceLocator::LocateGlobals().getConsoleInformation();
    assert(console.IsConsoleLocked());
    if (!_policyWindow || console.IsInVtIoMode())
    {
        return S_OK;
    }

    std::vector<HANDLE> processes;
    HRESULT snapshotStatus = S_OK;
    try
    {
        processes.reserve(_processes.size());
        for (const auto process : _processes)
        {
            const auto handle = process->GetRawHandle();
            THROW_HR_IF(E_HANDLE, !handle);
            processes.push_back(handle);
        }
    }
    catch (...)
    {
        snapshotStatus = wil::ResultFromCaughtException();
        LOG_IF_FAILED(snapshotStatus);
        processes.clear();
    }

    const auto count = gsl::narrow<DWORD>(processes.size());
    auto qosStatus = _associateWindowProcesses(_policyWindow, count, count ? processes.data() : nullptr);
    if (qosStatus == S_FALSE)
    {
        qosStatus = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }
    LOG_IF_FAILED(qosStatus);
    const auto boostStatus = _boostWindowProcesses(_policyWindow, count, count ? processes.data() : nullptr);
    LOG_IF_FAILED(boostStatus);
    auto boostClearStatus = S_OK;
    if (FAILED(boostStatus))
    {
        boostClearStatus = _boostWindowProcesses(_policyWindow, 0, nullptr);
        LOG_IF_FAILED(boostClearStatus);
    }
    TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                      "StandaloneConsoleQosGroup",
                      TraceLoggingValue(reinterpret_cast<uintptr_t>(_policyWindow), "hwnd"),
                      TraceLoggingUInt32(count, "ProcessCount"),
                      TraceLoggingHResult(snapshotStatus, "SnapshotStatus"),
                      TraceLoggingHResult(qosStatus, "QosStatus"),
                      TraceLoggingHResult(boostStatus, "BoostStatus"),
                      TraceLoggingHResult(boostClearStatus, "BoostClearStatus"));
    for (const auto process : processes)
    {
        TraceLoggingWrite(g_hConhostV2EventTraceProvider,
                          "StandaloneConsoleQosMember",
                          TraceLoggingUInt32(GetProcessId(process), "TargetProcessId"),
                          TraceLoggingHResult(qosStatus, "QosStatus"),
                          TraceLoggingHResult(boostStatus, "BoostStatus"));
    }
    return FAILED(qosStatus) ? qosStatus : FAILED(boostStatus) ? boostStatus : snapshotStatus;
}
CATCH_RETURN();

void ConsoleProcessList::ConfigureForegroundBoostChannel(wil::unique_handle section, wil::unique_handle changed)
{
    using namespace Microsoft::Console;
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());
    wil::unique_mapview_ptr<ForegroundBoostSharedState> state{ static_cast<ForegroundBoostSharedState*>(
        MapViewOfFile(section.get(), FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(ForegroundBoostSharedState))) };
    THROW_LAST_ERROR_IF_NULL(state);
    THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
                static_cast<DWORD>(state->version) != ForegroundBoostProtocol);
    _foregroundBoostState = std::move(state);
    _foregroundBoostSection = std::move(section);
    _foregroundBoostChanged = std::move(changed);
    _PublishForegroundBoostClients();
}

void ConsoleProcessList::_PublishForegroundBoostClients() noexcept
{
    if (!_foregroundBoostState)
    {
        return;
    }
    Microsoft::Console::ForegroundBoostSnapshot snapshot;
    for (const auto process : _processes)
    {
        if (static_cast<size_t>(snapshot.count) == snapshot.clients.size())
        {
            snapshot.status = HRESULT_FROM_WIN32(ERROR_MORE_DATA);
            LOG_IF_FAILED(snapshot.status);
            break;
        }
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (!GetProcessTimes(process->GetRawHandle(), &creation, &exit, &kernel, &user))
        {
            snapshot.status = HRESULT_FROM_WIN32(GetLastError());
            LOG_IF_FAILED(snapshot.status);
            break;
        }
        snapshot.clients[snapshot.count++] = { process->dwProcessId, creation };
    }
    _foregroundBoostState->Publish(snapshot);
    LOG_IF_WIN32_BOOL_FALSE(SetEvent(_foregroundBoostChanged.get()));
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
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());
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
    ServiceLocator::LocateConsoleControl()->SetForeground(hProcess, fForeground);
}
