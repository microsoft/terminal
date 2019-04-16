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
    _state{ ReadState::Ready }
{
    _prompt.reserve(256);
    _promptStartLocation = _screenInfo.GetTextBuffer().GetCursor().GetPosition();
}

void CookedRead::Erase()
{
    _prompt.erase();
}

void CookedRead::SetInsertMode(const bool mode) noexcept
{
    _insertMode = mode;
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

[[nodiscard]]
NTSTATUS CookedRead::Read(const bool isUnicode,
                          size_t& numBytes,
                          ULONG& controlKeyState) noexcept
{
    // TODO
    isUnicode;


    controlKeyState = 0;

    while(true)
    {
        switch(_state)
        {
            case ReadState::Ready:
                _readChar();
                break;
            case ReadState::GotChar:
                _process();
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
        }
    }
}

void CookedRead::_wait()
{
    _status = CONSOLE_STATUS_WAIT;
    _state = ReadState::Ready;
}

void CookedRead::_error()
{
    Erase();
    _state = ReadState::Ready;
}

void CookedRead::_complete(size_t& numBytes)
{
    numBytes = _prompt.size() * sizeof(wchar_t);
    _status = STATUS_SUCCESS;
    _state = ReadState::Ready;
}

void CookedRead::_readChar()
{
    wchar_t wch = UNICODE_NULL;
    _status = GetChar(_pInputBuffer,
                      &wch,
                      true,
                      nullptr,
                      nullptr,
                      nullptr);

    if (_status == CONSOLE_STATUS_WAIT)
    {
        _state = ReadState::Wait;
    }
    else if (NT_SUCCESS(_status))
    {
        _prompt.push_back(wch);
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

void CookedRead::_process()
{
    if (_prompt.back() == UNICODE_CARRIAGERETURN)
    {
        _prompt.push_back(UNICODE_LINEFEED);
    }

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

    std::copy_n(_prompt.begin(), _prompt.size(), _userBuffer);

    if (_prompt.back() == UNICODE_LINEFEED)
    {
        _state = ReadState::Complete;
    }
    else
    {
        _state = ReadState::Ready;
    }
}

bool CookedRead::_isTailSurrogatePair() const
{
    return (_prompt.size() >= 2 &&
            Utf16Parser::IsTrailingSurrogate(_prompt.back()) &&
            Utf16Parser::IsLeadingSurrogate(*(_prompt.crbegin() + 1)));
}
