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
#include "screenInfo.hpp"

#include "../types/inc/convert.hpp"
#include "../types/inc/GlyphWidth.hpp"
#include "../types/inc/viewport.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../server/ProcessHandle.h"

#include <wil/resource.h>

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;

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
// - IsWaitAllowed - Whether an async read via CONSOLE_STATUS_WAIT is permitted.
// Return Value:
// - STATUS_SUCCESS - If data was found and ready for return to the client.
// - CONSOLE_STATUS_WAIT - If we didn't have enough data or needed to
// block, this will be returned along with context in *ppWaiter.
// - Or an out of memory/math/string error message in NTSTATUS format.
[[nodiscard]] HRESULT ApiRoutines::GetConsoleInputImpl(IConsoleInputObject& inputBuffer,
                                                       InputEventQueue& outEvents,
                                                       const size_t eventReadCount,
                                                       INPUT_READ_HANDLE_DATA& readHandleState,
                                                       const bool IsUnicode,
                                                       const bool IsPeek,
                                                       const bool IsWaitAllowed,
                                                       CONSOLE_API_MSG* pWaitReplyMessage) noexcept
{
    try
    {
        if (eventReadCount == 0)
        {
            return STATUS_SUCCESS;
        }

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const auto Status = inputBuffer.Read(outEvents,
                                             eventReadCount,
                                             IsPeek,
                                             IsWaitAllowed,
                                             IsUnicode,
                                             false);

        if (CONSOLE_STATUS_WAIT == Status)
        {
            // If we're told to wait until later, move all of our context
            // to the read data object and send it back up to the server.
            std::ignore = ConsoleWaitQueue::s_CreateWait(pWaitReplyMessage, new DirectReadData(&inputBuffer, &readHandleState, eventReadCount));
        }
        return Status;
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
                                                           const std::span<const INPUT_RECORD>& events,
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
                                                          const std::span<const INPUT_RECORD> buffer,
                                                          size_t& written,
                                                          const bool append) noexcept
try
{
    written = 0;

    if (buffer.empty())
    {
        return S_OK;
    }

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    InputEventQueue events;

    auto it = buffer.begin();
    const auto end = buffer.end();

    // Check out the loop below. When a previous call ended on a leading DBCS we store it for
    // the next call to WriteConsoleInputAImpl to join it with the now available trailing DBCS.
    if (context.IsWritePartialByteSequenceAvailable())
    {
        auto lead = context.FetchWritePartialByteSequence();
        const auto& trail = *it;

        if (trail.EventType == KEY_EVENT)
        {
            const char narrow[2]{
                lead.Event.KeyEvent.uChar.AsciiChar,
                trail.Event.KeyEvent.uChar.AsciiChar,
            };
            wchar_t wide[2];
            const auto length = MultiByteToWideChar(gci.CP, 0, &narrow[0], 2, &wide[0], 2);

            for (int i = 0; i < length; i++)
            {
                lead.Event.KeyEvent.uChar.UnicodeChar = wide[i];
                events.push_back(lead);
            }

            ++it;
        }
    }

    for (; it != end; ++it)
    {
        if (it->EventType != KEY_EVENT)
        {
            events.push_back(*it);
            continue;
        }

        auto lead = *it;
        char narrow[2]{ lead.Event.KeyEvent.uChar.AsciiChar };
        int narrowLength = 1;

        if (IsDBCSLeadByteConsole(lead.Event.KeyEvent.uChar.AsciiChar, &gci.CPInfo))
        {
            ++it;
            if (it == end)
            {
                // Missing trailing DBCS -> Store the lead for the next call to WriteConsoleInputAImpl.
                context.StoreWritePartialByteSequence(lead);
                break;
            }

            const auto& trail = *it;
            if (trail.EventType != KEY_EVENT)
            {
                // Invalid input -> Skip.
                continue;
            }

            narrow[1] = trail.Event.KeyEvent.uChar.AsciiChar;
            narrowLength = 2;
        }

        wchar_t wide[2];
        const auto length = MultiByteToWideChar(gci.CP, 0, &narrow[0], narrowLength, &wide[0], 2);

        for (int i = 0; i < length; i++)
        {
            lead.Event.KeyEvent.uChar.UnicodeChar = wide[i];
            events.push_back(lead);
        }
    }

    return _WriteConsoleInputWImplHelper(context, events, written, append);
}
CATCH_RETURN();

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
                                                          const std::span<const INPUT_RECORD> buffer,
                                                          size_t& written,
                                                          const bool append) noexcept
{
    written = 0;

    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return _WriteConsoleInputWImplHelper(context, buffer, written, append);
    }
    CATCH_RETURN();
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
                                                     const std::span<CHAR_INFO> buffer,
                                                     const Viewport rectangle) noexcept
{
    try
    {
        const auto size = rectangle.Dimensions();
        auto outIter = buffer.begin();

        for (til::CoordType i = 0; i < size.height; ++i)
        {
            for (til::CoordType j = 0; j < size.width; ++j, ++outIter)
            {
                auto& in1 = *outIter;

                // If .AsciiChar and .UnicodeChar have the same offset (since they're a union),
                // we can just write the latter with a byte-sized value to set the former
                // _and_ simultaneously clear the upper byte of .UnicodeChar to 0. Nice!
                static_assert(__builtin_offsetof(CHAR_INFO, Char.AsciiChar) == __builtin_offsetof(CHAR_INFO, Char.UnicodeChar));

                // Any time we see the lead flag, we presume there will be a trailing one following it.
                // Giving us two bytes of space (one per cell in the ascii part of the character union)
                // to fill with whatever this Unicode character converts into.
                if (WI_IsFlagSet(in1.Attributes, COMMON_LVB_LEADING_BYTE))
                {
                    // As long as we're not looking at the exact last column of the buffer...
                    if (j < size.width - 1)
                    {
                        // Walk forward one because we're about to consume two cells.
                        ++j;
                        ++outIter;

                        auto& in2 = *outIter;

                        // Try to convert the unicode character (2 bytes) in the leading cell to the codepage.
                        CHAR AsciiDbcs[2]{};
                        ConvertToOem(codepage, &in1.Char.UnicodeChar, 1, &AsciiDbcs[0], 2);

                        // Fill the 1 byte (AsciiChar) portion of the leading and trailing cells with each of the bytes returned.
                        // We have to be bit careful here not to directly write the CHARs, because CHARs are signed whereas wchar_t isn't
                        // and we don't want any sign-extension. We want a 1:1 copy instead, so cast it to an unsigned char first.
                        in1.Char.UnicodeChar = std::bit_cast<uint8_t>(AsciiDbcs[0]);
                        in2.Char.UnicodeChar = std::bit_cast<uint8_t>(AsciiDbcs[1]);
                    }
                    else
                    {
                        // When we're in the last column with only a leading byte, we can't return that without a trailing.
                        // Instead, replace the output data with just a space and clear all flags.
                        in1.Char.UnicodeChar = UNICODE_SPACE;
                        WI_ClearAllFlags(in1.Attributes, COMMON_LVB_SBCSDBCS);
                    }
                }
                else if (WI_AreAllFlagsClear(in1.Attributes, COMMON_LVB_SBCSDBCS))
                {
                    // If there are no leading/trailing pair flags, then we only have 1 ascii byte to try to fit the
                    // 2 byte UTF-16 character into. Give it a go.
                    CHAR asciiChar{};
                    ConvertToOem(codepage, &in1.Char.UnicodeChar, 1, &asciiChar, 1);
                    in1.Char.UnicodeChar = std::bit_cast<uint8_t>(asciiChar);
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
[[nodiscard]] HRESULT _ConvertCellsToWInplace(const UINT codepage,
                                              std::span<CHAR_INFO> buffer,
                                              const Viewport& rectangle) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        const auto size = rectangle.Dimensions();
        auto outIter = buffer.begin();

        for (til::CoordType i = 0; i < size.height; ++i)
        {
            for (til::CoordType j = 0; j < size.width; ++j, ++outIter)
            {
                // Clear lead/trailing flags. We'll determine it for ourselves versus the given codepage.
                auto& in1 = *outIter;
                WI_ClearAllFlags(in1.Attributes, COMMON_LVB_SBCSDBCS);

                // If the 1 byte given is a lead in this codepage, we likely need two cells for the width.
                if (IsDBCSLeadByteConsole(in1.Char.AsciiChar, &gci.OutputCPInfo))
                {
                    // If we're not on the last column, we have two cells to use.
                    if (j < size.width - 1)
                    {
                        // Mark we're consuming two cells.
                        ++outIter;
                        ++j;

                        // Just as above - clear the flags, as we're setting them ourselves.
                        auto& in2 = *outIter;
                        WI_ClearAllFlags(in2.Attributes, COMMON_LVB_SBCSDBCS);

                        // Grab the lead/trailing byte pair from this cell and the next one forward.
                        CHAR AsciiDbcs[2];
                        AsciiDbcs[0] = in1.Char.AsciiChar;
                        AsciiDbcs[1] = in2.Char.AsciiChar;

                        // Convert it to UTF-16.
                        wchar_t wch = UNICODE_SPACE;
                        ConvertOutputToUnicode(codepage, &AsciiDbcs[0], 2, &wch, 1);

                        // Store the actual character in the first available position.
                        in1.Char.UnicodeChar = wch;
                        WI_SetFlag(in1.Attributes, COMMON_LVB_LEADING_BYTE);

                        // Put a padding character in the second position.
                        in2.Char.UnicodeChar = wch;
                        WI_SetFlag(in2.Attributes, COMMON_LVB_TRAILING_BYTE);
                    }
                    else
                    {
                        // If we were on the last column, put in a space.
                        in1.Char.UnicodeChar = UNICODE_SPACE;
                    }
                }
                else
                {
                    // If it's not detected as a lead byte of a pair, then just convert it in place and move on.
                    wchar_t wch = UNICODE_SPACE;
                    ConvertOutputToUnicode(codepage, &in1.Char.AsciiChar, 1, &wch, 1);
                    in1.Char.UnicodeChar = wch;
                }
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ReadConsoleOutputWImplHelper(const SCREEN_INFORMATION& context,
                                                   std::span<CHAR_INFO> targetBuffer,
                                                   const Viewport& requestRectangle,
                                                   Viewport& readRectangle) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto& storageBuffer = context.GetActiveBuffer();
        const auto storageRectangle = storageBuffer.GetBufferSize();
        const auto clippedRectangle = storageRectangle.Clamp(requestRectangle);

        if (!clippedRectangle.IsValid())
        {
            readRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        const auto bufferStride = gsl::narrow_cast<size_t>(std::max(0, requestRectangle.Width()));
        const auto width = gsl::narrow_cast<size_t>(clippedRectangle.Width());
        const auto offsetY = clippedRectangle.Top() - requestRectangle.Top();
        const auto offsetX = clippedRectangle.Left() - requestRectangle.Left();
        // We always write the intersection between the valid `storageRectangle` and the given `requestRectangle`.
        // This means that if the `requestRectangle` is -3 rows above the top of the buffer, we'll start
        // reading from `buffer` at row offset 3, because the first 3 are outside the valid range.
        // clippedRectangle.Top/Left() cannot be negative due to the previous Clamp() call.
        auto totalOffset = offsetY * bufferStride + offsetX;

        if (bufferStride <= 0 || targetBuffer.size() < gsl::narrow_cast<size_t>(clippedRectangle.Height() * bufferStride))
        {
            return E_INVALIDARG;
        }

        for (til::CoordType y = clippedRectangle.Top(); y <= clippedRectangle.BottomInclusive(); y++)
        {
            auto it = storageBuffer.GetCellDataAt({ clippedRectangle.Left(), y });

            for (size_t i = 0; i < width; i++)
            {
                targetBuffer[totalOffset + i] = gci.AsCharInfo(*it);
                ++it;
            }

            totalOffset += bufferStride;
        }

        readRectangle = clippedRectangle;
        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                          std::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                          Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto codepage = gci.OutputCP;

        RETURN_IF_FAILED(ReadConsoleOutputWImplHelper(context, buffer, sourceRectangle, readRectangle));

        LOG_IF_FAILED(_ConvertCellsToAInplace(codepage, buffer, readRectangle));

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                          std::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                          Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        return ReadConsoleOutputWImplHelper(context, buffer, sourceRectangle, readRectangle);
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT WriteConsoleOutputWImplHelper(SCREEN_INFORMATION& context,
                                                    std::span<const CHAR_INFO> buffer,
                                                    til::CoordType bufferStride,
                                                    const Viewport& requestRectangle,
                                                    Viewport& writtenRectangle) noexcept
{
    try
    {
        if (bufferStride <= 0)
        {
            return E_INVALIDARG;
        }

        auto& storageBuffer = context.GetActiveBuffer();
        const auto storageRectangle = storageBuffer.GetBufferSize();
        const auto clippedRectangle = storageRectangle.Clamp(requestRectangle);

        if (!clippedRectangle.IsValid())
        {
            writtenRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        const auto width = clippedRectangle.Width();
        // We always write the intersection between the valid `storageRectangle` and the given `requestRectangle`.
        // This means that if the `requestRectangle` is -3 rows above the top of the buffer, we'll start
        // reading from `buffer` at row offset 3, because the first 3 are outside the valid range.
        // clippedRectangle.Top/Left() cannot be negative due to the previous Clamp() call.
        const auto offsetY = clippedRectangle.Top() - requestRectangle.Top();
        const auto offsetX = clippedRectangle.Left() - requestRectangle.Left();
        auto totalOffset = offsetY * bufferStride + offsetX;

        if (bufferStride <= 0 || buffer.size() < gsl::narrow_cast<size_t>(clippedRectangle.Height() * bufferStride))
        {
            return E_INVALIDARG;
        }

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto writer = gci.GetVtWriterForBuffer(&context);

        for (til::CoordType y = clippedRectangle.Top(); y <= clippedRectangle.BottomInclusive(); y++)
        {
            const auto charInfos = buffer.subspan(totalOffset, width);
            const til::point target{ clippedRectangle.Left(), y };

            // Make the iterator and write to the target position.
            storageBuffer.Write(OutputCellIterator(charInfos), target);

            if (writer)
            {
                writer.WriteInfos(target, charInfos);
            }

            totalOffset += bufferStride;
        }

        // If we've overwritten image content, it needs to be erased.
        ImageSlice::EraseBlock(storageBuffer.GetTextBuffer(), clippedRectangle.ToExclusive());

        // Since we've managed to write part of the request, return the clamped part that we actually used.
        writtenRectangle = clippedRectangle;

        if (writer)
        {
            writer.Submit();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                           std::span<CHAR_INFO> buffer,
                                                           const Viewport& requestRectangle,
                                                           Viewport& writtenRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto writer = gci.GetVtWriterForBuffer(&context);

        if (writer)
        {
            writer.BackupCursor();
        }

        const auto codepage = gci.OutputCP;
        LOG_IF_FAILED(_ConvertCellsToWInplace(codepage, buffer, requestRectangle));

        RETURN_IF_FAILED(WriteConsoleOutputWImplHelper(context, buffer, requestRectangle.Width(), requestRectangle, writtenRectangle));

        if (writer)
        {
            writer.Submit();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                           std::span<CHAR_INFO> buffer,
                                                           const Viewport& requestRectangle,
                                                           Viewport& writtenRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        auto writer = gci.GetVtWriterForBuffer(&context);

        if (writer)
        {
            writer.BackupCursor();
        }

        RETURN_IF_FAILED(WriteConsoleOutputWImplHelper(context, buffer, requestRectangle.Width(), requestRectangle, writtenRectangle));

        if (writer)
        {
            writer.Submit();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                                  const til::point origin,
                                                                  std::span<WORD> buffer,
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
                                                                   const til::point origin,
                                                                   std::span<char> buffer,
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
        if (chars.size() <= buffer.size())
        {
            std::copy(chars.cbegin(), chars.cend(), buffer.begin());
            written = chars.size();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT ApiRoutines::ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                                   const til::point origin,
                                                                   std::span<wchar_t> buffer,
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
        if (chars.size() <= buffer.size())
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
// Graphics mode was used for bitmap VDM buffers, and was cut out of the
// modern host for a long time - CONSOLE_GRAPHICS_BUFFER is resurrected below,
// generalized so any client (not just NTVDM) can request one.
// OEM console font mode used to represent rewriting the entire buffer into codepage 437 so the renderer could handle it with raster fonts.
//  But now the entire buffer is always kept in Unicode and the renderer asks for translation when/if necessary for raster fonts only.
// See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms682122(v=vs.85).aspx
#define CONSOLE_TEXTMODE_BUFFER 1
#define CONSOLE_GRAPHICS_BUFFER 2
//#define CONSOLE_OEMFONT_DISPLAY 4

// The original NT4/2k console server used 0x1F0001 (MUTANT_ALL_ACCESS) here;
// spelled out explicitly since it isn't universally pulled in by the SDK headers.
#ifndef MUTANT_ALL_ACCESS
#define MUTANT_ALL_ACCESS 0x1F0001
#endif

// Routine Description:
// - Builds the shared pixel buffer backing a CONSOLE_GRAPHICS_BUFFER screen
//   buffer. A pagefile-backed section is created and mapped once into this
//   process (so the renderer can read it) and once directly into the
//   requesting client's process (so the client can write pixels with zero IPC
//   per frame), plus a Mutant duplicated into the client so both sides can
//   synchronize access to the pixel data with nothing but kernel objects.
//   This is the same mechanism NTVDM's full-screen DOS graphics mode
//   originally relied on, generalized to any client.
// Arguments:
// - Message - The originating create-screen-buffer request; supplies the
//   client's process and the BITMAPINFO payload describing the desired pixel
//   format (sent as the message's input buffer, alongside the fixed struct).
// - a - The fixed portion of the create-screen-buffer request.
// - graphicsBuffer - Receives the constructed GraphicsBuffer on success.
// - pixelSize - Receives the buffer's dimensions, in pixels.
// Return Value:
// - STATUS_SUCCESS, or an error from the underlying section/mapping/mutant calls.
[[nodiscard]] static NTSTATUS CreateGraphicsBuffer(_In_ PCONSOLE_API_MSG Message,
                                                   _In_ PCONSOLE_CREATESCREENBUFFER_MSG a,
                                                   _Out_ std::unique_ptr<GraphicsBuffer>& graphicsBuffer,
                                                   _Out_ til::size& pixelSize)
{
    // Arbitrary but generous ceilings so a hostile/buggy client can't make us
    // commit an unreasonable amount of shared memory.
    constexpr ULONG maxBitmapInfoLength = 64 * 1024;
    constexpr ULONGLONG maxBitmapImageSize = 256ULL * 1024 * 1024;
    constexpr LONG maxDimension = 16384;

    if (a->BitmapInfoLength < sizeof(BITMAPINFOHEADER) || a->BitmapInfoLength > maxBitmapInfoLength)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID inputBuffer{};
    ULONG inputBufferSize{};
    auto Status = NTSTATUS_FROM_HRESULT(Message->GetInputBuffer(&inputBuffer, &inputBufferSize));
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // For a CD_IO_OBJECT_TYPE_NEW_OUTPUT create, GetInputBuffer()'s default
    // read offset (0) points at the start of the fixed
    // CD_CREATE_OBJECT_INFORMATION + CONSOLE_CREATESCREENBUFFER_MSG structs
    // themselves (the same data already available via Information/a) - the
    // client's actual BITMAPINFO payload was appended after those in the
    // NtCreateFile EA blob, so skip past them to reach it.
    constexpr ULONG fixedStructSize = sizeof(CD_CREATE_OBJECT_INFORMATION) + sizeof(CONSOLE_CREATESCREENBUFFER_MSG);
    if (inputBufferSize < fixedStructSize + a->BitmapInfoLength)
    {
        return STATUS_INVALID_PARAMETER;
    }
    const auto bitmapInfoBytes = static_cast<const BYTE*>(inputBuffer) + fixedStructSize;

    // Copy the client's BITMAPINFO (header + optional color table) into our
    // own storage - both so we're free to normalize it below, and so it
    // outlives this call (it's retained on the GraphicsBuffer for later
    // queries, e.g. by the renderer).
    std::vector<BYTE> bitmapInfoStorage(a->BitmapInfoLength);
    memcpy(bitmapInfoStorage.data(), bitmapInfoBytes, a->BitmapInfoLength);
    auto& header = *reinterpret_cast<BITMAPINFOHEADER*>(bitmapInfoStorage.data());

    if (header.biSize < sizeof(BITMAPINFOHEADER) ||
        header.biPlanes != 1 ||
        header.biWidth <= 0 || header.biWidth > maxDimension ||
        header.biHeight == 0 || header.biHeight < -maxDimension || header.biHeight > maxDimension ||
        (header.biBitCount != 8 && header.biBitCount != 16 && header.biBitCount != 24 && header.biBitCount != 32) ||
        (a->Usage != DIB_RGB_COLORS && a->Usage != DIB_PAL_COLORS))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // For DIB_PAL_COLORS, bmiColors[] is an array of WORD palette indices
    // (not RGBQUAD) sized by biClrUsed (or 2^biBitCount if unset) - make sure
    // the client actually sent that many bytes, since the renderer reads
    // straight from this storage later (GdiEngine::PaintConsoleBitmap).
    if (a->Usage == DIB_PAL_COLORS && header.biBitCount <= 8)
    {
        const ULONG numColorTableEntries = header.biClrUsed != 0 ? header.biClrUsed : (1u << header.biBitCount);
        const auto requiredLength = static_cast<ULONGLONG>(header.biSize) + static_cast<ULONGLONG>(numColorTableEntries) * sizeof(WORD);
        if (a->BitmapInfoLength < requiredLength)
        {
            return STATUS_INVALID_PARAMETER;
        }
    }

    // Match the original implementation: always top-down, always uncompressed.
    if (header.biHeight > 0)
    {
        header.biHeight = -header.biHeight;
    }
    header.biCompression = BI_RGB;

    // Don't trust the client's biSizeImage - recompute it ourselves from the
    // (now validated) width/height/bit depth.
    const auto strideBytes = ((static_cast<ULONGLONG>(header.biWidth) * header.biBitCount + 31) / 32) * 4;
    const auto sizeImage64 = strideBytes * static_cast<ULONGLONG>(-header.biHeight);
    if (sizeImage64 == 0 || sizeImage64 > maxBitmapImageSize)
    {
        return STATUS_INVALID_PARAMETER;
    }
    header.biSizeImage = static_cast<ULONG>(sizeImage64);

    const SIZE_T bitmapSize = gsl::narrow_cast<SIZE_T>(sizeImage64);

    wil::unique_handle hSection;
    LARGE_INTEGER maximumSize;
    maximumSize.QuadPart = bitmapSize;
    Status = NtCreateSection(&hSection,
                             SECTION_ALL_ACCESS,
                             nullptr,
                             &maximumSize,
                             PAGE_READWRITE,
                             SEC_COMMIT,
                             nullptr);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    PVOID bitMap = nullptr;
    auto viewSize = bitmapSize;
    Status = NtMapViewOfSection(hSection.get(),
                                GetCurrentProcess(),
                                &bitMap,
                                0,
                                bitmapSize,
                                nullptr,
                                &viewSize,
                                NT_VIEW_UNMAP,
                                0,
                                PAGE_READWRITE);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }
    auto unmapSelf = wil::scope_exit([&]() noexcept {
        if (bitMap)
        {
            NtUnmapViewOfSection(GetCurrentProcess(), bitMap);
        }
    });

    // Duplicate our own handle to the client's process (rather than trusting
    // one out of the message directly) - this GraphicsBuffer, and thus this
    // handle, may outlive the specific call that created it.
    const auto clientProcess = Message->GetProcessHandle();
    if (!clientProcess)
    {
        return STATUS_INVALID_PARAMETER;
    }

    wil::unique_handle hClientProcess;
    Status = NtDuplicateObject(GetCurrentProcess(),
                               clientProcess->GetRawHandle(),
                               GetCurrentProcess(),
                               &hClientProcess,
                               0,
                               0,
                               DUPLICATE_SAME_ACCESS);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    PVOID clientBitMap = nullptr;
    auto clientViewSize = bitmapSize;
    Status = NtMapViewOfSection(hSection.get(),
                                hClientProcess.get(),
                                &clientBitMap,
                                0,
                                bitmapSize,
                                nullptr,
                                &clientViewSize,
                                NT_VIEW_UNMAP,
                                0,
                                PAGE_READWRITE);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }
    auto unmapClient = wil::scope_exit([&]() noexcept {
        if (clientBitMap)
        {
            NtUnmapViewOfSection(hClientProcess.get(), clientBitMap);
        }
    });

    wil::unique_handle hMutex;
    Status = NtCreateMutant(&hMutex, MUTANT_ALL_ACCESS, nullptr, FALSE);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    HANDLE hClientMutex = nullptr;
    Status = NtDuplicateObject(GetCurrentProcess(),
                               hMutex.get(),
                               hClientProcess.get(),
                               &hClientMutex,
                               0,
                               0,
                               DUPLICATE_SAME_ACCESS);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // Everything succeeded - ownership of bitMap/clientBitMap is moving to the
    // GraphicsBuffer below, so don't unmap them on the way out.
    unmapSelf.release();
    unmapClient.release();

    pixelSize = til::size{ header.biWidth, -header.biHeight };
    graphicsBuffer = std::make_unique<GraphicsBuffer>(std::move(hSection),
                                                       std::move(hClientProcess),
                                                       std::move(hMutex),
                                                       hClientMutex,
                                                       bitMap,
                                                       clientBitMap,
                                                       bitmapSize,
                                                       std::move(bitmapInfoStorage),
                                                       a->Usage);

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS ConsoleCreateScreenBuffer(std::unique_ptr<ConsoleHandleData>& handle,
                                                 _In_ PCONSOLE_API_MSG Message,
                                                 _In_ PCD_CREATE_OBJECT_INFORMATION Information,
                                                 _In_ PCONSOLE_CREATESCREENBUFFER_MSG a)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // Exactly one of the two supported buffer kinds must be requested.
    const auto wantsTextBuffer = WI_IsFlagSet(a->Flags, CONSOLE_TEXTMODE_BUFFER);
    const auto wantsGraphicsBuffer = WI_IsFlagSet(a->Flags, CONSOLE_GRAPHICS_BUFFER);
    if (WI_IsAnyFlagSet(a->Flags, ~(CONSOLE_TEXTMODE_BUFFER | CONSOLE_GRAPHICS_BUFFER)) ||
        wantsTextBuffer == wantsGraphicsBuffer)
    {
        return STATUS_INVALID_PARAMETER;
    }

    const auto HandleType = ConsoleHandleData::HandleType::Output;

    const auto& siExisting = gci.GetActiveOutputBuffer();
    const auto& existingFont = siExisting.GetCurrentFont();

    // For a text buffer this is the new buffer's window/screen-buffer size, as
    // before. For a graphics buffer it gets overwritten below with the pixel
    // dimensions from the client's BITMAPINFO - CreateInstance doesn't need to
    // know the difference, it just sizes the (otherwise unused, for a graphics
    // buffer) backing TextBuffer to match.
    auto WindowSize = siExisting.GetViewport().Dimensions();

    std::unique_ptr<GraphicsBuffer> graphicsBuffer;
    if (wantsGraphicsBuffer)
    {
        const auto Status = CreateGraphicsBuffer(Message, a, graphicsBuffer, WindowSize);
        if (FAILED_NTSTATUS(Status))
        {
            return Status;
        }
    }

    SCREEN_INFORMATION* ScreenInfo = nullptr;
    auto Status = SCREEN_INFORMATION::CreateInstance(WindowSize,
                                                     existingFont,
                                                     WindowSize,
                                                     siExisting.GetAttributes(),
                                                     siExisting.GetAttributes(),
                                                     Cursor::CURSOR_SMALL_SIZE,
                                                     &ScreenInfo);

    if (FAILED_NTSTATUS(Status))
    {
        goto Exit;
    }

    if (wantsGraphicsBuffer)
    {
        ScreenInfo->AttachGraphicsBuffer(std::move(graphicsBuffer));
    }

    Status = NTSTATUS_FROM_HRESULT(ScreenInfo->AllocateIoHandle(HandleType,
                                                                Information->DesiredAccess,
                                                                Information->ShareMode,
                                                                handle));

    if (FAILED_NTSTATUS(Status))
    {
        goto Exit;
    }

    SCREEN_INFORMATION::s_InsertScreenBuffer(ScreenInfo);

Exit:
    if (FAILED_NTSTATUS(Status))
    {
        delete ScreenInfo;
    }

    return Status;
}

// Routine Description:
// - ConsolepRegisterVDM's real logic. Registers the calling process as THE
//   VDM for this console (only one is allowed at a time) and creates a
//   shared VDM text buffer - the mechanism NTVDM relies on during its own
//   startup (the RegisterConsoleVDM() call in nt_det.c's initTextSection()).
//   The older fullscreen hardware-video-state section (GdiFullscreenControl
//   and the three hardware events) worked through Windows 7 under the XPDM
//   display driver model, but stopped functioning once WDDM (required for
//   newer display drivers) replaced it - WDDM's composited model doesn't
//   support the exclusive/direct hardware access the mechanism relied on.
//   It was also always #ifdef i386-only in the original source, since there
//   was no x64 NTVDM to support, so there's nothing to port for this build
//   regardless of the WDDM issue. NTVDM already tolerates it being absent
//   gracefully (a->StateLength == 0 means "fullscreen [hardware state] is
//   disabled in the console"), so this doesn't build it either.
// - a->RegisterFlags == 0 is the unregister case.
[[nodiscard]] NTSTATUS RegisterConsoleVdm(_In_ PCONSOLE_API_MSG Message,
                                          _Inout_ PCONSOLE_REGISTERVDM_MSG a)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    const auto clientProcess = Message->GetProcessHandle();
    if (!clientProcess)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (a->RegisterFlags == 0)
    {
        if (!gci.IsVdmRegistered())
        {
            return STATUS_ACCESS_DENIED;
        }

        gci.DetachVdmRegistration();
        ServiceLocator::LocateGlobals().accessibilityNotifier.ApplicationEnd(clientProcess->dwProcessId);

        return STATUS_SUCCESS;
    }

    if (gci.IsVdmRegistered())
    {
        return STATUS_INVALID_PARAMETER;
    }

    // The original verified the caller was really a VDM process via
    // NtVdmControl(VdmQueryVdmProcess, ...) here. That's not viable on this
    // platform - the entire kernel-mode VDM subsystem (NtVdmControl included)
    // was never ported to x64 Windows, so the call unconditionally returns
    // STATUS_NOT_IMPLEMENTED there regardless of who's actually calling
    // (confirmed empirically: 0xC0000002 against a genuine ntvdmx64 caller).
    // Dropped: the one-VDM-per-console exclusivity is still enforced above
    // via IsVdmRegistered(), just not a check on the caller's identity/type.
    wil::unique_handle hClientProcess;
    auto Status = NtDuplicateObject(GetCurrentProcess(),
                               clientProcess->GetRawHandle(),
                               GetCurrentProcess(),
                               &hClientProcess,
                               0,
                               0,
                               DUPLICATE_SAME_ACCESS);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // The modern CONSOLE_REGISTERVDM_MSG dropped the old VDMBufferSize IN
    // field entirely - there's nothing left in the wire message to read a
    // requested size from. Hardcoded to 80x50 cells at 4 bytes/cell
    // (char+attr as WORDs each, per the non-i386 branch of the original
    // SrvRegisterConsoleVDM), matching what NTVDM's own client side always
    // requests anyway (nt_det.c's initTextSection():
    // textBufferSize.X = 80; .Y = 50;).
    constexpr til::size vdmBufferSize{ 80, 50 };
    constexpr ULONGLONG bufferSize = static_cast<ULONGLONG>(vdmBufferSize.width) * vdmBufferSize.height * 4;

    wil::unique_handle hSection;
    LARGE_INTEGER maximumSize;
    maximumSize.QuadPart = static_cast<LONGLONG>(bufferSize);
    Status = NtCreateSection(&hSection,
                             SECTION_ALL_ACCESS,
                             nullptr,
                             &maximumSize,
                             PAGE_READWRITE,
                             SEC_COMMIT,
                             nullptr);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    PVOID buffer = nullptr;
    auto viewSize = static_cast<SIZE_T>(bufferSize);
    Status = NtMapViewOfSection(hSection.get(),
                                GetCurrentProcess(),
                                &buffer,
                                0,
                                static_cast<SIZE_T>(bufferSize),
                                nullptr,
                                &viewSize,
                                NT_VIEW_UNMAP,
                                0,
                                PAGE_READWRITE);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }
    auto unmapSelf = wil::scope_exit([&]() noexcept {
        if (buffer)
        {
            NtUnmapViewOfSection(GetCurrentProcess(), buffer);
        }
    });

    PVOID clientBuffer = nullptr;
    auto clientViewSize = static_cast<SIZE_T>(bufferSize);
    Status = NtMapViewOfSection(hSection.get(),
                                hClientProcess.get(),
                                &clientBuffer,
                                0,
                                static_cast<SIZE_T>(bufferSize),
                                nullptr,
                                &clientViewSize,
                                NT_VIEW_UNMAP,
                                0,
                                PAGE_READWRITE);
    if (FAILED_NTSTATUS(Status))
    {
        return Status;
    }

    // Everything succeeded - ownership of buffer is moving to the
    // VdmRegistration below, so don't unmap it on the way out.
    unmapSelf.release();

    const auto isWow = WI_IsFlagSet(a->RegisterFlags, CONSOLE_REGISTER_WOW);
    gci.AttachVdmRegistration(std::make_unique<VdmRegistration>(std::move(hClientProcess),
                                                                 std::move(hSection),
                                                                 buffer,
                                                                 clientBuffer,
                                                                 vdmBufferSize,
                                                                 isWow));

    // Match the original's "fullscreen hardware state disabled" contract -
    // see the routine description above.
    a->StateLength = 0;
    a->StateBuffer = nullptr;
    a->VDMBuffer = clientBuffer;

    ServiceLocator::LocateGlobals().accessibilityNotifier.ApplicationStart(clientProcess->dwProcessId);

    return STATUS_SUCCESS;
}
