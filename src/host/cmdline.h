// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "input.h"
#include "screenInfo.hpp"
#include "server.h"

#include "history.h"
#include "alias.h"
#include "readDataCooked.hpp"
#include "popup.h"

class CommandLine
{
public:
    ~CommandLine();

    static CommandLine& Instance();

    static bool IsEditLineEmpty();
    void Hide(const bool fUpdateFields);
    void Show();
    bool IsVisible() const noexcept;

    [[nodiscard]] NTSTATUS ProcessCommandLine(COOKED_READ_DATA& cookedReadData,
                                              _In_ WCHAR wch,
                                              const DWORD dwKeyState);

    [[nodiscard]] HRESULT StartCommandNumberPopup(COOKED_READ_DATA& cookedReadData);

    bool HasPopup() const noexcept;
    Popup& GetPopup() const;

    void EndCurrentPopup();
    void EndAllPopups();

    void DeletePromptAfterCursor(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point DeleteFromRightOfCursor(COOKED_READ_DATA& cookedReadData) noexcept;

protected:
    CommandLine();

    // delete these because we don't want to accidentally get copies of the singleton
    CommandLine(const CommandLine&) = delete;
    CommandLine& operator=(const CommandLine&) = delete;

    [[nodiscard]] NTSTATUS _startCommandListPopup(COOKED_READ_DATA& cookedReadData);
    [[nodiscard]] NTSTATUS _startCopyFromCharPopup(COOKED_READ_DATA& cookedReadData);
    [[nodiscard]] NTSTATUS _startCopyToCharPopup(COOKED_READ_DATA& cookedReadData);

    void _processHistoryCycling(COOKED_READ_DATA& cookedReadData, const CommandHistory::SearchDirection searchDirection);
    void _setPromptToOldestCommand(COOKED_READ_DATA& cookedReadData);
    void _setPromptToNewestCommand(COOKED_READ_DATA& cookedReadData);
    til::point _deletePromptBeforeCursor(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _moveCursorToEndOfPrompt(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _moveCursorToStartOfPrompt(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _moveCursorLeftByWord(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _moveCursorLeft(COOKED_READ_DATA& cookedReadData);
    til::point _moveCursorRightByWord(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _moveCursorRight(COOKED_READ_DATA& cookedReadData) noexcept;
    void _insertCtrlZ(COOKED_READ_DATA& cookedReadData) noexcept;
    void _deleteCommandHistory(COOKED_READ_DATA& cookedReadData) noexcept;
    void _fillPromptWithPreviousCommandFragment(COOKED_READ_DATA& cookedReadData) noexcept;
    til::point _cycleMatchingCommandHistoryToPrompt(COOKED_READ_DATA& cookedReadData);

#ifdef UNIT_TESTING
    friend class CommandLineTests;
    friend class CommandNumberPopupTests;
#endif

private:
    std::deque<std::unique_ptr<Popup>> _popups;
    bool _isVisible;
};

void DeleteCommandLine(COOKED_READ_DATA& cookedReadData, const bool fUpdateFields);

void RedrawCommandLine(COOKED_READ_DATA& cookedReadData);

// Values for WriteChars(), WriteCharsLegacy() dwFlags
#define WC_INTERACTIVE 0x01
#define WC_KEEP_CURSOR_VISIBLE 0x02

// Word delimiters
bool IsWordDelim(const wchar_t wch);
bool IsWordDelim(const std::wstring_view charData);

bool IsValidStringBuffer(_In_ bool Unicode, _In_reads_bytes_(Size) PVOID Buffer, _In_ ULONG Size, _In_ ULONG Count, ...);

void SetCurrentCommandLine(COOKED_READ_DATA& cookedReadData, _In_ CommandHistory::Index Index);
