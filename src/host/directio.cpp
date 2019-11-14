// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "directio.h"

#include "_output.h"
#include "output.h"
#include "input.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "readDataDirect.hpp"
#include "ApiRoutines.h"

#include "../types/inc/convert.hpp"
#include "../types/inc/GlyphWidth.hpp"
#include "../types/inc/viewport.hpp"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;

class CONSOLE_INFORMATION;

#define UNICODE_DBCS_PADDING 0xffff

// Routine Description:
// - converts non-unicode InputEvents to unicode InputEvents
// Arguments:
// inEvents - InputEvents to convert
// partialEvent - on output, will contain a partial dbcs byte char
// data if the last event in inEvents is a dbcs lead byte
// Return Value:
// - inEvents will contain unicode InputEvents
// - partialEvent may contain a partial dbcs KeyEvent
void EventsToUnicode(_Inout_ std::deque<std::unique_ptr<IInputEvent>>& inEvents,
                     _Out_ std::unique_ptr<IInputEvent>& partialEvent)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    std::deque<std::unique_ptr<IInputEvent>> outEvents;

    while (!inEvents.empty())
    {
        std::unique_ptr<IInputEvent> currentEvent = std::move(inEvents.front());
        inEvents.pop_front();

        if (currentEvent->EventType() != InputEventType::KeyEvent)
        {
            outEvents.push_back(std::move(currentEvent));
        }
        else
        {
            const KeyEvent* const keyEvent = static_cast<const KeyEvent* const>(currentEvent.get());

            std::wstring outWChar;
            HRESULT hr = S_OK;

            // convert char data to unicode
            if (IsDBCSLeadByteConsole(static_cast<char>(keyEvent->GetCharData()), &gci.CPInfo))
            {
                if (inEvents.empty())
                {
                    // we ran out of data and have a partial byte leftover
                    partialEvent = std::move(currentEvent);
                    break;
                }

                // get the 2nd byte and convert to unicode
                const KeyEvent* const keyEventEndByte = static_cast<const KeyEvent* const>(inEvents.front().get());
                inEvents.pop_front();

                char inBytes[] = {
                    static_cast<char>(keyEvent->GetCharData()),
                    static_cast<char>(keyEventEndByte->GetCharData())
                };
                try
                {
                    outWChar = ConvertToW(gci.CP, { inBytes, ARRAYSIZE(inBytes) });
                }
                catch (...)
                {
                    hr = wil::ResultFromCaughtException();
                }
            }
            else
            {
                char inBytes[] = {
                    static_cast<char>(keyEvent->GetCharData())
                };
                try
                {
                    outWChar = ConvertToW(gci.CP, { inBytes, ARRAYSIZE(inBytes) });
                }
                catch (...)
                {
                    hr = wil::ResultFromCaughtException();
                }
            }

            // push unicode key events back out
            if (SUCCEEDED(hr) && outWChar.size() > 0)
            {
                KeyEvent unicodeKeyEvent = *keyEvent;
                for (const auto wch : outWChar)
                {
                    try
                    {
                        unicodeKeyEvent.SetCharData(wch);
                        outEvents.push_back(std::make_unique<KeyEvent>(unicodeKeyEvent));
                    }
                    catch (...)
                    {
                        LOG_HR(wil::ResultFromCaughtException());
                    }
                }
            }
        }
    }

    inEvents.swap(outEvents);
    return;
}

// Routine Description:
// - This routine reads or peeks input events.  In both cases, the events
//   are copied to the user's buffer.  In the read case they are removed
//   from the input buffer and in the peek case they are not.
// Arguments:
// - pInputBuffer - The input buffer to take records from to return to the client
// - outEvents - The storage location to fill with input events
// - eventReadCount - The number of events to read
// - pInputReadHandleData - A structure that will help us maintain
// some input context across various calls on the same input
// handle. Primarily used to restore the "other piece" of partially
// returned strings (because client buffer wasn't big enough) on the
// next call.
// - IsUnicode - Whether to operate on Unicode characters or convert
// on the current Input Codepage.
// - IsPeek - If this is a peek operation (a.k.a. do not remove
// characters from the input buffer while copying to client buffer.)
// - ppWaiter - If we have to wait (not enough data to fill client
// buffer), this contains context that will allow the server to
// restore this call later.
// Return Value:
// - STATUS_SUCCESS - If data was found and ready for return to the client.
// - CONSOLE_STATUS_WAIT - If we didn't have enough data or needed to
// block, this will be returned along with context in *ppWaiter.
// - Or an out of memory/math/string error message in NTSTATUS format.
[[nodiscard]] static NTSTATUS _DoGetConsoleInput(InputBuffer& inputBuffer,
                                                 std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                 const size_t eventReadCount,
                                                 INPUT_READ_HANDLE_DATA& readHandleState,
                                                 const bool IsUnicode,
                                                 const bool IsPeek,
                                                 std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        waiter.reset();

        if (eventReadCount == 0)
        {
            return STATUS_SUCCESS;
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        std::deque<std::unique_ptr<IInputEvent>> partialEvents;
        if (!IsUnicode)
        {
            if (inputBuffer.IsReadPartialByteSequenceAvailable())
            {
                partialEvents.push_back(inputBuffer.FetchReadPartialByteSequence(IsPeek));
            }
        }

        size_t amountToRead;
        if (FAILED(SizeTSub(eventReadCount, partialEvents.size(), &amountToRead)))
        {
            return STATUS_INTEGER_OVERFLOW;
        }
        std::deque<std::unique_ptr<IInputEvent>> readEvents;
        NTSTATUS Status = inputBuffer.Read(readEvents,
                                           amountToRead,
                                           IsPeek,
                                           true,
                                           IsUnicode,
                                           false);

        if (CONSOLE_STATUS_WAIT == Status)
        {
            FAIL_FAST_IF(!(readEvents.empty()));
            // If we're told to wait until later, move all of our context
            // to the read data object and send it back up to the server.
            waiter = std::make_unique<DirectReadData>(&inputBuffer,
                                                      &readHandleState,
                                                      eventReadCount,
                                                      std::move(partialEvents));
        }
        else if (NT_SUCCESS(Status))
        {
            // split key events to oem chars if necessary
            if (!IsUnicode)
            {
                try
                {
                    SplitToOem(readEvents);
                }
                CATCH_LOG();
            }

            // combine partial and readEvents
            while (!partialEvents.empty())
            {
                readEvents.push_front(std::move(partialEvents.back()));
                partialEvents.pop_back();
            }

            // move events over
            for (size_t i = 0; i < eventReadCount; ++i)
            {
                if (readEvents.empty())
                {
                    break;
                }
                outEvents.push_back(std::move(readEvents.front()));
                readEvents.pop_front();
            }

            // store partial event if necessary
            if (!readEvents.empty())
            {
                inputBuffer.StoreReadPartialByteSequence(std::move(readEvents.front()));
                readEvents.pop_front();
                FAIL_FAST_IF(!(readEvents.empty()));
            }
        }
        return Status;
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }
}

// Routine Description:
// - Retrieves input records from the given input object and returns them to the client.
// - The peek version will NOT remove records when it copies them out.
// - The A version will convert to W using the console's current Input codepage (see SetConsoleCP)
// Arguments:
// - context - The input buffer to take records from to return to the client
// - outEvents - storage location for read events
// - eventsToRead - The number of input events to read
// - readHandleState - A structure that will help us maintain
// some input context across various calls on the same input
// handle. Primarily used to restore the "other piece" of partially
// returned strings (because client buffer wasn't big enough) on the
// next call.
// - waiter - If we have to wait (not enough data to fill client
// buffer), this contains context that will allow the server to
// restore this call later.
[[nodiscard]] HRESULT ApiRoutines::PeekConsoleInputAImpl(IConsoleInputObject& context,
                                                         std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                         const size_t eventsToRead,
                                                         INPUT_READ_HANDLE_DATA& readHandleState,
                                                         std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        NTSTATUS Status = _DoGetConsoleInput(context,
                                             outEvents,
                                             eventsToRead,
                                             readHandleState,
                                             false,
                                             true,
                                             waiter);
        if (CONSOLE_STATUS_WAIT == Status)
        {
            return HRESULT_FROM_NT(Status);
        }
        RETURN_NTSTATUS(Status);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves input records from the given input object and returns them to the client.
// - The peek version will NOT remove records when it copies them out.
// - The W version accepts UCS-2 formatted characters (wide characters)
// Arguments:
// - context - The input buffer to take records from to return to the client
// - outEvents - storage location for read events
// - eventsToRead - The number of input events to read
// - readHandleState - A structure that will help us maintain
// some input context across various calls on the same input
// handle. Primarily used to restore the "other piece" of partially
// returned strings (because client buffer wasn't big enough) on the
// next call.
// - waiter - If we have to wait (not enough data to fill client
// buffer), this contains context that will allow the server to
// restore this call later.
[[nodiscard]] HRESULT ApiRoutines::PeekConsoleInputWImpl(IConsoleInputObject& context,
                                                         std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                         const size_t eventsToRead,
                                                         INPUT_READ_HANDLE_DATA& readHandleState,
                                                         std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        NTSTATUS Status = _DoGetConsoleInput(context,
                                             outEvents,
                                             eventsToRead,
                                             readHandleState,
                                             true,
                                             true,
                                             waiter);
        if (CONSOLE_STATUS_WAIT == Status)
        {
            return HRESULT_FROM_NT(Status);
        }
        RETURN_NTSTATUS(Status);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves input records from the given input object and returns them to the client.
// - The read version WILL remove records when it copies them out.
// - The A version will convert to W using the console's current Input codepage (see SetConsoleCP)
// Arguments:
// - context - The input buffer to take records from to return to the client
// - outEvents - storage location for read events
// - eventsToRead - The number of input events to read
// - readHandleState - A structure that will help us maintain
// some input context across various calls on the same input
// handle. Primarily used to restore the "other piece" of partially
// returned strings (because client buffer wasn't big enough) on the
// next call.
// - waiter - If we have to wait (not enough data to fill client
// buffer), this contains context that will allow the server to
// restore this call later.
[[nodiscard]] HRESULT ApiRoutines::ReadConsoleInputAImpl(IConsoleInputObject& context,
                                                         std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                         const size_t eventsToRead,
                                                         INPUT_READ_HANDLE_DATA& readHandleState,
                                                         std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        NTSTATUS Status = _DoGetConsoleInput(context,
                                             outEvents,
                                             eventsToRead,
                                             readHandleState,
                                             false,
                                             false,
                                             waiter);
        if (CONSOLE_STATUS_WAIT == Status)
        {
            return HRESULT_FROM_NT(Status);
        }
        RETURN_NTSTATUS(Status);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves input records from the given input object and returns them to the client.
// - The read version WILL remove records when it copies them out.
// - The W version accepts UCS-2 formatted characters (wide characters)
// Arguments:
// - context - The input buffer to take records from to return to the client
// - outEvents - storage location for read events
// - eventsToRead - The number of input events to read
// - readHandleState - A structure that will help us maintain
// some input context across various calls on the same input
// handle. Primarily used to restore the "other piece" of partially
// returned strings (because client buffer wasn't big enough) on the
// next call.
// - waiter - If we have to wait (not enough data to fill client
// buffer), this contains context that will allow the server to
// restore this call later.
[[nodiscard]] HRESULT ApiRoutines::ReadConsoleInputWImpl(IConsoleInputObject& context,
                                                         std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                         const size_t eventsToRead,
                                                         INPUT_READ_HANDLE_DATA& readHandleState,
                                                         std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    try
    {
        NTSTATUS Status = _DoGetConsoleInput(context,
                                             outEvents,
                                             eventsToRead,
                                             readHandleState,
                                             true,
                                             false,
                                             waiter);
        if (CONSOLE_STATUS_WAIT == Status)
        {
            return HRESULT_FROM_NT(Status);
        }
        RETURN_NTSTATUS(Status);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes events to the input buffer
// Arguments:
// - context - the input buffer to write to
// - events - the events to written
// - written  - on output, the number of events written
// - append - true if events should be written to the end of the input
// buffer, false if they should be written to the front
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] static HRESULT _WriteConsoleInputWImplHelper(InputBuffer& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& events,
                                                           size_t& written,
                                                           const bool append) noexcept
{
    try
    {
        written = 0;

        // add to InputBuffer
        if (append)
        {
            written = context.Write(events);
        }
        else
        {
            written = context.Prepend(events);
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes events to the input buffer already formed into IInputEvents (private call)
// Arguments:
// - context - the input buffer to write to
// - events - the events to written
// - written  - on output, the number of events written
// - append - true if events should be written to the end of the input
// buffer, false if they should be written to the front
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] HRESULT DoSrvPrivateWriteConsoleInputW(_Inout_ InputBuffer* const pInputBuffer,
                                                     _Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                     _Out_ size_t& eventsWritten,
                                                     const bool append) noexcept
{
    return _WriteConsoleInputWImplHelper(*pInputBuffer, events, eventsWritten, append);
}

// Routine Description:
// - Writes events to the input buffer, translating from codepage to unicode first
// Arguments:
// - context - the input buffer to write to
// - buffer - the events to written
// - written  - on output, the number of events written
// - append - true if events should be written to the end of the input
// buffer, false if they should be written to the front
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleInputAImpl(InputBuffer& context,
                                                          const std::basic_string_view<INPUT_RECORD> buffer,
                                                          size_t& written,
                                                          const bool append) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        auto events = IInputEvent::Create(buffer);

        // add partial byte event if necessary
        if (context.IsWritePartialByteSequenceAvailable())
        {
            events.push_front(context.FetchWritePartialByteSequence(false));
        }

        // convert to unicode if necessary
        std::unique_ptr<IInputEvent> partialEvent;
        EventsToUnicode(events, partialEvent);

        if (partialEvent.get())
        {
            context.StoreWritePartialByteSequence(std::move(partialEvent));
        }

        return _WriteConsoleInputWImplHelper(context, events, written, append);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes events to the input buffer
// Arguments:
// - context - the input buffer to write to
// - buffer - the events to written
// - written  - on output, the number of events written
// - append - true if events should be written to the end of the input
// buffer, false if they should be written to the front
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleInputWImpl(InputBuffer& context,
                                                          const std::basic_string_view<INPUT_RECORD> buffer,
                                                          size_t& written,
                                                          const bool append) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        auto events = IInputEvent::Create(buffer);

        return _WriteConsoleInputWImplHelper(context, events, written, append);
    }
    CATCH_RETURN();
}

// Function Description:
// - Writes the input records to the beginning of the input buffer. This is used
//      by VT sequences that need a response immediately written back to the
//      input.
// Arguments:
// - pInputBuffer - the input buffer to write to
// - events - the events to written
// - eventsWritten - on output, the number of events written
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] HRESULT DoSrvPrivatePrependConsoleInput(_Inout_ InputBuffer* const pInputBuffer,
                                                      _Inout_ std::deque<std::unique_ptr<IInputEvent>>& events,
                                                      _Out_ size_t& eventsWritten)
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    eventsWritten = 0;

    try
    {
        // add partial byte event if necessary
        if (pInputBuffer->IsWritePartialByteSequenceAvailable())
        {
            events.push_front(pInputBuffer->FetchWritePartialByteSequence(false));
        }
    }
    CATCH_RETURN();

    // add to InputBuffer
    eventsWritten = pInputBuffer->Prepend(events);

    return S_OK;
}

// Function Description:
// - Writes the input KeyEvent to the console as a console control event. This
//      can be used for potentially generating Ctrl-C events, as
//      HandleGenericKeyEvent will correctly generate the Ctrl-C response in
//      the same way that it'd be handled from the window proc, with the proper
//      processed vs raw input handling.
//  If the input key is *not* a Ctrl-C key, then it will get written to the
//      buffer just the same as any other KeyEvent.
// Arguments:
// - pInputBuffer - the input buffer to write to. Currently unused, as
//      HandleGenericKeyEvent just gets the global input buffer, but all
//      ConGetSet API's require an input or output object.
// - key - The keyevent to send to the console.
// Return Value:
// - HRESULT indicating success or failure
[[nodiscard]] HRESULT DoSrvPrivateWriteConsoleControlInput(_Inout_ InputBuffer* const /*pInputBuffer*/,
                                                           _In_ KeyEvent key)
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    HandleGenericKeyEvent(key, false);

    return S_OK;
}

// Routine Description:
// - This is used when the app is reading output as cells and needs them converted
//   into a particular codepage on the way out.
// Arguments:
// - codepage - The relevant codepage for translation
// - buffer - This is the buffer containing all of the character data to be converted
// - rectangle - This is the rectangle describing the region that the buffer covers.
// Return Value:
// - Generally S_OK. Could be a memory or math error code.
[[nodiscard]] static HRESULT _ConvertCellsToAInplace(const UINT codepage,
                                                     const gsl::span<CHAR_INFO> buffer,
                                                     const Viewport rectangle) noexcept
{
    try
    {
        std::vector<CHAR_INFO> tempBuffer(buffer.cbegin(), buffer.cend());

        const auto size = rectangle.Dimensions();
        auto tempIter = tempBuffer.cbegin();
        auto outIter = buffer.begin();

        for (int i = 0; i < size.Y; i++)
        {
            for (int j = 0; j < size.X; j++)
            {
                // Any time we see the lead flag, we presume there will be a trailing one following it.
                // Giving us two bytes of space (one per cell in the ascii part of the character union)
                // to fill with whatever this Unicode character converts into.
                if (WI_IsFlagSet(tempIter->Attributes, COMMON_LVB_LEADING_BYTE))
                {
                    // As long as we're not looking at the exact last column of the buffer...
                    if (j < size.X - 1)
                    {
                        // Walk forward one because we're about to consume two cells.
                        j++;

                        // Try to convert the unicode character (2 bytes) in the leading cell to the codepage.
                        CHAR AsciiDbcs[2] = { 0 };
                        UINT NumBytes = gsl::narrow<UINT>(sizeof(AsciiDbcs));
                        NumBytes = ConvertToOem(codepage, &tempIter->Char.UnicodeChar, 1, &AsciiDbcs[0], NumBytes);

                        // Fill the 1 byte (AsciiChar) portion of the leading and trailing cells with each of the bytes returned.
                        outIter->Char.AsciiChar = AsciiDbcs[0];
                        outIter->Attributes = tempIter->Attributes;
                        outIter++;
                        tempIter++;
                        outIter->Char.AsciiChar = AsciiDbcs[1];
                        outIter->Attributes = tempIter->Attributes;
                        outIter++;
                        tempIter++;
                    }
                    else
                    {
                        // When we're in the last column with only a leading byte, we can't return that without a trailing.
                        // Instead, replace the output data with just a space and clear all flags.
                        outIter->Char.AsciiChar = UNICODE_SPACE;
                        outIter->Attributes = tempIter->Attributes;
                        WI_ClearAllFlags(outIter->Attributes, COMMON_LVB_SBCSDBCS);
                        outIter++;
                        tempIter++;
                    }
                }
                else if (WI_AreAllFlagsClear(tempIter->Attributes, COMMON_LVB_SBCSDBCS))
                {
                    // If there are no leading/trailing pair flags, then we only have 1 ascii byte to try to fit the
                    // 2 byte UTF-16 character into. Give it a go.
                    ConvertToOem(codepage, &tempIter->Char.UnicodeChar, 1, &outIter->Char.AsciiChar, 1);
                    outIter->Attributes = tempIter->Attributes;
                    outIter++;
                    tempIter++;
                }
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - This is used when the app writes oem to the output buffer we want
//   UnicodeOem or Unicode in the buffer, depending on font
// Arguments:
// - codepage - The relevant codepage for translation
// - buffer - This is the buffer containing all of the character data to be converted
// - rectangle - This is the rectangle describing the region that the buffer covers.
// Return Value:
// - Generally S_OK. Could be a memory or math error code.
[[nodiscard]] static HRESULT _ConvertCellsToWInplace(const UINT codepage,
                                                     gsl::span<CHAR_INFO> buffer,
                                                     const Viewport& rectangle) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        const auto size = rectangle.Dimensions();
        auto outIter = buffer.begin();

        for (int i = 0; i < size.Y; i++)
        {
            for (int j = 0; j < size.X; j++)
            {
                // Clear lead/trailing flags. We'll determine it for ourselves versus the given codepage.
                WI_ClearAllFlags(outIter->Attributes, COMMON_LVB_SBCSDBCS);

                // If the 1 byte given is a lead in this codepage, we likely need two cells for the width.
                if (IsDBCSLeadByteConsole(outIter->Char.AsciiChar, &gci.OutputCPInfo))
                {
                    // If we're not on the last column, we have two cells to use.
                    if (j < size.X - 1)
                    {
                        // Mark we're consuming two cells.
                        j++;

                        // Grab the lead/trailing byte pair from this cell and the next one forward.
                        CHAR AsciiDbcs[2];
                        AsciiDbcs[0] = outIter->Char.AsciiChar;
                        AsciiDbcs[1] = (outIter + 1)->Char.AsciiChar;

                        // Convert it to UTF-16.
                        WCHAR UnicodeDbcs[2];
                        ConvertOutputToUnicode(codepage, &AsciiDbcs[0], 2, &UnicodeDbcs[0], 2);

                        // Store the actual character in the first available position.
                        outIter->Char.UnicodeChar = UnicodeDbcs[0];
                        WI_ClearAllFlags(outIter->Attributes, COMMON_LVB_SBCSDBCS);
                        WI_SetFlag(outIter->Attributes, COMMON_LVB_LEADING_BYTE);
                        outIter++;

                        // Put a padding character in the second position.
                        outIter->Char.UnicodeChar = UNICODE_DBCS_PADDING;
                        WI_ClearAllFlags(outIter->Attributes, COMMON_LVB_SBCSDBCS);
                        WI_SetFlag(outIter->Attributes, COMMON_LVB_TRAILING_BYTE);
                        outIter++;
                    }
                    else
                    {
                        // If we were on the last column, put in a space.
                        outIter->Char.UnicodeChar = UNICODE_SPACE;
                        WI_ClearAllFlags(outIter->Attributes, COMMON_LVB_SBCSDBCS);
                        outIter++;
                    }
                }
                else
                {
                    // If it's not detected as a lead byte of a pair, then just convert it in place and move on.
                    CHAR c = outIter->Char.AsciiChar;

                    ConvertOutputToUnicode(codepage, &c, 1, &outIter->Char.UnicodeChar, 1);
                    outIter++;
                }
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] static std::vector<CHAR_INFO> _ConvertCellsToMungedW(gsl::span<CHAR_INFO> buffer, const Viewport& rectangle)
{
    std::vector<CHAR_INFO> result;
    result.reserve(buffer.size() * 2); // we estimate we'll need up to double the cells if they all expand.

    const auto size = rectangle.Dimensions();
    auto bufferIter = buffer.cbegin();

    for (SHORT i = 0; i < size.Y; i++)
    {
        for (SHORT j = 0; j < size.X; j++)
        {
            // Prepare a candidate charinfo on the output side copying the colors but not the lead/trail information.
            CHAR_INFO candidate;
            candidate.Attributes = bufferIter->Attributes;
            WI_ClearAllFlags(candidate.Attributes, COMMON_LVB_SBCSDBCS);

            // If the glyph we're given is full width, it needs to take two cells.
            if (IsGlyphFullWidth(bufferIter->Char.UnicodeChar))
            {
                // If we're not on the final cell of the row...
                if (j < size.X - 1)
                {
                    // Mark that we're consuming two cells.
                    j++;

                    // Fill one cell with a copy of the color and character marked leading
                    candidate.Char.UnicodeChar = bufferIter->Char.UnicodeChar;
                    WI_SetFlag(candidate.Attributes, COMMON_LVB_LEADING_BYTE);
                    result.push_back(candidate);

                    // Fill a second cell with a copy of the color marked trailing and a padding character.
                    candidate.Char.UnicodeChar = UNICODE_DBCS_PADDING;
                    candidate.Attributes = bufferIter->Attributes;
                    WI_ClearAllFlags(candidate.Attributes, COMMON_LVB_SBCSDBCS);
                    WI_SetFlag(candidate.Attributes, COMMON_LVB_TRAILING_BYTE);
                }
                else
                {
                    // If we're on the final cell, this won't fit. Replace with a space.
                    candidate.Char.UnicodeChar = UNICODE_SPACE;
                }
            }
            else
            {
                // If we're not full-width, we're half-width. Just copy the character over.
                candidate.Char.UnicodeChar = bufferIter->Char.UnicodeChar;
            }

            // Push our candidate in.
            result.push_back(candidate);

            // Advance to read the next item.
            bufferIter++;
        }
    }
    return result;
}

[[nodiscard]] static HRESULT _ReadConsoleOutputWImplHelper(const SCREEN_INFORMATION& context,
                                                           gsl::span<CHAR_INFO> targetBuffer,
                                                           const Microsoft::Console::Types::Viewport& requestRectangle,
                                                           Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& storageBuffer = context.GetActiveBuffer();
        const auto storageSize = storageBuffer.GetBufferSize().Dimensions();

        const auto targetSize = requestRectangle.Dimensions();

        // If either dimension of the request is too small, return an empty rectangle as read and exit early.
        if (targetSize.X <= 0 || targetSize.Y <= 0)
        {
            readRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        // The buffer given should be big enough to hold the dimensions of the request.
        ptrdiff_t targetArea;
        RETURN_IF_FAILED(PtrdiffTMult(targetSize.X, targetSize.Y, &targetArea));
        RETURN_HR_IF(E_INVALIDARG, targetArea < 0);
        RETURN_HR_IF(E_INVALIDARG, targetArea < targetBuffer.size());

        // Clip the request rectangle to the size of the storage buffer
        SMALL_RECT clip = requestRectangle.ToExclusive();
        clip.Right = std::min(clip.Right, storageSize.X);
        clip.Bottom = std::min(clip.Bottom, storageSize.Y);

        // Find the target point (where to write the user's buffer)
        // It will either be 0,0 or offset into the buffer by the inverse of the negative values.
        COORD targetPoint;
        targetPoint.X = clip.Left < 0 ? -clip.Left : 0;
        targetPoint.Y = clip.Top < 0 ? -clip.Top : 0;

        // The clipped rect must be inside the buffer size, so it has a minimum value of 0. (max of itself and 0)
        clip.Left = std::max(clip.Left, 0i16);
        clip.Top = std::max(clip.Top, 0i16);

        // The final "request rectangle" or the area inside the buffer we want to read, is the clipped dimensions.
        const auto clippedRequestRectangle = Viewport::FromExclusive(clip);

        // We will start reading the buffer at the point of the top left corner (origin) of the (potentially adjusted) request
        const auto sourcePoint = clippedRequestRectangle.Origin();

        // Get an iterator to the beginning of the return buffer
        // We might have to seek this forward or skip around if we clipped the request.
        auto targetIter = targetBuffer.begin();
        COORD targetPos = { 0 };
        const auto targetLimit = Viewport::FromDimensions(targetPoint, clippedRequestRectangle.Dimensions());

        // Get an iterator to the beginning of the request inside the screen buffer
        // This should walk exactly along every cell of the clipped request.
        auto sourceIter = storageBuffer.GetCellDataAt(sourcePoint, clippedRequestRectangle);

        // Walk through every cell of the target, advancing the buffer.
        // Validate that we always still have a valid iterator to the backing store,
        // that we always are writing inside the user's buffer (before the end)
        // and we're always targeting the user's buffer inside its original bounds.
        while (sourceIter && targetIter < targetBuffer.end())
        {
            // If the point we're trying to write is inside the limited buffer write zone...
            if (targetLimit.IsInBounds(targetPos))
            {
                // Copy the data into position...
                *targetIter = gci.AsCharInfo(*sourceIter);
                // ... and advance the read iterator.
                sourceIter++;
            }

            // Always advance the write iterator, we might have skipped it due to clipping.
            targetIter++;

            // Increment the target
            targetPos.X++;
            if (targetPos.X >= targetSize.X)
            {
                targetPos.X = 0;
                targetPos.Y++;
            }
        }

        // Reply with the region we read out of the backing buffer (potentially clipped)
        readRectangle = clippedRequestRectangle;

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                          gsl::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                          Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto codepage = gci.OutputCP;

        RETURN_IF_FAILED(_ReadConsoleOutputWImplHelper(context, buffer, sourceRectangle, readRectangle));

        LOG_IF_FAILED(_ConvertCellsToAInplace(codepage, buffer, readRectangle));

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                          gsl::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                          Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        RETURN_IF_FAILED(_ReadConsoleOutputWImplHelper(context, buffer, sourceRectangle, readRectangle));

        if (!context.GetActiveBuffer().GetCurrentFont().IsTrueTypeFont())
        {
            // For compatibility reasons, we must maintain the behavior that munges the data if we are writing while a raster font is enabled.
            // This can be removed when raster font support is removed.
            UnicodeRasterFontCellMungeOnRead(buffer);
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] static HRESULT _WriteConsoleOutputWImplHelper(SCREEN_INFORMATION& context,
                                                            gsl::span<CHAR_INFO> buffer,
                                                            const Viewport& requestRectangle,
                                                            Viewport& writtenRectangle) noexcept
{
    try
    {
        auto& storageBuffer = context.GetActiveBuffer();
        const auto storageRectangle = storageBuffer.GetBufferSize();
        const auto storageSize = storageRectangle.Dimensions();

        const auto sourceSize = requestRectangle.Dimensions();

        // If either dimension of the request is too small, return an empty rectangle as the read and exit early.
        if (sourceSize.X <= 0 || sourceSize.Y <= 0)
        {
            writtenRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        // If the top and left of the destination we're trying to write it outside the buffer,
        // give the original request rectangle back and exit early OK.
        if (requestRectangle.Left() >= storageSize.X || requestRectangle.Top() >= storageSize.Y)
        {
            writtenRectangle = requestRectangle;
            return S_OK;
        }

        // Do clipping according to the legacy patterns.
        SMALL_RECT writeRegion = requestRectangle.ToInclusive();
        SMALL_RECT sourceRect;
        if (writeRegion.Right > storageSize.X - 1)
        {
            writeRegion.Right = storageSize.X - 1;
        }
        sourceRect.Right = writeRegion.Right - writeRegion.Left;
        if (writeRegion.Bottom > storageSize.Y - 1)
        {
            writeRegion.Bottom = storageSize.Y - 1;
        }
        sourceRect.Bottom = writeRegion.Bottom - writeRegion.Top;

        if (writeRegion.Left < 0)
        {
            sourceRect.Left = -writeRegion.Left;
            writeRegion.Left = 0;
        }
        else
        {
            sourceRect.Left = 0;
        }

        if (writeRegion.Top < 0)
        {
            sourceRect.Top = -writeRegion.Top;
            writeRegion.Top = 0;
        }
        else
        {
            sourceRect.Top = 0;
        }

        if (sourceRect.Left > sourceRect.Right || sourceRect.Top > sourceRect.Bottom)
        {
            return E_INVALIDARG;
        }

        const auto writeRectangle = Viewport::FromInclusive(writeRegion);

        auto target = writeRectangle.Origin();

        // For every row in the request, create a view into the clamped portion of just the one line to write.
        // This allows us to restrict the width of the call without allocating/copying any memory by just making
        // a smaller view over the existing big blob of data from the original call.
        for (; target.Y < writeRectangle.BottomExclusive(); target.Y++)
        {
            // We find the offset into the original buffer by the dimensions of the original request rectangle.
            ptrdiff_t rowOffset = 0;
            RETURN_IF_FAILED(PtrdiffTSub(target.Y, requestRectangle.Top(), &rowOffset));
            RETURN_IF_FAILED(PtrdiffTMult(rowOffset, requestRectangle.Width(), &rowOffset));

            ptrdiff_t colOffset = 0;
            RETURN_IF_FAILED(PtrdiffTSub(target.X, requestRectangle.Left(), &colOffset));

            ptrdiff_t totalOffset = 0;
            RETURN_IF_FAILED(PtrdiffTAdd(rowOffset, colOffset, &totalOffset));

            // Now we make a subspan starting from that offset for as much of the original request as would fit
            const auto subspan = buffer.subspan(totalOffset, writeRectangle.Width());

            // Convert to a CHAR_INFO view to fit into the iterator
            const auto charInfos = std::basic_string_view<CHAR_INFO>(subspan.data(), subspan.size());

            // Make the iterator and write to the target position.
            OutputCellIterator it(charInfos);
            storageBuffer.Write(it, target);
        }

        // Since we've managed to write part of the request, return the clamped part that we actually used.
        writtenRectangle = writeRectangle;

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                           gsl::span<CHAR_INFO> buffer,
                                                           const Viewport& requestRectangle,
                                                           Viewport& writtenRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto codepage = gci.OutputCP;
        LOG_IF_FAILED(_ConvertCellsToWInplace(codepage, buffer, requestRectangle));

        RETURN_IF_FAILED(_WriteConsoleOutputWImplHelper(context, buffer, requestRectangle, writtenRectangle));

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                           gsl::span<CHAR_INFO> buffer,
                                                           const Viewport& requestRectangle,
                                                           Viewport& writtenRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        if (!context.GetActiveBuffer().GetCurrentFont().IsTrueTypeFont())
        {
            // For compatibility reasons, we must maintain the behavior that munges the data if we are writing while a raster font is enabled.
            // This can be removed when raster font support is removed.
            auto translated = _ConvertCellsToMungedW(buffer, requestRectangle);
            RETURN_IF_FAILED(_WriteConsoleOutputWImplHelper(context, translated, requestRectangle, writtenRectangle));
        }
        else
        {
            RETURN_IF_FAILED(_WriteConsoleOutputWImplHelper(context, buffer, requestRectangle, writtenRectangle));
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                                  const COORD origin,
                                                                  gsl::span<WORD> buffer,
                                                                  size_t& written) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto attrs = ReadOutputAttributes(context.GetActiveBuffer(), origin, buffer.size());
        std::copy(attrs.cbegin(), attrs.cend(), buffer.begin());
        written = attrs.size();

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& context,
                                                                   const COORD origin,
                                                                   gsl::span<char> buffer,
                                                                   size_t& written) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto chars = ReadOutputStringA(context.GetActiveBuffer(),
                                             origin,
                                             buffer.size());

        // for compatibility reasons, if we receive more chars than can fit in the buffer
        // then we don't send anything back.
        if (chars.size() <= gsl::narrow<size_t>(buffer.size()))
        {
            std::copy(chars.cbegin(), chars.cend(), buffer.begin());
            written = chars.size();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                                   const COORD origin,
                                                                   gsl::span<wchar_t> buffer,
                                                                   size_t& written) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto chars = ReadOutputStringW(context.GetActiveBuffer(),
                                             origin,
                                             buffer.size());

        // Only copy if the whole result will fit.
        if (chars.size() <= gsl::narrow<size_t>(buffer.size()))
        {
            std::copy(chars.cbegin(), chars.cend(), buffer.begin());
            written = chars.size();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// There used to be a text mode and a graphics mode flag.
// Text mode was used for regular applications like CMD.exe.
// Graphics mode was used for bitmap VDM buffers and is no longer supported.
// OEM console font mode used to represent rewriting the entire buffer into codepage 437 so the renderer could handle it with raster fonts.
//  But now the entire buffer is always kept in Unicode and the renderer asks for translation when/if necessary for raster fonts only.
// We keep these definitions here so the API can enforce that the only one we support any longer is the original text mode.
// See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682122(v=vs.85).aspx
#define CONSOLE_TEXTMODE_BUFFER 1
//#define CONSOLE_GRAPHICS_BUFFER 2
//#define CONSOLE_OEMFONT_DISPLAY 4

[[nodiscard]] NTSTATUS ConsoleCreateScreenBuffer(std::unique_ptr<ConsoleHandleData>& handle,
                                                 _In_ PCONSOLE_API_MSG /*Message*/,
                                                 _In_ PCD_CREATE_OBJECT_INFORMATION Information,
                                                 _In_ PCONSOLE_CREATESCREENBUFFER_MSG a)
{
    Telemetry::Instance().LogApiCall(Telemetry::ApiCall::CreateConsoleScreenBuffer);
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // If any buffer type except the one we support is set, it's invalid.
    if (WI_IsAnyFlagSet(a->Flags, ~CONSOLE_TEXTMODE_BUFFER))
    {
        // We no longer support anything other than a textmode buffer
        return STATUS_INVALID_PARAMETER;
    }

    ConsoleHandleData::HandleType const HandleType = ConsoleHandleData::HandleType::Output;

    const SCREEN_INFORMATION& siExisting = gci.GetActiveOutputBuffer();

    // Create new screen buffer.
    COORD WindowSize = siExisting.GetViewport().Dimensions();
    const FontInfo& existingFont = siExisting.GetCurrentFont();
    SCREEN_INFORMATION* ScreenInfo = nullptr;
    NTSTATUS Status = SCREEN_INFORMATION::CreateInstance(WindowSize,
                                                         existingFont,
                                                         WindowSize,
                                                         siExisting.GetAttributes(),
                                                         siExisting.GetAttributes(),
                                                         Cursor::CURSOR_SMALL_SIZE,
                                                         &ScreenInfo);

    if (!NT_SUCCESS(Status))
    {
        goto Exit;
    }

    Status = NTSTATUS_FROM_HRESULT(ScreenInfo->AllocateIoHandle(HandleType,
                                                                Information->DesiredAccess,
                                                                Information->ShareMode,
                                                                handle));

    if (!NT_SUCCESS(Status))
    {
        goto Exit;
    }

    SCREEN_INFORMATION::s_InsertScreenBuffer(ScreenInfo);

Exit:
    if (!NT_SUCCESS(Status))
    {
        delete ScreenInfo;
    }

    return Status;
}
