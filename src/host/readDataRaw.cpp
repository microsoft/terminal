// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "readDataRaw.hpp"
#include "dbcs.h"
#include "stream.h"
#include "../types/inc/GlyphWidth.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

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
    THROW_HR_IF(E_INVALIDARG, _BufferSize == 0);
}

// Routine Description:
// - Destructs a read data class.
// - Decrements count of readers waiting on the given handle.
RAW_READ_DATA::~RAW_READ_DATA() = default;

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

    // If a ctrl-c is seen, don't terminate read. If ctrl-break is seen, terminate read.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::CtrlC))
    {
        return false;
    }

    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::CtrlBreak))
    {
        *pReplyStatus = STATUS_ALERTED;
        return true;
    }

    // See if we were called because the thread that owns this wait block is exiting.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::ThreadDying))
    {
        *pReplyStatus = STATUS_THREAD_IS_TERMINATING;
        return true;
    }

    // We must see if we were woken up because the handle is being
    // closed. If so, we decrement the read count. If it goes to zero,
    // we wake up the close thread. Otherwise, we wake up any other
    // thread waiting for data.
    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::HandleClosing))
    {
        *pReplyStatus = STATUS_ALERTED;
        return true;
    }

    // If we get to here, this routine was called either by the input
    // thread or a write routine. Both of these callers grab the current
    // console lock.

    std::span buffer{ reinterpret_cast<char*>(_BufPtr), _BufferSize };
    *pReplyStatus = ReadCharacterInput(*_pInputBuffer, buffer, *pNumBytes, *_pInputReadHandleData, fIsUnicode);
    return *pReplyStatus != CONSOLE_STATUS_WAIT;
}

void RAW_READ_DATA::MigrateUserBuffersOnTransitionToBackgroundWait(const void* oldBuffer, void* newBuffer)
{
    // See the comment in WaitBlock.cpp for more information.
    if (_BufPtr == static_cast<const wchar_t*>(oldBuffer))
    {
        _BufPtr = static_cast<wchar_t*>(newBuffer);
    }
}
