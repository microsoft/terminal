// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

class CommandHistory
{
public:
    using Index = int32_t;
    static constexpr Index IndexMax = INT32_MAX;

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
                             const Index startingIndex,
                             Index& indexFound,
                             const MatchOptions options);
    bool IsAppNameMatch(const std::wstring_view other) const;

    [[nodiscard]] HRESULT Add(const std::wstring_view command,
                              const bool suppressDuplicates);

    [[nodiscard]] HRESULT Retrieve(const SearchDirection searchDirection,
                                   const std::span<wchar_t> buffer,
                                   size_t& commandSize);

    [[nodiscard]] HRESULT RetrieveNth(const Index index,
                                      const std::span<wchar_t> buffer,
                                      size_t& commandSize);

    Index GetNumberOfCommands() const;
    std::wstring_view GetNth(Index index) const;
    const std::vector<std::wstring>& GetCommands() const noexcept;

    void Realloc(Index commands);
    void Empty();

    std::wstring Remove(const Index iDel);

    bool AtFirstCommand() const;
    bool AtLastCommand() const;

    std::wstring_view GetLastCommand() const;

    void Swap(const Index indexA, const Index indexB);

private:
    void _Reset();

    // _Next and _Prev go to the next and prev command
    // _Inc  and _Dec go to the next and prev slots
    // Don't get the two confused - it matters when the cmd history is not full!
    void _Prev(Index& ind) const;
    void _Next(Index& ind) const;
    void _Dec(Index& ind) const;
    void _Inc(Index& ind) const;

    // NOTE: In conhost v1 this used to be a circular buffer because removal at the
    // start is a very common operation. It seems this was lost in the C++ refactor.
    std::vector<std::wstring> _commands;
    Index _maxCommands = 0;

    std::wstring _appName;
    HANDLE _processHandle = nullptr;

    static std::list<CommandHistory> s_historyLists;

public:
    DWORD Flags = 0;
    Index LastDisplayed = 0;

#ifdef UNIT_TESTING
    static void s_ClearHistoryListStorage();
    friend class HistoryTests;
#endif
};

DEFINE_ENUM_FLAG_OPERATORS(CommandHistory::MatchOptions);
