// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "writeData.hpp"

#include "_stream.h"
#include "..\types\inc\convert.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

// Routine Description:
// - Creates a new write data object for used in servicing write console requests
// Arguments:
// - siContext - The output buffer to write text data to
// - pwchContext - The string information that the client application sent us to be written
// - cbContext - Byte count of the string above
// - uiOutputCodepage - When the wait is completed, we *might* have to convert the byte count
//                      back into a specific codepage if the initial call was an A call.
//                      We need to remember what output codepage was set at the moment in time
//                      when the write was delayed as it might change by the time it is serviced.
// Return Value:
// - THROW: Throws if space cannot be allocated to copy the given string
WriteData::WriteData(SCREEN_INFORMATION& siContext,
                     _In_reads_bytes_(cbContext) wchar_t* const pwchContext,
                     const size_t cbContext,
                     const UINT uiOutputCodepage) :
    IWaitRoutine(ReplyDataType::Write),
    _siContext(siContext),
    _pwchContext(THROW_IF_NULL_ALLOC(reinterpret_cast<wchar_t*>(new byte[cbContext]))),
    _cbContext(cbContext),
    _uiOutputCodepage(uiOutputCodepage),
    _fLeadByteCaptured(false),
    _fLeadByteConsumed(false),
    _cchUtf8Consumed(0)
{
    memmove(_pwchContext, pwchContext, _cbContext);
}

// Routine Description:
// - Destroys the write data object
// - Frees the string copy we made on creation
WriteData::~WriteData()
{
    if (nullptr != _pwchContext)
    {
        delete[] _pwchContext;
    }
}

// Routine Description:
// - Stores some additional information about lead byte adjustments from the conversion
//   in WriteConsoleA before the real WriteConsole processing (always W) is reached
//   so we can restore an accurate A byte count at the very end when the wait is serviced.
// Arguments:
// - fLeadByteCaptured - A lead byte was removed from the string before converted it and saved it.
//                       We need to report to the original caller that we "wrote" the byte
//                       even though it is held in escrow for the next call because it was
//                       the last character in the stream.
// - fLeadByteConsumed - We had a lead byte in escrow from the previous call that we stitched onto the
//                       front of the input string even though the caller didn't write it in this call.
//                       We need to report the byte count back to the caller without including this byte
//                       in the calculation as it wasn't a part of what was given in this exact call.
// Return Value:
// - <none>
void WriteData::SetLeadByteAdjustmentStatus(const bool fLeadByteCaptured,
                                            const bool fLeadByteConsumed)
{
    _fLeadByteCaptured = fLeadByteCaptured;
    _fLeadByteConsumed = fLeadByteConsumed;
}

// Routine Description:
// - For UTF-8 codepages, remembers how many bytes that the UTF-8 parser said it consumed from the input stream.
//   This will allow us to give back the correct value after the wait routine Notify services the data later.
// Arguments:
// - cchUtf8Consumed - Count of characters consumed by the UTF-8 parser off the input stream to generate the
//                     wide character string that is stowed in this object for consumption in the notify routine later.
// Return Value:
// - <none>
void WriteData::SetUtf8ConsumedCharacters(const size_t cchUtf8Consumed)
{
    _cchUtf8Consumed = cchUtf8Consumed;
}

// Routine Description:
// - Called back at a later time to resume the writing operation when the output object becomes unblocked.
// Arguments:
// - TerminationReason - if this routine is called because a ctrl-c or ctrl-break was seen, this argument
//                      contains CtrlC or CtrlBreak. If the owning thread is exiting, it will have ThreadDying. Otherwise 0.
// - fIsUnicode - Input data was in UCS-2 unicode or it needs to be converted with the current Output Codepage
// - pReplyStatus - The status code to return to the client application that originally called the API (before it was queued to wait)
// - pNumBytes - The number of bytes of data that the server/driver will need to transmit back to the client process
// - pControlKeyState - Unused for write operations. Set to 0.
// - pOutputData - not used.
// - true if the wait is done and result buffer/status code can be sent back to the client.
// - false if we need to continue to wait because the output object blocked again
bool WriteData::Notify(const WaitTerminationReason TerminationReason,
                       const bool fIsUnicode,
                       _Out_ NTSTATUS* const pReplyStatus,
                       _Out_ size_t* const pNumBytes,
                       _Out_ DWORD* const pControlKeyState,
                       _Out_ void* const /*pOutputData*/)
{
    *pNumBytes = _cbContext;
    *pControlKeyState = 0;

    if (WI_IsFlagSet(TerminationReason, WaitTerminationReason::ThreadDying))
    {
        *pReplyStatus = STATUS_THREAD_IS_TERMINATING;
        return true;
    }

    // if we get to here, this routine was called by the input
    // thread, which grabs the current console lock.

    // This routine should be called by a thread owning the same lock on the
    // same console as we're reading from.

    FAIL_FAST_IF(!(Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked()));

    std::unique_ptr<WriteData> waiter;
    size_t cbContext = _cbContext;
    NTSTATUS Status = DoWriteConsole(_pwchContext,
                                     &cbContext,
                                     _siContext,
                                     waiter);

    if (Status == CONSOLE_STATUS_WAIT)
    {
        // an extra waiter will be created by DoWriteConsole, but we're already a waiter so discard it.
        waiter.reset();
        return false;
    }

    // There's extra work to do to correct the byte counts if the original call was an A-version call.
    // We always process and hold text in the waiter as W-version text, but the A call is expecting
    // a byte value in its own codepage of how much we have written in that codepage.
    if (!fIsUnicode)
    {
        if (CP_UTF8 != _uiOutputCodepage)
        {
            // At this level with WriteConsole, everything is byte counts, so change back to char counts for
            // GetALengthFromW to work correctly.
            const size_t cchContext = cbContext / sizeof(wchar_t);

            // For non-UTF-8 codepages, we need to back convert the amount consumed and then
            // correlate that with any lead bytes we may have kept for later or reintroduced
            // from previous calls.
            size_t cchTextBufferRead = 0;

            // Start by counting the number of A bytes we used in printing our W string to the screen.
            try
            {
                cchTextBufferRead = GetALengthFromW(_uiOutputCodepage, { _pwchContext, cchContext });
            }
            CATCH_LOG();

            // If we captured a byte off the string this time around up above, it means we didn't feed
            // it into the WriteConsoleW above, and therefore its consumption isn't accounted for
            // in the count we just made. Add +1 to compensate.
            if (_fLeadByteCaptured)
            {
                cchTextBufferRead++;
            }

            // If we consumed an internally-stored lead byte this time around up above, it means that we
            // fed a byte into WriteConsoleW that wasn't a part of this particular call's request.
            // We need to -1 to compensate and tell the caller the right number of bytes consumed this request.
            if (_fLeadByteConsumed)
            {
                cchTextBufferRead--;
            }

            cbContext = cchTextBufferRead;
        }
        else
        {
            // For UTF-8, we were told exactly how many valid bytes were consumed before we got into the wait state.
            // Just give that value back now.
            cbContext = _cchUtf8Consumed;
        }
    }

    *pNumBytes = cbContext;
    *pReplyStatus = Status;
    return true;
}
