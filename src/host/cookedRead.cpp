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
    _insertMode{ ServiceLocator::LocateGlobals().getConsoleInformation().GetInsertMode() }
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

    NTSTATUS Status = _readCharInputLoop(isUnicode);

    // if the read was completed (status != wait), free the cooked read
    // data.  also, close the temporary output handle that was opened to
    // echo the characters read.
    // TODO
    /*
    if (Status != CONSOLE_STATUS_WAIT)
    {
        Status = _handlePostCharInputLoop(isUnicode, numBytes, controlKeyState);
    }
    */

    numBytes = _prompt.size() * sizeof(wchar_t);
    return Status;
}

[[nodiscard]]
NTSTATUS CookedRead::_readCharInputLoop(const bool isUnicode) noexcept
{
    // TODO
    isUnicode;

    NTSTATUS Status = STATUS_SUCCESS;

    while (true)
    {
        wchar_t wch = UNICODE_NULL;

        Status = GetChar(_pInputBuffer,
                         &wch,
                         true,
                         nullptr,
                         nullptr,
                         nullptr);

        if (!NT_SUCCESS(Status))
        {
            if (Status != CONSOLE_STATUS_WAIT)
            {
                Erase();
            }
            break;
        }

        _prompt.push_back(wch);

        if (!Utf16Parser::IsLeadingSurrogate(wch))
        {
            if (_processInput())
            {
                Status = STATUS_SUCCESS;
                break;
            }
        }
    }



    return Status;
}

bool CookedRead::_isTailSurrogatePair() const
{
    return (_prompt.size() >= 2 &&
            Utf16Parser::IsTrailingSurrogate(_prompt.back()) &&
            Utf16Parser::IsLeadingSurrogate(*(_prompt.crbegin() + 1)));
}

bool CookedRead::_processInput()
{
    if (_prompt.back() == UNICODE_CARRIAGERETURN)
    {
        _prompt.push_back(UNICODE_LINEFEED);
    }

    LOG_IF_FAILED(_screenInfo.SetCursorPosition(_promptStartLocation, true));
    size_t bytesToWrite = _prompt.size() * sizeof(wchar_t);
    short scrollY = 0;
    NTSTATUS Status = WriteCharsLegacy(_screenInfo,
                                       _prompt.c_str(),
                                       _prompt.c_str(),
                                       _prompt.c_str(),
                                       &bytesToWrite,
                                       nullptr,
                                       _promptStartLocation.X,
                                       WC_DESTRUCTIVE_BACKSPACE | WC_KEEP_CURSOR_VISIBLE | WC_ECHO,
                                       &scrollY);

    Status;

    std::copy_n(_prompt.begin(), _prompt.size(), _userBuffer);

    if (_prompt.back() == UNICODE_LINEFEED)
    {
        return true;
    }
    else
    {
        return false;
    }


}
