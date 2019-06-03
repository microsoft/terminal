// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "readDataRaw.hpp"
#include "dbcs.h"
#include "stream.h"
#include "../types/inc/GlyphWidth.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

// Routine Description:
// - Constructs raw read data class to hold context across sessions
// generally when there's not enough data to return.
// Arguments:
// - pInputBuffer - Buffer that data will be read from.
// - pInputReadHandleData - Context stored across calls from the same
// input handle to return partial data appropriately.
// - BufferSize - The amount of client byte space available for
// returning information.
// - BufPtr - Pointer to the client space available for returning
// information (BufferSize is *2 of this count because it's wchar_t)
// Return Value:
// - THROW: Throws E_INVALIDARG for invalid pointers, if BufferSize is zero or if
// it's not divisible by the size of a wchar
RAW_READ_DATA::RAW_READ_DATA(_In_ InputBuffer* const pInputBuffer,
                             _In_ INPUT_READ_HANDLE_DATA* const pInputReadHandleData,
                             const size_t BufferSize,
                             _In_ WCHAR* const BufPtr) :
    ReadData(pInputBuffer, pInputReadHandleData),
    _BufferSize{ BufferSize },
    _BufPtr{ THROW_HR_IF_NULL(E_INVALIDARG, BufPtr) }
{
    THROW_HR_IF(E_INVALIDARG, _BufferSize % sizeof(wchar_t) != 0);
    THROW_HR_IF(E_INVALIDARG, _BufferSize == 0);
}

// Routine Description:
// - Destructs a read data class.
// - Decrements count of readers waiting on the given handle.
RAW_READ_DATA::~RAW_READ_DATA()
{
}

// Routine Description:
// - This routine is called to complete a raw read that blocked in ReadInputBuffer.
// - The context of the read was saved in the RawReadData structure.
// - This routine is called when events have been written to the input buffer.
// - It is called in the context of the writing thread.
// - It will be called at most once per read.
// Arguments:
// - TerminationReason - if this routine is called because a ctrl-c or
// ctrl-break was seen, this argument contains CtrlC or CtrlBreak. If
// the owning thread is exiting, it will have ThreadDying. Otherwise 0.
// - fIsUnicode - Whether to convert the final data to A (using
// Console Input CP) at the end or treat everything as Unicode (UCS-2)
// - pReplyStatus - The status code to return to the client
// application that originally called the API (before it was queued to
// wait)
// - pNumBytes - The number of bytes of data that the server/driver
// will need to transmit back to the client process
// - pControlKeyState - For certain types of reads, this specifies
// which modifier keys were held.
// - pOutputData - not used
// Return Value:
// - true if the wait is done and result buffer/status code can be
// sent back to the client.
// - false if we need to continue to wait until more data is
// available.
bool RAW_READ_DATA::Notify(const WaitTerminationReason TerminationReason,
                           const bool fIsUnicode,
                           _Out_ NTSTATUS* const pReplyStatus,
                           _Out_ size_t* const pNumBytes,
                           _Out_ DWORD* const pControlKeyState,
                           _Out_ void* const /*pOutputData*/)
{
    // This routine should be called by a thread owning the same lock
    // on the same console as we're reading from.
    FAIL_FAST_IF(_pInputReadHandleData->GetReadCount() == 0);

    FAIL_FAST_IF(!Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    *pReplyStatus = STATUS_SUCCESS;
    *pControlKeyState = 0;

    *pNumBytes = 0;
    size_t NumBytes = 0;

    PWCHAR lpBuffer;
    bool RetVal = true;
    bool fAddDbcsLead = false;
    bool fSkipFinally = false;

    // If a ctrl-c is seen, don't terminate read. If ctrl-break is seen, terminate read.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::CtrlC))
    {
        return false;
    }
    else if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::CtrlBreak))
    {
        *pReplyStatus = STATUS_ALERTED;
    }
    // See if we were called because the thread that owns this wait block is exiting.
    else if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::ThreadDying))
    {
        *pReplyStatus = STATUS_THREAD_IS_TERMINATING;
    }
    // We must see if we were woken up because the handle is being
    // closed. If so, we decrement the read count. If it goes to zero,
    // we wake up the close thread. Otherwise, we wake up any other
    // thread waiting for data.
    else if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::HandleClosing))
    {
        *pReplyStatus = STATUS_ALERTED;
    }
    else
    {
        // If we get to here, this routine was called either by the input
        // thread or a write routine. Both of these callers grab the current
        // console lock.

        lpBuffer = _BufPtr;

        if (!fIsUnicode && _pInputBuffer->IsReadPartialByteSequenceAvailable())
        {
            std::unique_ptr<IInputEvent> event = _pInputBuffer->FetchReadPartialByteSequence(false);
            const KeyEvent* const pKeyEvent = static_cast<const KeyEvent* const>(event.get());
            *lpBuffer = static_cast<char>(pKeyEvent->GetCharData());
            _BufferSize -= sizeof(wchar_t);
            *pReplyStatus = STATUS_SUCCESS;
            fAddDbcsLead = true;

            if (_BufferSize == 0)
            {
                *pNumBytes = 1;
                RetVal = false;
                fSkipFinally = true;
            }
        }
        else
        {
            // This call to GetChar may block.
            *pReplyStatus = GetChar(_pInputBuffer,
                                    lpBuffer,
                                    true,
                                    nullptr,
                                    nullptr,
                                    nullptr);
        }

        if (!NT_SUCCESS(*pReplyStatus) || fSkipFinally)
        {
            if (*pReplyStatus == CONSOLE_STATUS_WAIT)
            {
                RetVal = false;
            }
        }
        else
        {
            NumBytes += IsGlyphFullWidth(*lpBuffer) ? 2 : 1;
            lpBuffer++;
            *pNumBytes += sizeof(WCHAR);
            while (*pNumBytes < _BufferSize)
            {
                // This call to GetChar won't block.
                *pReplyStatus = GetChar(_pInputBuffer,
                                        lpBuffer,
                                        false,
                                        nullptr,
                                        nullptr,
                                        nullptr);
                if (!NT_SUCCESS(*pReplyStatus))
                {
                    *pReplyStatus = STATUS_SUCCESS;
                    break;
                }
                NumBytes += IsGlyphFullWidth(*lpBuffer) ? 2 : 1;
                lpBuffer++;
                *pNumBytes += sizeof(WCHAR);
            }
        }
    }

    // If the read was completed (status != wait), free the raw read data.
    if (*pReplyStatus != CONSOLE_STATUS_WAIT &&
        !fSkipFinally &&
        !fIsUnicode)
    {
        // It's ansi, so translate the string.
        std::unique_ptr<char[]> tempBuffer;
        try
        {
            tempBuffer = std::make_unique<char[]>(NumBytes);
        }
        catch (...)
        {
            return true;
        }

        lpBuffer = _BufPtr;
        std::unique_ptr<IInputEvent> partialEvent;

        *pNumBytes = TranslateUnicodeToOem(lpBuffer,
                                           gsl::narrow<ULONG>(*pNumBytes / sizeof(wchar_t)),
                                           tempBuffer.get(),
                                           gsl::narrow<ULONG>(NumBytes),
                                           partialEvent);
        if (partialEvent.get())
        {
            _pInputBuffer->StoreReadPartialByteSequence(std::move(partialEvent));
        }

        memmove(lpBuffer, tempBuffer.get(), *pNumBytes);
        if (fAddDbcsLead)
        {
            (*pNumBytes)++;
        }
    }
    return RetVal;
}
