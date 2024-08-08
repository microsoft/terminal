// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_output.h"

#include "directio.h"
#include "handle.h"
#include "misc.h"
#include "_stream.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/convert.hpp"
#include "../types/inc/Viewport.hpp"

#include <til/unicode.h>

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;

// Routine Description:
// - This routine writes a screen buffer region to the screen.
// Arguments:
// - screenInfo - reference to screen buffer information.
// - srRegion - Region to write in screen buffer coordinates. Region is inclusive
// Return Value:
// - <none>
void WriteToScreen(SCREEN_INFORMATION& screenInfo, const Viewport& region)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // update to screen, if we're not iconic.
    if (!screenInfo.IsActiveScreenBuffer() || WI_IsFlagSet(gci.Flags, CONSOLE_IS_ICONIC))
    {
        return;
    }

    // clip region to fit within the viewport
    const auto clippedRegion = screenInfo.GetViewport().Clamp(region);
    if (!clippedRegion.IsValid())
    {
        return;
    }

    if (screenInfo.IsActiveScreenBuffer())
    {
        if (ServiceLocator::LocateGlobals().pRender != nullptr)
        {
            ServiceLocator::LocateGlobals().pRender->TriggerRedraw(region);
        }
    }
}

enum class FillConsoleMode
{
    WriteAttribute,
    FillAttribute,
    WriteCharacter,
    FillCharacter,
};

struct FillConsoleResult
{
    size_t lengthRead = 0;
    til::CoordType cellsModified = 0;
};

static FillConsoleResult FillConsoleImpl(SCREEN_INFORMATION& screenInfo, FillConsoleMode mode, const void* data, const size_t lengthToWrite, const til::point startingCoordinate)
{
    if (lengthToWrite == 0)
    {
        return {};
    }

    LockConsole();
    const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& screenBuffer = screenInfo.GetActiveBuffer();
    auto& textBuffer = screenBuffer.GetTextBuffer();
    const auto bufferSize = screenBuffer.GetBufferSize();

    if (!bufferSize.IsInBounds(startingCoordinate))
    {
        return {};
    }

    auto writer = gci.GetVtWriterForBuffer(&screenInfo);
    if (writer)
    {
        writer.BackupCursor();
    }

    FillConsoleResult result;
    const auto h = bufferSize.Height();
    const auto w = bufferSize.Width();
    auto y = startingCoordinate.y;
    auto input = static_cast<const uint16_t*>(data);
    const std::wstring_view inputText{ (const wchar_t*)input, lengthToWrite };
    size_t inputPos = 0;
    til::small_vector<CHAR_INFO, 1024> infoBuffer;
    Viewport unused;

    infoBuffer.resize(gsl::narrow_cast<size_t>(w));

    auto fillCharacter = UNICODE_REPLACEMENT;
    til::CoordType fillCharacterWidth = 1;
    if (mode == FillConsoleMode::FillCharacter)
    {
        wchar_t chars[3] = { L'a', inputText[0], L'a' };
        std::wstring_view foobar{ &chars[0], 3 };
        til::CoordType width = 1;
        const auto stop1 = textBuffer.GraphemeNext(foobar, 0, nullptr);
        const auto stop2 = textBuffer.GraphemeNext(foobar, stop1, &width);
        if (stop1 == 1 && stop2 == 2 && !til::is_surrogate(foobar[1]))
        {
            fillCharacter = foobar[1];
            fillCharacterWidth = width;
        }
    }

    while (y < h && inputPos < lengthToWrite)
    {
        const auto beg = y == startingCoordinate.y ? startingCoordinate.x : 0;
        const auto columnsAvailable = w - beg;
        til::CoordType columns = 0;

        const auto readViewport = Viewport::FromInclusive({ beg, y, w - 1, y });
        THROW_IF_FAILED(ReadConsoleOutputWImplHelper(screenInfo, infoBuffer, readViewport, unused));

        switch (mode)
        {
        case FillConsoleMode::WriteAttribute:
            for (; columns < columnsAvailable && inputPos < lengthToWrite; ++columns, ++inputPos)
            {
                infoBuffer[columns].Attributes = input[inputPos];
            }
            break;
        case FillConsoleMode::FillAttribute:
            for (const auto attr = input[0]; columns < columnsAvailable && inputPos < lengthToWrite; ++columns, ++inputPos)
            {
                infoBuffer[columns].Attributes = attr;
            }
            break;
        case FillConsoleMode::WriteCharacter:
            for (; columns < columnsAvailable && inputPos < lengthToWrite; ++inputPos)
            {
                auto ch = input[inputPos];
                auto next = inputPos + 1;
                til::CoordType width = 1;

                if (ch >= 0x80)
                {
                    next = textBuffer.GraphemeNext(inputText, inputPos, &width);
                    if (next - inputPos != 1 || til::is_surrogate(ch))
                    {
                        ch = UNICODE_REPLACEMENT;
                    }
                }

                if (width > 1)
                {
                    const auto columnEnd = columns + width;

                    // If the wide glyph doesn't fit into the last column, pad it with whitespace.
                    if (columnEnd > columnsAvailable)
                    {
                        auto& lead = infoBuffer[columns++];
                        lead.Char.UnicodeChar = L' ';
                        lead.Attributes = lead.Attributes & ~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
                        break;
                    }

                    auto& lead = infoBuffer[columns++];
                    lead.Char.UnicodeChar = ch;
                    lead.Attributes = lead.Attributes & ~COMMON_LVB_TRAILING_BYTE | COMMON_LVB_LEADING_BYTE;

                    auto& trail = infoBuffer[columns++];
                    trail.Char.UnicodeChar = ch;
                    trail.Attributes = trail.Attributes & ~COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE;
                }
                else
                {
                    auto& lead = infoBuffer[columns++];
                    lead.Char.UnicodeChar = ch;
                    lead.Attributes = lead.Attributes & ~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
                }
            }
            break;
        case FillConsoleMode::FillCharacter:
        {
            // Identical to WriteCharacter above, but with the if() and for() swapped.
            if (fillCharacterWidth > 1)
            {
                for (; columns < columnsAvailable && inputPos < lengthToWrite; ++inputPos)
                {
                    // If the wide glyph doesn't fit into the last column, pad it with whitespace.
                    if ((columns + fillCharacterWidth) > columnsAvailable)
                    {
                        auto& lead = infoBuffer[columns++];
                        lead.Char.UnicodeChar = L' ';
                        lead.Attributes = lead.Attributes & ~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
                        break;
                    }

                    auto& lead = infoBuffer[columns++];
                    lead.Char.UnicodeChar = fillCharacter;
                    lead.Attributes = lead.Attributes & ~COMMON_LVB_TRAILING_BYTE | COMMON_LVB_LEADING_BYTE;

                    auto& trail = infoBuffer[columns++];
                    trail.Char.UnicodeChar = fillCharacter;
                    trail.Attributes = trail.Attributes & ~COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE;
                }
            }
            else
            {
                for (; columns < columnsAvailable && inputPos < lengthToWrite; ++inputPos)
                {
                    auto& lead = infoBuffer[columns++];
                    lead.Char.UnicodeChar = fillCharacter;
                    lead.Attributes = lead.Attributes & ~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
                }
            }
            break;
        }
        }

        const auto writeViewport = Viewport::FromInclusive({ beg, y, beg + columns - 1, y });
        THROW_IF_FAILED(WriteConsoleOutputWImplHelper(screenInfo, infoBuffer, w, writeViewport, unused));

        y += 1;
        result.cellsModified += columns;
    }

    result.lengthRead = inputPos;

    if (writer)
    {
        writer.Submit();
    }

    if (screenBuffer.HasAccessibilityEventing())
    {
        // Notify accessibility
        auto endingCoordinate = startingCoordinate;
        bufferSize.WalkInBounds(endingCoordinate, result.cellsModified);
        screenBuffer.NotifyAccessibilityEventing(startingCoordinate.x, startingCoordinate.y, endingCoordinate.x, endingCoordinate.y);
    }

    return result;
}

// Routine Description:
// - writes text attributes to the screen
// Arguments:
// - OutContext - the screen info to write to
// - attrs - the attrs to write to the screen
// - target - the starting coordinate in the screen
// - used - number of elements written
// Return Value:
// - S_OK, E_INVALIDARG or similar HRESULT error.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                   const std::span<const WORD> attrs,
                                                                   const til::point target,
                                                                   size_t& used) noexcept
{
    // Set used to 0 from the beginning in case we exit early.
    used = 0;

    if (attrs.empty())
    {
        return S_OK;
    }

    try
    {
        LockConsole();
        const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        used = FillConsoleImpl(OutContext, FillConsoleMode::WriteAttribute, attrs.data(), attrs.size(), target).cellsModified;
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - writes text to the screen
// Arguments:
// - screenInfo - the screen info to write to
// - chars - the text to write to the screen
// - target - the starting coordinate in the screen
// - used - number of elements written
// Return Value:
// - S_OK, E_INVALIDARG or similar HRESULT error.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                    const std::wstring_view chars,
                                                                    const til::point target,
                                                                    size_t& used) noexcept
{
    // Set used to 0 from the beginning in case we exit early.
    used = 0;

    if (chars.empty())
    {
        return S_OK;
    }

    try
    {
        LockConsole();
        const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        used = FillConsoleImpl(OutContext, FillConsoleMode::WriteCharacter, chars.data(), chars.size(), target).lengthRead;
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - writes text to the screen
// Arguments:
// - screenInfo - the screen info to write to
// - chars - the text to write to the screen
// - target - the starting coordinate in the screen
// - used - number of elements written
// Return Value:
// - S_OK, E_INVALIDARG or similar HRESULT error.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                    const std::string_view chars,
                                                                    const til::point target,
                                                                    size_t& used) noexcept
{
    // Set used to 0 from the beginning in case we exit early.
    used = 0;

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto codepage = gci.OutputCP;
    try
    {
        // convert to wide chars so we can call the W version of this function
        const auto wideChars = ConvertToW(codepage, chars);

        size_t wideCharsWritten = 0;
        RETURN_IF_FAILED(WriteConsoleOutputCharacterWImpl(OutContext, wideChars, target, wideCharsWritten));

        // Create a view over the wide chars and reduce it to the amount actually written (do in two steps to enforce bounds)
        std::wstring_view writtenView(wideChars);
        writtenView = writtenView.substr(0, wideCharsWritten);

        // Look over written wide chars to find equivalent count of ascii chars so we can properly report back
        // how many elements were actually written
        used = GetALengthFromW(codepage, writtenView);
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - fills the screen buffer with the specified text attribute
// Arguments:
// - OutContext - reference to screen buffer information.
// - attribute - the text attribute to use to fill
// - lengthToWrite - the number of elements to write
// - startingCoordinate - Screen buffer coordinate to begin writing to.
// - cellsModified - the number of elements written
// Return Value:
// - S_OK or suitable HRESULT code from failure to write (memory issues, invalid arg, etc.)
[[nodiscard]] HRESULT ApiRoutines::FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                  const WORD attribute,
                                                                  const size_t lengthToWrite,
                                                                  const til::point startingCoordinate,
                                                                  size_t& cellsModified,
                                                                  const bool enablePowershellShim) noexcept
{
    try
    {
        LockConsole();
        const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        // See FillConsoleOutputCharacterWImpl and its identical code.
        if (enablePowershellShim)
        {
            auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            if (const auto writer = gci.GetVtWriterForBuffer(&OutContext))
            {
                const auto currentBufferDimensions{ OutContext.GetBufferSize().Dimensions() };
                const auto wroteWholeBuffer = lengthToWrite == (currentBufferDimensions.area<size_t>());
                const auto startedAtOrigin = startingCoordinate == til::point{ 0, 0 };
                const auto wroteSpaces = attribute == (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);

                if (wroteWholeBuffer && startedAtOrigin && wroteSpaces)
                {
                    // PowerShell has previously called FillConsoleOutputCharacterW() which triggered a call to WriteClearScreen().
                    cellsModified = lengthToWrite;
                    return S_OK;
                }
            }
        }

        cellsModified = FillConsoleImpl(OutContext, FillConsoleMode::FillAttribute, &attribute, lengthToWrite, startingCoordinate).cellsModified;
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - fills the screen buffer with the specified wchar
// Arguments:
// - OutContext - reference to screen buffer information.
// - character - wchar to fill with
// - lengthToWrite - the number of elements to write
// - startingCoordinate - Screen buffer coordinate to begin writing to.
// - cellsModified - the number of elements written
// - enablePowershellShim - true iff the client process that's calling this
//   method is "powershell.exe". Used to enable certain compatibility shims for
//   conpty mode. See GH#3126.
// Return Value:
// - S_OK or suitable HRESULT code from failure to write (memory issues, invalid arg, etc.)
[[nodiscard]] HRESULT ApiRoutines::FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                   const wchar_t character,
                                                                   const size_t lengthToWrite,
                                                                   const til::point startingCoordinate,
                                                                   size_t& cellsModified,
                                                                   const bool enablePowershellShim) noexcept
{
    try
    {
        LockConsole();
        const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        // GH#3126 - This is a shim for powershell's `Clear-Host` function. In
        // the vintage console, `Clear-Host` is supposed to clear the entire
        // buffer. In conpty however, there's no difference between the viewport
        // and the entirety of the buffer. We're going to see if this API call
        // exactly matched the way we expect powershell to call it. If it does,
        // then let's manually emit a ^[[3J to the connected terminal, so that
        // their entire buffer will be cleared as well.
        if (enablePowershellShim)
        {
            auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            if (auto writer = gci.GetVtWriterForBuffer(&OutContext))
            {
                const auto currentBufferDimensions{ OutContext.GetBufferSize().Dimensions() };
                const auto wroteWholeBuffer = lengthToWrite == (currentBufferDimensions.area<size_t>());
                const auto startedAtOrigin = startingCoordinate == til::point{ 0, 0 };
                const auto wroteSpaces = character == UNICODE_SPACE;

                if (wroteWholeBuffer && startedAtOrigin && wroteSpaces)
                {
                    WriteClearScreen(OutContext);
                    writer.Submit();
                    cellsModified = lengthToWrite;
                    return S_OK;
                }
            }
        }

        cellsModified = FillConsoleImpl(OutContext, FillConsoleMode::FillCharacter, &character, lengthToWrite, startingCoordinate).lengthRead;
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - fills the screen buffer with the specified char
// Arguments:
// - OutContext - reference to screen buffer information.
// - character - ascii character to fill with
// - lengthToWrite - the number of elements to write
// - startingCoordinate - Screen buffer coordinate to begin writing to.
// - cellsModified - the number of elements written
// Return Value:
// - S_OK or suitable HRESULT code from failure to write (memory issues, invalid arg, etc.)
[[nodiscard]] HRESULT ApiRoutines::FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                   const char character,
                                                                   const size_t lengthToWrite,
                                                                   const til::point startingCoordinate,
                                                                   size_t& cellsModified) noexcept
try
{
    // In case ConvertToW throws causing an early return, set modified cells to 0.
    cellsModified = 0;

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // convert to wide chars and call W version
    const auto wchs = ConvertToW(gci.OutputCP, { &character, 1 });

    LOG_HR_IF(E_UNEXPECTED, wchs.size() > 1);

    return FillConsoleOutputCharacterWImpl(OutContext, wchs.at(0), lengthToWrite, startingCoordinate, cellsModified);
}
CATCH_RETURN()
