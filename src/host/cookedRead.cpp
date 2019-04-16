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
    _prompt.erase();
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

size_t CookedRead::VisibleCharCount() const
{
    return std::count_if(_prompt.begin(),
                         _prompt.end(),
                         [](const wchar_t& wch){ return !Utf16Parser::IsTrailingSurrogate(wch); });
}

size_t CookedRead::MoveInsertionIndexLeft()
{
    std::wstring_view glyph;
    bool moved = false;

    // move the insertion index left by one codepoint
    if (_insertionIndex >= 2 &&
        _isSurrogatePairAt(_insertionIndex - 2))
    {
        _insertionIndex -= 2;
        glyph = { &_prompt.at(_insertionIndex), 2 };
        moved = true;
    }
    else if (_insertionIndex > 0)
    {
        --_insertionIndex;
        glyph = { &_prompt.at(_insertionIndex), 1 };
        moved = true;
    }

    // calculate how many cells the cursor was moved by the insertion index move
    size_t cellsMoved = 0;
    if (moved)
    {
        if (IsGlyphFullWidth(glyph))
        {
            cellsMoved = 2;
        }
        else
        {
            cellsMoved = 1;
        }
    }
    return cellsMoved;
}

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

// Routine Description:
// - Reads characters from user input
// Arguments:
// - isUnicode -
// - numBytes -
// - controlKeyState -
// Return Value:
// - STATUS_SUCCESS if read is finished
// - CONSOLE_STATUS_WAIT if need to wait for more input
// - other status code otherwise
[[nodiscard]]
NTSTATUS CookedRead::Read(const bool isUnicode,
                          size_t& numBytes,
                          ULONG& controlKeyState) noexcept
{
    // TODO
    isUnicode;


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
                _process(unprocessedChars);
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
// -
// Arguments:
// -
// Return Value:
// -
void CookedRead::_error()
{
    Erase();
    _state = ReadState::Ready;
}

// Routine Description:
// -
// Arguments:
// -
// Return Value:
// -
void CookedRead::_complete(size_t& numBytes)
{
    std::copy_n(_prompt.begin(), _prompt.size(), _userBuffer);
    numBytes = _prompt.size() * sizeof(wchar_t);
    _status = STATUS_SUCCESS;
    _state = ReadState::Ready;
}

// Routine Description:
// -
// Arguments:
// -
// Return Value:
// -
void CookedRead::_readChar(std::deque<wchar_t>& unprocessedChars)
{
    wchar_t wch = UNICODE_NULL;
    _commandLineEditingKeys = false;
    _keyState = 0;

    _status = GetChar(_pInputBuffer,
                      &wch,
                      true,
                      nullptr,
                      &_commandLineEditingKeys,
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
// -
// Arguments:
// -
// Return Value:
// -
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
        _prompt.insert(_insertionIndex, 1, unprocessedChars.front());
        unprocessedChars.pop_front();
        ++_insertionIndex;
    }
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
        }
        else
        {
            count += IsGlyphFullWidth(_prompt.at(i)) ? 2 : 1;
        }
    }
    return count;
}

// Routine Description:
// -
// Arguments:
// -
// Return Value:
// -
void CookedRead::_process(std::deque<wchar_t>& unprocessedChars)
{
    FAIL_FAST_IF(unprocessedChars.empty());

    const bool enterPressed = unprocessedChars.back() == UNICODE_CARRIAGERETURN;

    // carriage return needs to be written at the end of the prompt in order to send all text correctly
    if (enterPressed)
    {
        _insertionIndex = _prompt.size();
    }

    _writeToPrompt(unprocessedChars);

    if (_isCtrlWakeupMaskTriggered(_prompt.back()))
    {
        _state = ReadState::Complete;
        return;
    }

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
// -
// Arguments:
// -
// Return Value:
// -
bool CookedRead::_isTailSurrogatePair() const
{
    return (_prompt.size() >= 2 &&
            Utf16Parser::IsTrailingSurrogate(_prompt.back()) &&
            Utf16Parser::IsLeadingSurrogate(*(_prompt.crbegin() + 1)));
}

bool CookedRead::_isSurrogatePairAt(const size_t index) const
{
    THROW_HR_IF(E_NOT_SUFFICIENT_BUFFER, index + 1 >= _prompt.size());
    return (Utf16Parser::IsTrailingSurrogate(_prompt.at(index)) &&
            Utf16Parser::IsLeadingSurrogate((_prompt.at(index + 1))));
}
