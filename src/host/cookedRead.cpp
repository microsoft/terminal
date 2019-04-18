/********************************************************
*                                                       *
*   Copyright (C) Microsoft. All rights reserved.       *
*                                                       *
********************************************************/


#include "precomp.h"
#include "cookedRead.hpp"
#include "stream.h"
#include "_stream.h"
#include "../types/inc/Utf16Parser.hpp"
#include "../types/inc/GlyphWidth.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"


CookedRead::CookedRead(InputBuffer* const pInputBuffer,
                       INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
                       SCREEN_INFORMATION& screenInfo,
                       CommandHistory* const pCommandHistory,
                       wchar_t* userBuffer,
                       const size_t cchUserBuffer,
                       const ULONG ctrlWakeupMask
) :
    ReadData(pInputBuffer, pInputReadHandleData),
    _screenInfo{ screenInfo },
    _userBuffer{ userBuffer },
    _cchUserBuffer{ cchUserBuffer },
    _ctrlWakeupMask{ ctrlWakeupMask },
    _pCommandHistory{ pCommandHistory },
    _insertMode{ ServiceLocator::LocateGlobals().getConsoleInformation().GetInsertMode() },
    _state{ ReadState::Ready },
    _status{ STATUS_SUCCESS },
    _insertionIndex{ 0 },
    _commandLineEditingKeys{ false },
    _keyState{ 0 },
    _commandKeyChar{ UNICODE_NULL },
    _echoInput{ WI_IsFlagSet(pInputBuffer->InputMode, ENABLE_ECHO_INPUT) }
{
    _prompt.reserve(256);
    _promptStartLocation = _screenInfo.GetTextBuffer().GetCursor().GetPosition();
}

SCREEN_INFORMATION& CookedRead::ScreenInfo()
{
    return _screenInfo;
}

bool CookedRead::HasHistory() const noexcept
{
    return _pCommandHistory != nullptr;
}

CommandHistory& CookedRead::History() noexcept
{
    return *_pCommandHistory;
}

void CookedRead::Erase()
{
    _clearPromptCells();
    _prompt.erase();
    _insertionIndex = 0;
    _writeToScreen(true);
}

bool CookedRead::IsInsertMode() const noexcept
{
    return _insertMode;
}

void CookedRead::SetInsertMode(const bool mode) noexcept
{
    _insertMode = mode;
}

bool CookedRead::IsEchoInput() const noexcept
{
    return _echoInput;
}

COORD CookedRead::PromptStartLocation() const noexcept
{
    return _promptStartLocation;
}

// Routine Description:
// - counts the number of visible characters in the prompt
// Return Value:
// - the number of visible characters in the prompt (counts surrogate pairs as one character)
size_t CookedRead::VisibleCharCount() const
{
    return _visibleCharCountOf({ _prompt.c_str(), _prompt.size() });
}

// Routine Description:
// - counts the number of visible characters in wstrView
// Arugments:
// - wstrView - the text to count the visible characters of
// Return Value:
// - the number of visible characters in the string view (counts surrogate pairs as one character)
size_t CookedRead::_visibleCharCountOf(const std::wstring_view wstrView)
{
    size_t count = 0;
    for (size_t i = 0; i < wstrView.size(); ++i)
    {
        if (i + 1 < wstrView.size() && _isSurrogatePairAt(wstrView, i))
        {
            // need to increment i in order to jump past surrogate pair
            ++i;
        }
        ++count;
    }
    return count;
}

// Routine Description:
// - moves the insertion index one codepoint to the left if possible
// Return Value:
// - returns the number of cells that the move operation passed over
size_t CookedRead::MoveInsertionIndexLeft()
{
    // check if there's anything to the left to
    if (_isInsertionIndexAtPromptBegin())
    {
        return 0;
    }

    // move the insertion index left by one codepoint
    std::wstring_view glyph;
    if (_insertionIndex >= 2 &&
        _isSurrogatePairAt(_insertionIndex - 2))
    {
        _insertionIndex -= 2;
        glyph = { &_prompt.at(_insertionIndex), 2 };
    }
    else
    {
        --_insertionIndex;
        glyph = { &_prompt.at(_insertionIndex), 1 };
    }

    // calculate how many cells the cursor was moved by the insertion index move
    size_t cellsMoved = 0;
    if (IsGlyphFullWidth(glyph))
    {
        cellsMoved = 2;
    }
    else
    {
        cellsMoved = 1;
    }
    return cellsMoved;
}

// Routine Description:
// - moves the insertion index one codepoint to the right if possible
// right beyond the end of the current prompt text
// Return Value:
// - returns the number of cells that the move operation passed over
size_t CookedRead::MoveInsertionIndexRight()
{
    // check if we're at the far right side of the prompt text for character insertion from last command in
    // the history
    if (_isInsertionIndexAtPromptEnd())
    {
        // if we have any command history, grab the nth next character from the previous command and write it
        // to the buffer, then move over it
        if (_pCommandHistory)
        {
            const std::wstring_view lastCommand = _pCommandHistory->GetLastCommand();
            const size_t lastCommandCodepointCount = _visibleCharCountOf(lastCommand);
            const size_t currentCommandCodepointCount = VisibleCharCount();
            if (lastCommandCodepointCount > currentCommandCodepointCount)
            {
                const std::vector<std::vector<wchar_t>> parsedLastCommand = Utf16Parser::Parse(lastCommand);
                const std::vector<wchar_t>& glyph = parsedLastCommand.at(currentCommandCodepointCount);
                for (const wchar_t wch : glyph)
                {
                    // don't bother adjusting the insertion index here, it will be handled below
                    _prompt.push_back(wch);
                }
                _writeToScreen(true);
            }
            else
            {
                // no history data to copy, so we can't move to the right
                return 0;
            }
        }
        else
        {
            // no history data to copy, so we can't move to the right
            return 0;
        }
    }

    // move the insertion index right by one codepoint
    std::wstring_view glyph;
    if (_insertionIndex + 1 < _prompt.size() &&
        _isSurrogatePairAt(_insertionIndex))
    {
        glyph = { &_prompt.at(_insertionIndex), 2 };
        _insertionIndex += 2;
    }
    else
    {
        glyph = { &_prompt.at(_insertionIndex), 1 };
        ++_insertionIndex;
    }

    // calculate how many cells the cursor was moved by the insertion index move
    size_t cellsMoved = 0;
    if (IsGlyphFullWidth(glyph))
    {
        cellsMoved = 2;
    }
    else
    {
        cellsMoved = 1;
    }
    return cellsMoved;
}

// Routine Description:
// - moves insertion index to the beginning of the prompt
void CookedRead::MoveInsertionIndexToStart() noexcept
{
    _insertionIndex = 0;
}

// Routine Description:
// - moves insertion index to the end of the prompt
// Return Value:
// - the number of cells that the insertion point has moved by
size_t CookedRead::MoveInsertionIndexToEnd()
{
    const size_t leftCells = _calculatePromptCellLength(false);
    const size_t allCells = _calculatePromptCellLength(true);
    _insertionIndex = _prompt.size();
    return allCells - leftCells;
}

// Routine Description:
// - moves insertion index to the left by a word
// Return Value:
// - the number of cells that the insertion point has moved by
size_t CookedRead::MoveInsertionIndexLeftByWord()
{
    if (_isInsertionIndexAtPromptBegin())
    {
        return 0;
    }

    size_t cellsMoved = 0;
    // move through any word delimiters to the left
    while (!_isInsertionIndexAtPromptBegin() && IsWordDelim(_prompt.at(_insertionIndex - 1)))
    {
        cellsMoved += MoveInsertionIndexLeft();
    }
    // move through word to next word delimiter
    while (!_isInsertionIndexAtPromptBegin() && !IsWordDelim(_prompt.at(_insertionIndex - 1)))
    {
        cellsMoved += MoveInsertionIndexLeft();
    }

    return cellsMoved;
}

// Routine Description:
// - moves insertion index to the right by a word
// Return Value:
// - the number of cells that the insertion point has moved by
size_t CookedRead::MoveInsertionIndexRightByWord()
{
    if (_isInsertionIndexAtPromptEnd())
    {
        return 0;
    }

    size_t cellsMoved = 0;
    if (!IsWordDelim(_prompt.at(_insertionIndex)))
    {
        // move through a word until we come to the first word delimiter
        while (!_isInsertionIndexAtPromptEnd() && !IsWordDelim(_prompt.at(_insertionIndex)))
        {
            cellsMoved += MoveInsertionIndexRight();
        }
    }
    // move through word delimiters until we encounter a char that isn't one
    while (!_isInsertionIndexAtPromptEnd() && IsWordDelim(_prompt.at(_insertionIndex)))
    {
        cellsMoved += MoveInsertionIndexRight();
    }

    return cellsMoved;
}

// Routine Description:
// - sets prompt to the oldest command in the command history
void CookedRead::SetPromptToOldestCommand()
{
    if (_pCommandHistory && _pCommandHistory->GetNumberOfCommands() > 0)
    {
        const std::wstring_view command = _pCommandHistory->GetNth(0);
        Erase();
        _prompt = command;
        _insertionIndex = _prompt.size();
        _writeToScreen(true);
    }
}

// Routine Description:
// - checks if wch matches up with the control character masking for early data return
// Arguments:
// - wch - the wchar to check
// Return Value:
// - true if wch is part of the the control character mask, false otherwise
bool CookedRead::_isCtrlWakeupMaskTriggered(const wchar_t wch) const noexcept
{
    // can't use wil flag set macros here because they require a static scope for the flag
    const ULONG flag = 1 << wch;
    const bool masked = (_ctrlWakeupMask & flag) != 0;

    return (wch < UNICODE_SPACE && masked);
}

// Routine Description:
// - This routine is called to complete a cooked read that blocked in ReadInputBuffer.
// - The context of the read was saved in the CookedReadData structure.
// - This routine is called when events have been written to the input buffer.
// - It is called in the context of the writing thread.
// - It may be called more than once.
// Arguments:
// - TerminationReason - if this routine is called because a ctrl-c or ctrl-break was seen, this argument
//                      contains CtrlC or CtrlBreak. If the owning thread is exiting, it will have ThreadDying. Otherwise 0.
// - fIsUnicode - Whether to convert the final data to A (using Console Input CP) at the end or treat everything as Unicode (UCS-2)
// - pReplyStatus - The status code to return to the client application that originally called the API (before it was queued to wait)
// - pNumBytes - The number of bytes of data that the server/driver will need to transmit back to the client process
// - pControlKeyState - For certain types of reads, this specifies which modifier keys were held.
// - pOutputData - not used
// Return Value:
// - true if the wait is done and result buffer/status code can be sent back to the client.
// - false if we need to continue to wait until more data is available.
bool CookedRead::Notify(const WaitTerminationReason TerminationReason,
                        const bool fIsUnicode,
                        _Out_ NTSTATUS* const pReplyStatus,
                        _Out_ size_t* const pNumBytes,
                        _Out_ DWORD* const pControlKeyState,
                        _Out_ void* const pOutputData)
{
    // TODO
    pOutputData;
    pNumBytes;
    fIsUnicode;


    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // this routine should be called by a thread owning the same
    // lock on the same console as we're reading from.
    FAIL_FAST_IF(!gci.IsConsoleLocked());

    *pNumBytes = 0;
    *pControlKeyState = 0;

    *pReplyStatus = STATUS_SUCCESS;

    FAIL_FAST_IF(_pInputReadHandleData->IsInputPending());

    // this routine should be called by a thread owning the same lock on the same console as we're reading from.
    FAIL_FAST_IF(_pInputReadHandleData->GetReadCount() == 0);

    // if ctrl-c or ctrl-break was seen, terminate read.
    if (WI_IsAnyFlagSet(TerminationReason, (WaitTerminationReason::CtrlC | WaitTerminationReason::CtrlBreak)))
    {
        *pReplyStatus = STATUS_ALERTED;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // See if we were called because the thread that owns this wait block is exiting.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::ThreadDying))
    {
        *pReplyStatus = STATUS_THREAD_IS_TERMINATING;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // We must see if we were woken up because the handle is being closed. If
    // so, we decrement the read count. If it goes to zero, we wake up the
    // close thread. Otherwise, we wake up any other thread waiting for data.

    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::HandleClosing))
    {
        *pReplyStatus = STATUS_ALERTED;
        gci.SetCookedReadData(nullptr);
        return true;
    }

    // TODO insert command history code checking here

    *pReplyStatus = Read(fIsUnicode, *pNumBytes, *pControlKeyState);
    if (*pReplyStatus != CONSOLE_STATUS_WAIT)
    {
        gci.SetCookedReadData(nullptr);
        return true;
    }
    else
    {
        return false;
    }
}

void CookedRead::Hide()
{
    _clearPromptCells();
}

void CookedRead::Show()
{
    _writeToScreen(true);
}

// Routine Description:
// - inserts a ctrl+z character into the prompt at the insertion index
void CookedRead::InsertCtrlZ()
{
    std::deque<wchar_t> chars = { UNICODE_CTRL_Z };
    _writeToPrompt(chars);
    _writeToScreen(true);
}

// Routine Description:
// - Reads characters from user input
// Arguments:
// - isUnicode - TODO
// - numBytes - TODO
// - controlKeyState - TODO
// Return Value:
// - STATUS_SUCCESS if read is finished
// - CONSOLE_STATUS_WAIT if need to wait for more input
// - other status code otherwise
[[nodiscard]]
NTSTATUS CookedRead::Read(const bool /*isUnicode*/,
                          size_t& numBytes,
                          ULONG& controlKeyState) noexcept
{
    controlKeyState = 0;
    std::deque<wchar_t> unprocessedChars;

    while(true)
    {
        switch(_state)
        {
            case ReadState::Ready:
                _readChar(unprocessedChars);
                break;
            case ReadState::GotChar:
                _processChars(unprocessedChars);
                break;
            case ReadState::Wait:
                _wait();
                return _status;
            case ReadState::Complete:
                _complete(numBytes);
                return _status;
            case ReadState::Error:
                _error();
                return _status;
            case ReadState::CommandKey:
                _commandKey();
                break;
        }
    }
}

// Routine Description:
// - handles a command line editing key
void CookedRead::_commandKey()
{
    _status = CommandLine::Instance().ProcessCommandLine(*this, _commandKeyChar, _keyState);
    // TODO check this, it means that a popup was completed but should that mean we should go to the Ready
    // or Complete state?
    if (_status == CONSOLE_STATUS_READ_COMPLETE)
    {
        _state = ReadState::Ready;
    }
    else if (_status == CONSOLE_STATUS_WAIT_NO_BLOCK)
    {
        _status = CONSOLE_STATUS_WAIT;
        _state = ReadState::Wait;
    }
    else if (_status == CONSOLE_STATUS_WAIT)
    {
        _state = ReadState::Wait;
    }
    else if (NT_SUCCESS(_status))
    {
        _state = ReadState::Ready;
    }
    else
    {
        _state = ReadState::Error;
    }
}

// Routine Description:
// - executes wait state
void CookedRead::_wait()
{
    _status = CONSOLE_STATUS_WAIT;
    _state = ReadState::Ready;
}

// Routine Description:
// - executes error state
void CookedRead::_error()
{
    Erase();
    _state = ReadState::Ready;
}

// Routine Description:
// - executes Complete state. copies pompt data to the user buffer.
// Arguments:
// - numBytes - the number of bytes that were copied to the user buffer
void CookedRead::_complete(size_t& numBytes)
{
    // store to history if enter was pressed
    if (_pCommandHistory)
    {
        auto findIt = std::find(_prompt.cbegin(), _prompt.cend(), UNICODE_CARRIAGERETURN);
        if (findIt != _prompt.cend())
        {
            // commands are stored without the /r/n so don't use the full prompt
            const size_t length = std::distance(_prompt.cbegin(), findIt);
            auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            LOG_IF_FAILED(_pCommandHistory->Add({ _prompt.c_str(), length },
                                                WI_IsFlagSet(gci.Flags, CONSOLE_HISTORY_NODUP)));
        }
    }


    std::copy_n(_prompt.begin(), _prompt.size(), _userBuffer);
    numBytes = _prompt.size() * sizeof(wchar_t);
    _status = STATUS_SUCCESS;
    _state = ReadState::Ready;
}

// Routine Description:
// - reads a character from the input buffer
// Arguments:
// - unprocessedChars - any newly read characters will be appended here
void CookedRead::_readChar(std::deque<wchar_t>& unprocessedChars)
{
    wchar_t wch = UNICODE_NULL;
    _commandLineEditingKeys = false;
    _keyState = 0;

    // get a char from the input buffer
    _status = GetChar(_pInputBuffer,
                      &wch,
                      true,
                      &_commandLineEditingKeys,
                      nullptr,
                      &_keyState);

    if (_status == CONSOLE_STATUS_WAIT)
    {
        _state = ReadState::Wait;
    }
    else if (NT_SUCCESS(_status))
    {
        if (_commandLineEditingKeys)
        {
            _commandKeyChar = wch;
            _state = ReadState::CommandKey;
            return;
        }

        // insert char
        unprocessedChars.push_back(wch);

        if (Utf16Parser::IsLeadingSurrogate(wch))
        {
            // a leading surrogate can't be processed by iteslf so try to read another wchar
            _state = ReadState::Ready;
        }
        else
        {
            _state = ReadState::GotChar;
        }
    }
    else
    {
        _state = ReadState::Error;
    }
}

// Routine Description:
// - processes any character data read. updates prompt and screen.
// Arguments:
// - unprocessedChars - character data to process. must not be empty
void CookedRead::_processChars(std::deque<wchar_t>& unprocessedChars)
{
    FAIL_FAST_IF(unprocessedChars.empty());

    _clearPromptCells();

    const bool enterPressed = unprocessedChars.back() == UNICODE_CARRIAGERETURN;

    // carriage return needs to be written at the end of the prompt in order to send all text correctly
    if (enterPressed)
    {
        _insertionIndex = _prompt.size();
    }

    // store character data
    _writeToPrompt(unprocessedChars);

    // check for control character masking
    if (!_prompt.empty() && _isCtrlWakeupMaskTriggered(_prompt.back()))
    {
        _state = ReadState::Complete;
        return;
    }

    // update prompt shown on the screen
    _writeToScreen(!enterPressed);

    if (enterPressed)
    {
        _state = ReadState::Complete;
    }
    else
    {
        _state = ReadState::Ready;
    }
}

// Routine Description:
// - overwrites prompt cells with spaces
void CookedRead::_clearPromptCells()
{
    std::wstring spaces(_calculatePromptCellLength(true), UNICODE_SPACE);

    LOG_IF_FAILED(_screenInfo.SetCursorPosition(_promptStartLocation, true));
    size_t bytesToWrite = spaces.size() * sizeof(wchar_t);
    short scrollY = 0;
    _status = WriteCharsLegacy(_screenInfo,
                               spaces.c_str(),
                               spaces.c_str(),
                               spaces.c_str(),
                               &bytesToWrite,
                               nullptr,
                               _promptStartLocation.X,
                               WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                               &scrollY);

    // move the cursor to the correct insert location
    COORD cursorPosition = _promptStartLocation;
    cursorPosition.X += gsl::narrow<short>(_calculatePromptCellLength(false));
    _status = AdjustCursorPosition(_screenInfo, cursorPosition, true, nullptr);
    FAIL_FAST_IF_NTSTATUS_FAILED(_status);
}

// Routine Description:
// - performs some character transformations and stores the text to the prompt
// Arguments:
// - unprocessedChars - the chars that need to be written to the prompt
void CookedRead::_writeToPrompt(std::deque<wchar_t>& unprocessedChars)
{
    if (unprocessedChars.back() == EXTKEY_ERASE_PREV_WORD || unprocessedChars.back() == UNICODE_BACKSPACE2)
    {
        unprocessedChars.back() = UNICODE_BACKSPACE;
    }
    else if (unprocessedChars.back() == UNICODE_CARRIAGERETURN)
    {
        unprocessedChars.push_back(UNICODE_LINEFEED);
    }

    while (!unprocessedChars.empty())
    {
        const wchar_t wch = unprocessedChars.front();
        unprocessedChars.pop_front();

        if (wch == UNICODE_BACKSPACE)
        {
            _backspace();
        }
        else
        {
            _prompt.insert(_insertionIndex, 1, wch);
            ++_insertionIndex;
        }
    }
}

// Routine Description:
// - handles a backspace character, deleting a codepoint from the left of the insertion point
void CookedRead::_backspace()
{
    // if at the beginning there's nothing to delete
    if (_isInsertionIndexAtPromptBegin())
    {
        return;
    }

    // check if there's a surrogate pair to delete
    if (_insertionIndex >= 2 && _isSurrogatePairAt(_insertionIndex - 2))
    {
        _insertionIndex -= 2;
        _prompt.erase(_insertionIndex, 2);
        return;
    }

    // there isn't a surrogate pair to the left, delete a single char
    --_insertionIndex;
    _prompt.erase(_insertionIndex, 1);
}

// Routine Description:
// - writes the entire prompt data to the screen
// Arguments:
// - resetCursor - true if we need to manually adjust the cursor
void CookedRead::_writeToScreen(const bool resetCursor)
{
    LOG_IF_FAILED(_screenInfo.SetCursorPosition(_promptStartLocation, true));
    size_t bytesToWrite = _prompt.size() * sizeof(wchar_t);
    short scrollY = 0;
    _status = WriteCharsLegacy(_screenInfo,
                               _prompt.c_str(),
                               _prompt.c_str(),
                               _prompt.c_str(),
                               &bytesToWrite,
                               nullptr,
                               _promptStartLocation.X,
                               WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                               &scrollY);

    // move the cursor to the correct insert location
    if (resetCursor)
    {
        COORD cursorPosition = _promptStartLocation;
        cursorPosition.X += gsl::narrow<short>(_calculatePromptCellLength(false));
        _status = AdjustCursorPosition(_screenInfo, cursorPosition, true, nullptr);
        FAIL_FAST_IF_NTSTATUS_FAILED(_status);
    }
}

// Routine Description:
// - checks if the last codepoint in the prompt storage is a UTF16 surrogate pair
// Return Value:
// - true if the last codeponit in the prompt is a surrogate UTF16 pair
bool CookedRead::_isTailSurrogatePair() const
{
    return (_prompt.size() >= 2 &&
            Utf16Parser::IsTrailingSurrogate(_prompt.back()) &&
            Utf16Parser::IsLeadingSurrogate(*(_prompt.crbegin() + 1)));
}

// Routine Description:
// - checks if there is a UTF16 pair that starts at index in the prompt storage
// Arguments:
// - index - the index of the start of the search for a surrogate pair
// Return Value:
// - true if there is a proper surrogate pair starting at index
// Note:
// - will refuse to walk beyond bounds of prompt, make sure that a surrogate pair at index is within the
// bounds of the prompt storage
bool CookedRead::_isSurrogatePairAt(const size_t index) const
{
    return _isSurrogatePairAt({ _prompt.c_str(), _prompt.size() }, index);
}

// Routine Description:
// - checks if there is a UTF16 pair that starts at index
// Arguments:
// - wstrView - the text to search in
// - index - the index of the start of the search for a surrogate pair
// Return Value:
// - true if there is a proper surrogate pair starting at index
// Note:
// - will refuse to walk beyond bounds of wstrView, make sure that a surrogate pair at index is within the
// bounds of the view
bool CookedRead::_isSurrogatePairAt(const std::wstring_view wstrView, const size_t index)
{
    THROW_HR_IF(E_NOT_SUFFICIENT_BUFFER, index + 1 >= wstrView.size());
    return (Utf16Parser::IsLeadingSurrogate(wstrView.at(index)) &&
            Utf16Parser::IsTrailingSurrogate(wstrView.at(index + 1)));
}

// Routine Description:
// - checks if insertion index is at the far left end of the prompt
// Return Value:
// - true if insertion index is at the beginning of the prompt, false otherwise
bool CookedRead::_isInsertionIndexAtPromptBegin()
{
    return _insertionIndex == 0;
}

// Routine Description:
// - checks if insertion index is at the far right end of the prompt
// Return Value:
// - true if insertion index is at the end of the prompt, false otherwise
bool CookedRead::_isInsertionIndexAtPromptEnd()
{
    FAIL_FAST_IF(_insertionIndex > _prompt.size());
    return _insertionIndex == _prompt.size();
}

// Routine Description:
// - calculates how many cells the prompt should take up
// Arguments:
// - wholePrompt - true if the whole prompt text should be calculated. if false, then
// cell length is calculated only up to the insertion index of the prompt.
// Return Value:
// - the number of cells that text data in the prompt should use.
size_t CookedRead::_calculatePromptCellLength(const bool wholePrompt) const
{
    const size_t stopIndex = wholePrompt ? _prompt.size() : _insertionIndex;
    size_t count = 0;
    for (size_t i = 0; i < stopIndex; ++i)
    {
        // check for a potential surrogate pair
        if (i + 1 < stopIndex && _isSurrogatePairAt(i))
        {
            count += IsGlyphFullWidth({ &_prompt.at(i), 2 }) ? 2 : 1;
            // we found a surrogate pair so jump over it
            ++i;
        }
        else
        {
            count += IsGlyphFullWidth(_prompt.at(i)) ? 2 : 1;
        }
    }
    return count;
}
