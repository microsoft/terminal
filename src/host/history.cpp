// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "history.h"

#include "_output.h"
#include "output.h"
#include "stream.h"
#include "_stream.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "../types/inc/convert.hpp"
#include "srvinit.h"
#include "resource.h"

#include "ApiRoutines.h"

#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using Microsoft::Console::Interactivity::ServiceLocator;

// I need to be a list because we rearrange elements inside to maintain a
// "least recently used" state. Doing many rearrangement operations with
// a list will maintain the iterator pointers as valid to the elements
// (where other collections like deque do not.)
// If CommandHistory::s_Allocate and friends stop shuffling elements
// for maintaining LRU, then this datatype can be changed.
std::list<CommandHistory> CommandHistory::s_historyLists;

CommandHistory* CommandHistory::s_Find(const HANDLE processHandle)
{
    for (auto& historyList : s_historyLists)
    {
        if (historyList._processHandle == processHandle)
        {
            FAIL_FAST_IF(WI_IsFlagClear(historyList.Flags, CLE_ALLOCATED));
            return &historyList;
        }
    }

    return nullptr;
}

// Routine Description:
// - This routine marks the command history buffer freed.
// Arguments:
// - processHandle - handle to client process.
void CommandHistory::s_Free(const HANDLE processHandle)
{
    const auto History = CommandHistory::s_Find(processHandle);
    if (History)
    {
        WI_ClearFlag(History->Flags, CLE_ALLOCATED);
        History->_processHandle = nullptr;
    }
}

void CommandHistory::s_ResizeAll(const size_t commands)
{
    const auto size = gsl::narrow<Index>(commands);

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.SetHistoryBufferSize(gsl::narrow<UINT>(commands));

    for (auto& historyList : s_historyLists)
    {
        historyList.Realloc(size);
    }
}

bool CommandHistory::IsAppNameMatch(const std::wstring_view other) const
{
    return CompareStringOrdinal(_appName.data(), gsl::narrow<int>(_appName.size()), other.data(), gsl::narrow<int>(other.size()), TRUE) == CSTR_EQUAL;
}

// Routine Description:
// - This routine is called when escape is entered or a command is added.
void CommandHistory::_Reset()
{
    LastDisplayed = GetNumberOfCommands() - 1;
    WI_SetFlag(Flags, CLE_RESET);
}

[[nodiscard]] HRESULT CommandHistory::Add(const std::wstring_view newCommand,
                                          const bool suppressDuplicates)
{
    RETURN_HR_IF(E_OUTOFMEMORY, _maxCommands == 0);
    FAIL_FAST_IF(WI_IsFlagClear(Flags, CLE_ALLOCATED));

    if (newCommand.size() == 0)
    {
        return S_OK;
    }

    try
    {
        if (_commands.size() == 0 ||
            _commands.back().size() != newCommand.size() ||
            !std::equal(_commands.back().cbegin(),
                        _commands.back().cbegin() + newCommand.size(),
                        newCommand.cbegin(),
                        newCommand.cend()))
        {
            std::wstring reuse{};

            if (suppressDuplicates)
            {
                Index index;
                if (FindMatchingCommand(newCommand, LastDisplayed, index, CommandHistory::MatchOptions::ExactMatch))
                {
                    reuse = Remove(index);
                }
            }

            // find free record.  if all records are used, free the lru one.
            if (GetNumberOfCommands() == _maxCommands)
            {
                _commands.erase(_commands.cbegin());
                // move LastDisplayed back one in order to stay synced with the
                // command it referred to before erasing the lru one
                --LastDisplayed;
            }

            // add newCommand to array
            if (!reuse.empty())
            {
                _commands.emplace_back(reuse);
            }
            else
            {
                _commands.emplace_back(newCommand);
            }

            if (LastDisplayed == -1 ||
                _commands.at(LastDisplayed).size() != newCommand.size() ||
                !std::equal(_commands.at(LastDisplayed).cbegin(),
                            _commands.at(LastDisplayed).cbegin() + newCommand.size(),
                            newCommand.cbegin(),
                            newCommand.cend()))
            {
                _Reset();
            }
        }
    }
    CATCH_RETURN();
    WI_SetFlag(Flags, CLE_RESET); // remember that we've returned a cmd

    return S_OK;
}

std::wstring_view CommandHistory::GetNth(Index index) const
{
    if (index >= 0 && index < GetNumberOfCommands())
    {
        return _commands.at(index);
    }
    return {};
}

const std::vector<std::wstring>& CommandHistory::GetCommands() const noexcept
{
    return _commands;
}

[[nodiscard]] HRESULT CommandHistory::RetrieveNth(const Index index, std::span<wchar_t> buffer, size_t& commandSize)
{
    LastDisplayed = index;

    try
    {
        const auto& cmd = _commands.at(index);
        if (cmd.size() > buffer.size())
        {
            commandSize = buffer.size(); // room for CRLF?
        }
        else
        {
            commandSize = cmd.size();
        }

        std::copy_n(cmd.cbegin(), commandSize, buffer.begin());

        commandSize *= sizeof(wchar_t);

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT CommandHistory::Retrieve(const SearchDirection searchDirection,
                                               const std::span<wchar_t> buffer,
                                               size_t& commandSize)
{
    FAIL_FAST_IF(!(WI_IsFlagSet(Flags, CLE_ALLOCATED)));

    if (_commands.size() == 0)
    {
        return E_FAIL;
    }

    if (_commands.size() == 1)
    {
        LastDisplayed = 0;
    }
    else if (searchDirection == SearchDirection::Previous)
    {
        // if this is the first time for this read that a command has
        // been retrieved, return the current command.  otherwise, return
        // the previous command.
        if (WI_IsFlagSet(Flags, CLE_RESET))
        {
            WI_ClearFlag(Flags, CLE_RESET);
        }
        else
        {
            _Prev(LastDisplayed);
        }
    }
    else
    {
        _Next(LastDisplayed);
    }

    return RetrieveNth(LastDisplayed, buffer, commandSize);
}

std::wstring_view CommandHistory::GetLastCommand() const
{
    return GetNth(LastDisplayed);
}

void CommandHistory::Empty()
{
    _commands.clear();
    LastDisplayed = -1;
    WI_SetFlag(Flags, CLE_RESET);
}

bool CommandHistory::AtFirstCommand() const
{
    if (WI_IsFlagSet(Flags, CLE_RESET))
    {
        return FALSE;
    }

    auto i = LastDisplayed - 1;
    if (i == -1)
    {
        i = GetNumberOfCommands() - 1;
    }

    return (i == GetNumberOfCommands() - 1);
}

bool CommandHistory::AtLastCommand() const
{
    return LastDisplayed == GetNumberOfCommands() - 1;
}

void CommandHistory::Realloc(const Index commands)
{
    if (_maxCommands == commands)
    {
        return;
    }

    _commands.resize(std::min(_commands.size(), gsl::narrow_cast<size_t>(std::max(0, commands))));

    WI_SetFlag(Flags, CLE_RESET);
    LastDisplayed = GetNumberOfCommands() - 1;
    _maxCommands = commands;
}

void CommandHistory::s_ReallocExeToFront(const std::wstring_view appName, const size_t commands)
{
    const auto size = gsl::narrow<Index>(commands);

    for (auto it = s_historyLists.begin(), end = s_historyLists.end(); it != end; ++it)
    {
        if (WI_IsFlagSet(it->Flags, CLE_ALLOCATED) && it->IsAppNameMatch(appName))
        {
            it->Realloc(size);
            s_historyLists.splice(s_historyLists.begin(), s_historyLists, it);
            return;
        }
    }
}

CommandHistory* CommandHistory::s_FindByExe(const std::wstring_view appName)
{
    for (auto& historyList : s_historyLists)
    {
        if (WI_IsFlagSet(historyList.Flags, CLE_ALLOCATED) && historyList.IsAppNameMatch(appName))
        {
            return &historyList;
        }
    }
    return nullptr;
}

size_t CommandHistory::s_CountOfHistories()
{
    return s_historyLists.size();
}

// Routine Description:
// - This routine returns the LRU command history buffer, or the command history buffer that corresponds to the app name.
// Arguments:
// - Console - pointer to console.
// Return Value:
// - Pointer to command history buffer.  if none are available, returns nullptr.
CommandHistory* CommandHistory::s_Allocate(const std::wstring_view appName, const HANDLE processHandle)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // Reuse a history buffer.  The buffer must be !CLE_ALLOCATED.
    // If possible, the buffer should have the same app name.
    const auto beg = s_historyLists.begin();
    const auto end = s_historyLists.end();
    auto BestCandidate = end;
    auto SameApp = false;

    for (auto it = beg; it != end; ++it)
    {
        if (WI_IsFlagClear(it->Flags, CLE_ALLOCATED))
        {
            // use MRU history buffer with same app name
            if (it->IsAppNameMatch(appName))
            {
                BestCandidate = it;
                SameApp = true;
                break;
            }
        }
    }

    // if there isn't a free buffer for the app name and the maximum number of
    // command history buffers hasn't been allocated, allocate a new one.
    if (!SameApp && s_historyLists.size() < gci.GetNumberOfHistoryBuffers())
    {
        CommandHistory History;

        History._appName = appName;
        History.Flags = CLE_ALLOCATED;
        History.LastDisplayed = -1;
        History._maxCommands = gsl::narrow<Index>(gci.GetHistoryBufferSize());
        History._processHandle = processHandle;
        return &s_historyLists.emplace_front(History);
    }

    // If we have no candidate already and we need one,
    // take the LRU (which is the back/last one) which isn't allocated
    // and if possible the one with empty commands list.
    if (BestCandidate == end)
    {
        for (auto it = beg; it != end; ++it)
        {
            if (WI_IsFlagClear(it->Flags, CLE_ALLOCATED))
            {
                if (it->_commands.empty() || BestCandidate == end || !BestCandidate->_commands.empty())
                {
                    BestCandidate = it;
                }
            }
        }
    }

    // If the app name doesn't match, copy in the new app name and free the old commands.
    if (BestCandidate != end)
    {
        if (!SameApp)
        {
            BestCandidate->_commands.clear();
            BestCandidate->LastDisplayed = -1;
            BestCandidate->_appName = appName;
        }

        BestCandidate->_processHandle = processHandle;
        WI_SetFlag(BestCandidate->Flags, CLE_ALLOCATED);

        // move to the front of the list
        s_historyLists.splice(beg, s_historyLists, BestCandidate);
        return &*BestCandidate;
    }

    return nullptr;
}

CommandHistory::Index CommandHistory::GetNumberOfCommands() const
{
    return gsl::narrow_cast<Index>(_commands.size());
}

void CommandHistory::_Prev(Index& ind) const
{
    if (ind <= 0)
    {
        ind = GetNumberOfCommands();
    }
    ind--;
}

void CommandHistory::_Next(Index& ind) const
{
    ++ind;
    if (ind >= GetNumberOfCommands())
    {
        ind = 0;
    }
}

void CommandHistory::_Dec(Index& ind) const
{
    if (ind <= 0)
    {
        ind = _maxCommands;
    }
    ind--;
}

void CommandHistory::_Inc(Index& ind) const
{
    ++ind;
    if (ind >= _maxCommands)
    {
        ind = 0;
    }
}

std::wstring CommandHistory::Remove(const Index iDel)
{
    if (iDel < 0 || iDel >= GetNumberOfCommands())
    {
        return {};
    }

    const auto str = std::move(_commands.at(iDel));
    _commands.erase(_commands.begin() + iDel);

    if (LastDisplayed == iDel)
    {
        LastDisplayed = -1;
    }
    else if (LastDisplayed > iDel)
    {
        _Dec(LastDisplayed);
    }

    return str;
}

// Routine Description:
// - this routine finds the most recent command that starts with the letters already in the current command.  it returns the array index (no mod needed).
[[nodiscard]] bool CommandHistory::FindMatchingCommand(const std::wstring_view givenCommand,
                                                       const Index startingIndex,
                                                       Index& indexFound,
                                                       const MatchOptions options)
{
    indexFound = startingIndex;

    if (_commands.size() == 0)
    {
        return false;
    }

    if (WI_IsFlagClear(options, MatchOptions::JustLooking) && WI_IsFlagSet(Flags, CLE_RESET))
    {
        WI_ClearFlag(Flags, CLE_RESET);
    }
    else
    {
        _Prev(indexFound);
    }

    if (givenCommand.empty())
    {
        return true;
    }

    try
    {
        for (size_t i = 0; i < _commands.size(); i++)
        {
            const auto& storedCommand = _commands.at(indexFound);
            if ((WI_IsFlagClear(options, MatchOptions::ExactMatch) && (givenCommand.size() <= storedCommand.size())) || (givenCommand.size() == storedCommand.size()))
            {
                if (til::starts_with(storedCommand, givenCommand))
                {
                    return true;
                }
            }

            _Prev(indexFound);
        }
    }
    CATCH_LOG();

    return false;
}

#ifdef UNIT_TESTING
void CommandHistory::s_ClearHistoryListStorage()
{
    s_historyLists.clear();
}
#endif

// Routine Description:
// - swaps the locations of two history items
// Arguments:
// - indexA - index of one history item to swap
// - indexB - index of one history item to swap
void CommandHistory::Swap(const Index indexA, const Index indexB)
{
    const auto num = GetNumberOfCommands();
    if (indexA != indexB &&
        indexA >= 0 && indexA < num &&
        indexB >= 0 && indexB < num)
    {
        std::swap(_commands.at(indexA), _commands.at(indexB));
    }
}

// Routine Description:
// - Clears all command history for the given EXE name
// - Will convert input parameters and call the W version of this method
// Arguments:
// - exeName - The client EXE application attached to the host whose history we should clear
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::ExpungeConsoleCommandHistoryAImpl(const std::string_view exeName) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        const auto exeNameW = ConvertToW(gci.CP, exeName);

        return ExpungeConsoleCommandHistoryWImpl(exeNameW);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Clears all command history for the given EXE name
// Arguments:
// - exeName - The client EXE application attached to the host whose history we should clear
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::ExpungeConsoleCommandHistoryWImpl(const std::wstring_view exeName) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto history = CommandHistory::s_FindByExe(exeName);
        if (history)
        {
            history->Empty();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the number of commands that will be stored in history for a given EXE name
// - Will convert input parameters and call the W version of this method
// Arguments:
// - exeName - A client EXE application attached to the host
// - numberOfCommands - Specifies the maximum length of the associated history buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::SetConsoleNumberOfCommandsAImpl(const std::string_view exeName,
                                                     const size_t numberOfCommands) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        const auto exeNameW = ConvertToW(gci.CP, exeName);

        return SetConsoleNumberOfCommandsWImpl(exeNameW, numberOfCommands);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the number of commands that will be stored in history for a given EXE name
// Arguments:
// - exeName - A client EXE application attached to the host
// - numberOfCommands - Specifies the maximum length of the associated history buffer
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::SetConsoleNumberOfCommandsWImpl(const std::wstring_view exeName,
                                                     const size_t numberOfCommands) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        CommandHistory::s_ReallocExeToFront(exeName, numberOfCommands);

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// - Works for both Unicode and Multibyte text.
// - This method configuration is called for both A/W routines to allow us an efficient way of asking the system
//   the lengths of how long each conversion would be without actually performing the full allocations/conversions.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - countInUnicode - True for W version (UCS-2 Unicode) calls. False for A version calls (all multibyte formats.)
// - codepage - Set to valid Windows Codepage for A version calls. Ignored for W (but typically just set to 0.)
// - historyLength - Receives the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleCommandHistoryLengthImplHelper(const std::wstring_view exeName,
                                                 const bool countInUnicode,
                                                 const UINT codepage,
                                                 size_t& historyLength)
{
    // Ensure output variables are initialized
    historyLength = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    const auto pCommandHistory = CommandHistory::s_FindByExe(exeName);
    if (nullptr != pCommandHistory)
    {
        size_t cchNeeded = 0;

        // Every command history item is made of a string length followed by 1 null character.
        const size_t cchNull = 1;

        for (const auto& command : pCommandHistory->GetCommands())
        {
            auto cchCommand = command.size();

            // If we're counting how much multibyte space will be needed, trial convert the command string before we add.
            if (!countInUnicode)
            {
                cchCommand = GetALengthFromW(codepage, command);
            }

            // This is the proposed length of the whole string.
            size_t cchProposed;
            RETURN_IF_FAILED(SizeTAdd(cchCommand, cchNull, &cchProposed));

            // Accumulate the result
            RETURN_IF_FAILED(SizeTAdd(cchNeeded, cchProposed, &cchNeeded));
        }

        historyLength = cchNeeded;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// - Converts input text from A to W then makes the call to the W implementation.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - length - Receives the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryLengthAImpl(const std::string_view exeName,
                                                         size_t& length) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

    // Ensure output variables are initialized
    length = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto exeNameW = ConvertToW(codepage, exeName);
        return GetConsoleCommandHistoryLengthImplHelper(exeNameW, false, codepage, length);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the amount of space needed to retrieve all command history for a given EXE name
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - length - Receives the length of buffer that would be required to retrieve all history for the given exe.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryLengthWImpl(const std::wstring_view exeName,
                                                         size_t& length) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return GetConsoleCommandHistoryLengthImplHelper(exeName, true, 0, length);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the full command history for a given EXE name known to the console.
// - It is permitted to call this function without having a target buffer. Use the result to allocate
//   the appropriate amount of space and call again.
// - This behavior exists to allow the A version of the function to help allocate the right temp buffer for conversion of
//   the output/result data.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - historyBuffer - The target buffer for data we are attempting to retrieve. Optionally empty to retrieve needed space.
// - writtenOrNeeded - Will specify how many characters were written (if buffer is valid)
//                     or how many characters would have been consumed.
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT GetConsoleCommandHistoryWImplHelper(const std::wstring_view exeName,
                                            std::span<wchar_t> historyBuffer,
                                            size_t& writtenOrNeeded)
{
    // Ensure output variables are initialized
    writtenOrNeeded = 0;
    if (historyBuffer.size() > 0)
    {
        til::at(historyBuffer, 0) = UNICODE_NULL;
    }

    const auto CommandHistory = CommandHistory::s_FindByExe(exeName);

    if (nullptr != CommandHistory)
    {
        auto CommandBufferW = historyBuffer.data();

        size_t cchTotalLength = 0;

        const size_t cchNull = 1;

        for (const auto& command : CommandHistory->GetCommands())
        {
            const auto cchCommand = command.size();

            size_t cchNeeded;
            RETURN_IF_FAILED(SizeTAdd(cchCommand, cchNull, &cchNeeded));

            // If we can return the data, attempt to do so until we're done or it overflows.
            // If we cannot return data, we're just going to loop anyway and count how much space we'd need.
            if (historyBuffer.size() > 0)
            {
                // Calculate what the new total would be after we add what we need.
                size_t cchNewTotal;
                RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchNewTotal));

                RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), cchNewTotal > historyBuffer.size());

                size_t cchRemaining;
                RETURN_IF_FAILED(SizeTSub(historyBuffer.size(),
                                          cchTotalLength,
                                          &cchRemaining));

                RETURN_IF_FAILED(StringCchCopyNW(CommandBufferW,
                                                 cchRemaining,
                                                 command.data(),
                                                 command.size()));

                CommandBufferW += cchNeeded;
            }

            RETURN_IF_FAILED(SizeTAdd(cchTotalLength, cchNeeded, &cchTotalLength));
        }

        writtenOrNeeded = cchTotalLength;
    }

    return S_OK;
}

// Routine Description:
// - Retrieves the full command history for a given EXE name known to the console.
// - Converts inputs from A to W, calls the W version of this method, and then converts the resulting text W to A.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - commandHistory - The target buffer for data we are attempting to retrieve.
// - written - Will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryAImpl(const std::string_view exeName,
                                                   std::span<char> commandHistory,
                                                   size_t& written) noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.CP;

    // Ensure output variables are initialized
    written = 0;

    try
    {
        if (commandHistory.size() > 0)
        {
            til::at(commandHistory, 0) = ANSI_NULL;
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Convert our input parameters to Unicode.
        const auto exeNameW = ConvertToW(codepage, exeName);

        // Figure out how big our temporary Unicode buffer must be to retrieve output
        size_t bufferNeeded;
        RETURN_IF_FAILED(GetConsoleCommandHistoryWImplHelper(exeNameW, {}, bufferNeeded));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == bufferNeeded);

        // Allocate a unicode buffer of the right size.
        auto buffer = std::make_unique<wchar_t[]>(bufferNeeded);
        RETURN_IF_NULL_ALLOC(buffer);

        // Call the Unicode version of this method
        size_t bufferWritten;
        RETURN_IF_FAILED(GetConsoleCommandHistoryWImplHelper(exeNameW, std::span<wchar_t>(buffer.get(), bufferNeeded), bufferWritten));

        // Convert result to A
        const auto converted = ConvertToA(codepage, { buffer.get(), bufferWritten });

        // Copy safely to output buffer
        // - CommandHistory are a series of null terminated strings. We cannot use a SafeString function to copy.
        //   So instead, validate and use raw memory copy.
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW), converted.size() > commandHistory.size());
        memcpy_s(commandHistory.data(), commandHistory.size(), converted.data(), converted.size());

        // And return the size copied.
        written = converted.size();

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves the full command history for a given EXE name known to the console.
// - Converts inputs from A to W, calls the W version of this method, and then converts the resulting text W to A.
// Arguments:
// - exeName - The client EXE application attached to the host whose set we should check
// - commandHistory - The target buffer for data we are attempting to retrieve.
// - written - Will specify how many characters were written
// Return Value:
// - Check HRESULT with SUCCEEDED. Can return memory, safe math, safe string, or locale conversion errors.
HRESULT ApiRoutines::GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                   std::span<wchar_t> commandHistory,
                                                   size_t& written) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return GetConsoleCommandHistoryWImplHelper(exeName, commandHistory, written);
    }
    CATCH_RETURN();
}
