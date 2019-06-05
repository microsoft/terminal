
#pragma once

#include "readData.hpp"
#include "screenInfo.hpp"
#include "history.h"

class CookedRead final : public ReadData
{
public:

    CookedRead(InputBuffer* const pInputBuffer,
               INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
               SCREEN_INFORMATION& screenInfo,
               std::shared_ptr<CommandHistory> pCommandHistory,
               wchar_t* userBuffer,
               const size_t cchUserBuffer,
               const ULONG ctrlWakeupMask,
               const std::wstring_view exeName
        );

    bool Notify(const WaitTerminationReason TerminationReason,
                const bool fIsUnicode,
                _Out_ NTSTATUS* const pReplyStatus,
                _Out_ size_t* const pNumBytes,
                _Out_ DWORD* const pControlKeyState,
                _Out_ void* const pOutputData);

    [[nodiscard]]
    NTSTATUS Read(const bool isUnicode,
                  size_t& numBytes,
                  ULONG& controlKeyState) noexcept;

    void Erase();
    void Hide();
    void Show();


    bool IsInsertMode() const noexcept;
    void SetInsertMode(const bool mode) noexcept;

    bool IsEchoInput() const noexcept;

    COORD PromptStartLocation() const noexcept;
    COORD BeforePopupCursorPosition() const noexcept;
    size_t VisibleCharCount() const;

    size_t MoveInsertionIndexLeft();
    size_t MoveInsertionIndexRight();
    void MoveInsertionIndexToStart() noexcept;
    size_t MoveInsertionIndexToEnd();
    size_t MoveInsertionIndexLeftByWord();
    size_t MoveInsertionIndexRightByWord();

    void SetPromptToOldestCommand();
    void SetPromptToNewestCommand();
    void SetPromptToCommand(const size_t index);
    void SetPromptToCommand(const CommandHistory::SearchDirection searchDirection);
    void SetPromptToMatchingHistoryCommand();
    void FillPromptWithPreviousCommandFragment();

    void Overwrite(const std::wstring_view::const_iterator startIt, const std::wstring_view::const_iterator endIt);

    size_t DeletePromptBeforeInsertionIndex();
    void DeletePromptAfterInsertionIndex();
    void DeleteFromRightOfInsertionIndex();

    void InsertCtrlZ();

    SCREEN_INFORMATION& ScreenInfo();

    std::shared_ptr<CommandHistory> History() noexcept;
    bool HasHistory() const noexcept;

    void BufferInput(const wchar_t wch);

    std::wstring_view Prompt();
    std::wstring_view PromptFromInsertionIndex();

    size_t InsertionIndex() const noexcept;

private:
    enum class ReadState
    {
        Ready, // ready to read more user input
        Wait, // need to wait for more input
        Error, // something went wrong
        GotChar, // read a character from the user
        Complete, // the prompt has been completed (user pressed enter)
        CommandKey, // the user pressed a command editing key
        Popup
    };

    static bool _isSurrogatePairAt(const std::wstring_view wstrView, const size_t index);
    static size_t _visibleCharCountOf(const std::wstring_view wstrView);

    bool _isTailSurrogatePair() const;
    bool _isSurrogatePairAt(const size_t index) const;
    bool _isCtrlWakeupMaskTriggered(const wchar_t wch) const noexcept;

    bool _insertMode;


    // the buffer that the prompt data will go into in order to finish the ReadConsole api call
    wchar_t* _userBuffer;
    size_t _cchUserBuffer;

    SCREEN_INFORMATION& _screenInfo;
    // storage for the prompt text data
    std::wstring _prompt;
    // the location of where the interactive portion of the prompt starts
    COORD _promptStartLocation;
    // the location of the cursor before a popup is launched
    COORD _beforePopupCursorPosition;
    std::shared_ptr<CommandHistory> _pCommandHistory; // shared pointer
    // mask of control keys that if pressed will end the cooked read early
    const ULONG _ctrlWakeupMask;
    // current state of the CookedRead
    ReadState _state;
    NTSTATUS _status;
    // index of _prompt for character insertion
    size_t _insertionIndex;

    // used for special command line keys
    bool _commandLineEditingKeys;
    DWORD _keyState;
    wchar_t _commandKeyChar;

    bool _echoInput;

    std::deque<wchar_t> _bufferedInput;

    // name of the application that requested the read. used for alias transformation.
    std::wstring _exeName;


    void _readChar(std::deque<wchar_t>& unprocessedChars);
    void _processChars(std::deque<wchar_t>& unprocessedChars);
    void _error();
    void _wait();
    void _complete(size_t& numBytes);
    void _commandKey();
    void _popup();

    void _backspace();
    void _clearPromptCells();
    void _writeToPrompt(std::deque<wchar_t>& unprocessedChars);
    void _writeToScreen(const bool resetCursor);

    size_t _calculatePromptCellLength(const bool wholePrompt) const;

    bool _isInsertionIndexAtPromptBegin();
    bool _isInsertionIndexAtPromptEnd();

    void _adjustCursorToInsertionIndex();

#if UNIT_TESTING
    friend class CommandLineTests;
    friend class CopyFromCharPopupTests;
    friend class CopyToCharPopupTests;
    friend class CommandNumberPopupTests;
    friend class CommandListPopupTests;
    friend class PopupTestHelper;

public:
    inline void SetPromptStartLocation(const COORD location)
    {
        _promptStartLocation = location;
    }

#endif
};
