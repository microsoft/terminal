/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- history.h

Abstract:
- Encapsulates the cmdline functions and structures specifically related to
        command history functionality.
--*/

#pragma once

class CommandHistory
{
public:
    // CommandHistory Flags
    static constexpr int CLE_ALLOCATED = 0x00000001;
    static constexpr int CLE_RESET = 0x00000002;

    static CommandHistory* s_Allocate(const std::wstring_view appName, const HANDLE processHandle);
    static CommandHistory* s_Find(const HANDLE processHandle);
    static CommandHistory* s_FindByExe(const std::wstring_view appName);
    static void s_ReallocExeToFront(const std::wstring_view appName, const size_t commands);
    static void s_Free(const HANDLE processHandle);
    static void s_ResizeAll(const size_t commands);
    static size_t s_CountOfHistories();

    enum class MatchOptions
    {
        None = 0x0,
        ExactMatch = 0x1,
        JustLooking = 0x2
    };

    enum class SearchDirection
    {
        Previous,
        Next
    };

    bool FindMatchingCommand(const std::wstring_view command,
                             const SHORT startingIndex,
                             SHORT& indexFound,
                             const MatchOptions options);
    bool IsAppNameMatch(const std::wstring_view other) const;

    [[nodiscard]] HRESULT Add(const std::wstring_view command,
                              const bool suppressDuplicates);

    [[nodiscard]] HRESULT Retrieve(const SearchDirection searchDirection,
                                   const gsl::span<wchar_t> buffer,
                                   size_t& commandSize);

    [[nodiscard]] HRESULT RetrieveNth(const SHORT index,
                                      const gsl::span<wchar_t> buffer,
                                      size_t& commandSize);

    size_t GetNumberOfCommands() const;
    std::wstring_view GetNth(const SHORT index) const;

    void Realloc(const size_t commands);
    void Empty();

    std::wstring Remove(const SHORT iDel);

    bool AtFirstCommand() const;
    bool AtLastCommand() const;

    std::wstring_view GetLastCommand() const;

    void Swap(const short indexA, const short indexB);

private:
    void _Reset();

    // _Next and _Prev go to the next and prev command
    // _Inc  and _Dec go to the next and prev slots
    // Don't get the two confused - it matters when the cmd history is not full!
    void _Prev(SHORT& ind) const;
    void _Next(SHORT& ind) const;
    void _Dec(SHORT& ind) const;
    void _Inc(SHORT& ind) const;

    std::vector<std::wstring> _commands;
    SHORT _maxCommands;

    std::wstring _appName;
    HANDLE _processHandle;

    static std::list<CommandHistory> s_historyLists;

public:
    DWORD Flags;
    SHORT LastDisplayed;

#ifdef UNIT_TESTING
    static void s_ClearHistoryListStorage();
    friend class HistoryTests;
#endif
};

DEFINE_ENUM_FLAG_OPERATORS(CommandHistory::MatchOptions);
