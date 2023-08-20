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

#include "../interactivity/inc/ServiceLocator.hpp"

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

        const auto Status = inputBuffer.Read(outEvents,
                                             eventReadCount,
                                             IsPeek,
                                             true,
                                             IsUnicode,
                                             false);

        if (CONSOLE_STATUS_WAIT == Status)
        {
            // If we're told to wait until later, move all of our context
            // to the read data object and send it back up to the server.
            waiter = std::make_unique<DirectReadData>(&inputBuffer,
                                                      &readHandleState,
                                                      eventReadCount);
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
                static_assert(offsetof(CHAR_INFO, Char.AsciiChar) == offsetof(CHAR_INFO, Char.UnicodeChar));

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
                        in1.Char.UnicodeChar = til::bit_cast<uint8_t>(AsciiDbcs[0]);
                        in2.Char.UnicodeChar = til::bit_cast<uint8_t>(AsciiDbcs[1]);
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
                    in1.Char.UnicodeChar = til::bit_cast<uint8_t>(asciiChar);
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

[[nodiscard]] static std::vector<CHAR_INFO> _ConvertCellsToMungedW(std::span<CHAR_INFO> buffer, const Viewport& rectangle)
{
    std::vector<CHAR_INFO> result;
    result.reserve(buffer.size());

    const auto size = rectangle.Dimensions();
    auto bufferIter = buffer.begin();

    for (til::CoordType i = 0; i < size.height; i++)
    {
        for (til::CoordType j = 0; j < size.width; j++)
        {
            // Prepare a candidate charinfo on the output side copying the colors but not the lead/trail information.
            auto candidate = *bufferIter;
            WI_ClearAllFlags(candidate.Attributes, COMMON_LVB_SBCSDBCS);

            // If the glyph we're given is full width, it needs to take two cells.
            if (IsGlyphFullWidth(candidate.Char.UnicodeChar))
            {
                // If we're not on the final cell of the row...
                if (j < size.width - 1)
                {
                    // Mark that we're consuming two cells.
                    j++;

                    // Fill one cell with a copy of the color and character marked leading
                    WI_SetFlag(candidate.Attributes, COMMON_LVB_LEADING_BYTE);
                    result.push_back(candidate);

                    // Fill a second cell with a copy of the color marked trailing and a padding character.
                    WI_ClearFlag(candidate.Attributes, COMMON_LVB_LEADING_BYTE);
                    WI_SetFlag(candidate.Attributes, COMMON_LVB_TRAILING_BYTE);
                }
                else
                {
                    // If we're on the final cell, this won't fit. Replace with a space.
                    candidate.Char.UnicodeChar = UNICODE_SPACE;
                }
            }

            // Push our candidate in.
            result.push_back(candidate);

            // Advance to read the next item.
            ++bufferIter;
        }
    }
    return result;
}

[[nodiscard]] static HRESULT _ReadConsoleOutputWImplHelper(const SCREEN_INFORMATION& context,
                                                           std::span<CHAR_INFO> targetBuffer,
                                                           const Microsoft::Console::Types::Viewport& requestRectangle,
                                                           Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& storageBuffer = context.GetActiveBuffer().GetTextBuffer();
        const auto storageSize = storageBuffer.GetSize().Dimensions();

        const auto targetSize = requestRectangle.Dimensions();

        // If either dimension of the request is too small, return an empty rectangle as read and exit early.
        if (targetSize.width <= 0 || targetSize.height <= 0)
        {
            readRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        // The buffer given should be big enough to hold the dimensions of the request.
        const auto targetArea = targetSize.area<size_t>();
        RETURN_HR_IF(E_INVALIDARG, targetArea < targetBuffer.size());

        // Clip the request rectangle to the size of the storage buffer
        auto clip = requestRectangle.ToExclusive();
        clip.right = std::min(clip.right, storageSize.width);
        clip.bottom = std::min(clip.bottom, storageSize.height);

        // Find the target point (where to write the user's buffer)
        // It will either be 0,0 or offset into the buffer by the inverse of the negative values.
        til::point targetPoint;
        targetPoint.x = clip.left < 0 ? -clip.left : 0;
        targetPoint.y = clip.top < 0 ? -clip.top : 0;

        // The clipped rect must be inside the buffer size, so it has a minimum value of 0. (max of itself and 0)
        clip.left = std::max(clip.left, 0);
        clip.top = std::max(clip.top, 0);

        // The final "request rectangle" or the area inside the buffer we want to read, is the clipped dimensions.
        const auto clippedRequestRectangle = Viewport::FromExclusive(clip);

        // We will start reading the buffer at the point of the top left corner (origin) of the (potentially adjusted) request
        const auto sourcePoint = clippedRequestRectangle.Origin();

        // Get an iterator to the beginning of the return buffer
        // We might have to seek this forward or skip around if we clipped the request.
        auto targetIter = targetBuffer.begin();
        til::point targetPos;
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
                ++sourceIter;
            }

            // Always advance the write iterator, we might have skipped it due to clipping.
            ++targetIter;

            // Increment the target
            targetPos.x++;
            if (targetPos.x >= targetSize.width)
            {
                targetPos.x = 0;
                targetPos.y++;
            }
        }

        // Reply with the region we read out of the backing buffer (potentially clipped)
        readRectangle = clippedRequestRectangle;

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

        RETURN_IF_FAILED(_ReadConsoleOutputWImplHelper(context, buffer, sourceRectangle, readRectangle));

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
                                                            std::span<CHAR_INFO> buffer,
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
        if (sourceSize.width <= 0 || sourceSize.height <= 0)
        {
            writtenRectangle = Viewport::FromDimensions(requestRectangle.Origin(), { 0, 0 });
            return S_OK;
        }

        // If the top and left of the destination we're trying to write it outside the buffer,
        // give the original request rectangle back and exit early OK.
        if (requestRectangle.Left() >= storageSize.width || requestRectangle.Top() >= storageSize.height)
        {
            writtenRectangle = requestRectangle;
            return S_OK;
        }

        // Do clipping according to the legacy patterns.
        auto writeRegion = requestRectangle.ToInclusive();
        til::inclusive_rect sourceRect;
        if (writeRegion.right > storageSize.width - 1)
        {
            writeRegion.right = storageSize.width - 1;
        }
        sourceRect.right = writeRegion.right - writeRegion.left;
        if (writeRegion.bottom > storageSize.height - 1)
        {
            writeRegion.bottom = storageSize.height - 1;
        }
        sourceRect.bottom = writeRegion.bottom - writeRegion.top;

        if (writeRegion.left < 0)
        {
            sourceRect.left = -writeRegion.left;
            writeRegion.left = 0;
        }
        else
        {
            sourceRect.left = 0;
        }

        if (writeRegion.top < 0)
        {
            sourceRect.top = -writeRegion.top;
            writeRegion.top = 0;
        }
        else
        {
            sourceRect.top = 0;
        }

        if (sourceRect.left > sourceRect.right || sourceRect.top > sourceRect.bottom)
        {
            return E_INVALIDARG;
        }

        const auto writeRectangle = Viewport::FromInclusive(writeRegion);

        auto target = writeRectangle.Origin();

        // For every row in the request, create a view into the clamped portion of just the one line to write.
        // This allows us to restrict the width of the call without allocating/copying any memory by just making
        // a smaller view over the existing big blob of data from the original call.
        for (; target.y < writeRectangle.BottomExclusive(); target.y++)
        {
            // We find the offset into the original buffer by the dimensions of the original request rectangle.
            const auto rowOffset = (target.y - requestRectangle.Top()) * requestRectangle.Width();
            const auto colOffset = target.x - requestRectangle.Left();
            const auto totalOffset = rowOffset + colOffset;

            // Now we make a subspan starting from that offset for as much of the original request as would fit
            const auto subspan = buffer.subspan(totalOffset, writeRectangle.Width());

            // Convert to a CHAR_INFO view to fit into the iterator
            const auto charInfos = std::span<const CHAR_INFO>(subspan.data(), subspan.size());

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
                                                           std::span<CHAR_INFO> buffer,
                                                           const Viewport& requestRectangle,
                                                           Viewport& writtenRectangle) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto codepage = gci.OutputCP;
        LOG_IF_FAILED(_ConvertCellsToWInplace(codepage, buffer, requestRectangle));

        RETURN_IF_FAILED(_WriteConsoleOutputWImplHelper(context, buffer, requestRectangle, writtenRectangle));

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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // If any buffer type except the one we support is set, it's invalid.
    if (WI_IsAnyFlagSet(a->Flags, ~CONSOLE_TEXTMODE_BUFFER))
    {
        // We no longer support anything other than a textmode buffer
        return STATUS_INVALID_PARAMETER;
    }

    const auto HandleType = ConsoleHandleData::HandleType::Output;

    const auto& siExisting = gci.GetActiveOutputBuffer();

    // Create new screen buffer.
    auto WindowSize = siExisting.GetViewport().Dimensions();
    const auto& existingFont = siExisting.GetCurrentFont();
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
