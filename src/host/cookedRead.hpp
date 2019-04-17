
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
               CommandHistory* const pCommandHistory,
               wchar_t* userBuffer,
               const size_t cchUserBuffer,
               const ULONG ctrlWakeupMask
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


    bool IsInsertMode() const noexcept;
    void SetInsertMode(const bool mode) noexcept;

    bool IsEchoInput() const noexcept;

    COORD PromptStartLocation() const noexcept;
    size_t VisibleCharCount() const;

    size_t MoveInsertionIndexLeft();
    size_t MoveInsertionIndexRight();

    SCREEN_INFORMATION& ScreenInfo();

    CommandHistory& History() noexcept;
    bool HasHistory() const noexcept;

private:
    enum class ReadState
    {
        Ready, // ready to read more user input
        Wait, // need to wait for more input
        Error, // something went wrong
        GotChar, // read a character from the user
        Complete, // the prompt has been completed (user pressed enter)
        CommandKey // the user pressed a command editing key
    };


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
    CommandHistory* const _pCommandHistory; // non-ownership pointer
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


    void _readChar(std::deque<wchar_t>& unprocessedChars);
    void _processChars(std::deque<wchar_t>& unprocessedChars);
    void _error();
    void _wait();
    void _complete(size_t& numBytes);
    void _commandKey();

    void _writeToPrompt(std::deque<wchar_t>& unprocessedChars);
    void _writeToScreen(const bool resetCursor);

    size_t _calculatePromptCellLength(const bool wholePrompt) const;
};
