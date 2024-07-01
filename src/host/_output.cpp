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
    WriteCharacter,
    FillAttribute,
    FillCharacter,
};

static void FillConsoleImpl(SCREEN_INFORMATION& screenInfo, FillConsoleMode mode, const uint16_t* data, const size_t lengthToWrite, const til::point startingCoordinate, size_t& cellsModified)
{
    // Set modified cells to 0 from the beginning.
    cellsModified = 0;

    if (lengthToWrite == 0)
    {
        return;
    }

    LockConsole();
    const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto routines = ServiceLocator::LocateGlobals().api;
    auto& screenBuffer = screenInfo.GetActiveBuffer();
    const auto bufferSize = screenBuffer.GetBufferSize();

    if (!bufferSize.IsInBounds(startingCoordinate))
    {
        return;
    }

    if (const auto io = gci.GetVtIo(&screenInfo))
    {
        const auto corkLock = io->Cork();

        const auto w = gsl::narrow_cast<uint64_t>(bufferSize.Width());
        const auto h = gsl::narrow_cast<uint64_t>(bufferSize.Height());
        const auto x = gsl::narrow_cast<uint64_t>(startingCoordinate.x);
        const auto y = gsl::narrow_cast<uint64_t>(startingCoordinate.y);
        const auto maxOffset = w * h;
        const auto startOffset = std::clamp<uint64_t>(y * w + x, 0, maxOffset);
        const auto endOffset = std::clamp<uint64_t>(startOffset + lengthToWrite, 0, maxOffset);
        const auto distance = gsl::narrow<size_t>(endOffset - startOffset);

        til::small_vector<CHAR_INFO, 1024> infos;
        infos.resize(lengthToWrite);

        til::point_span ps;
        ps.start = startingCoordinate;
        ps.end.x = gsl::narrow_cast<til::CoordType>((endOffset - 1) % w);
        ps.end.y = gsl::narrow_cast<til::CoordType>((endOffset - 1) / w);

        const auto rects = ps.split_rects(bufferSize.Width());

        for (const auto& r : rects)
        {
            const auto viewport = Viewport::FromInclusive(r);
            Viewport readViewport;
            THROW_IF_FAILED(routines->ReadConsoleOutputWImpl(screenInfo, infos, viewport, readViewport));

            switch (mode)
            {
            case FillConsoleMode::WriteAttribute:
                for (auto& info : infos)
                {
                    info.Attributes = *data++;
                }
                break;
            case FillConsoleMode::WriteCharacter:
                for (auto& info : infos)
                {
                    info.Char.UnicodeChar = *data++;
                }
                break;
            case FillConsoleMode::FillAttribute:
            {
                const auto fill = *data;
                for (auto& info : infos)
                {
                    info.Attributes = fill;
                }
                break;
            }
            case FillConsoleMode::FillCharacter:
            {
                const auto fill = *data;
                for (auto& info : infos)
                {
                    info.Char.UnicodeChar = fill;
                }
                break;
            }
            }

            Viewport writtenViewport;
            THROW_IF_FAILED(WriteConsoleOutputWImplHelper(screenInfo, infos, viewport.Width(), readViewport, writtenViewport));
        }

        if (io && io->BufferHasContent())
        {
            io->WriteCUP(screenInfo.GetTextBuffer().GetCursor().GetPosition());
        }

        cellsModified = distance;
    }
    else
    {
        OutputCellIterator it;

        switch (mode)
        {
        case FillConsoleMode::WriteAttribute:
            it = OutputCellIterator(TextAttribute(*data), lengthToWrite);
            break;
        case FillConsoleMode::WriteCharacter:
            it = OutputCellIterator(*data, lengthToWrite);
            break;
        case FillConsoleMode::FillAttribute:
            it = OutputCellIterator(TextAttribute(*data), lengthToWrite);
            break;
        case FillConsoleMode::FillCharacter:
            it = OutputCellIterator(*data, lengthToWrite);
            break;
        default:
            __assume(false);
        }

        const auto done = screenBuffer.Write(it, startingCoordinate);
        const auto cellsModifiedCoord = done.GetCellDistance(it);

        cellsModified = cellsModifiedCoord;

        // If we've overwritten image content, it needs to be erased.
        ImageSlice::EraseCells(screenInfo.GetTextBuffer(), startingCoordinate, cellsModified);
    }

    if (screenBuffer.HasAccessibilityEventing())
    {
        // Notify accessibility
        auto endingCoordinate = startingCoordinate;
        bufferSize.MoveInBounds(gsl::narrow<til::CoordType>(cellsModified), endingCoordinate);
        screenBuffer.NotifyAccessibilityEventing(startingCoordinate.x, startingCoordinate.y, endingCoordinate.x, endingCoordinate.y);
    }
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

        FillConsoleImpl(OutContext, FillConsoleMode::WriteAttribute, attrs.data(), attrs.size(), target, used);
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

        FillConsoleImpl(OutContext, FillConsoleMode::WriteCharacter, reinterpret_cast<const uint16_t*>(chars.data()), chars.size(), target, used);
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
                                                                  size_t& cellsModified) noexcept
{
    try
    {
        LockConsole();
        const auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        FillConsoleImpl(OutContext, FillConsoleMode::FillAttribute, &attribute, lengthToWrite, startingCoordinate, cellsModified);
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

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        if (const auto io = gci.GetVtIo(&OutContext))
        {
            // GH#3126 - This is a shim for powershell's `Clear-Host` function. In
            // the vintage console, `Clear-Host` is supposed to clear the entire
            // buffer. In conpty however, there's no difference between the viewport
            // and the entirety of the buffer. We're going to see if this API call
            // exactly matched the way we expect powershell to call it. If it does,
            // then let's manually emit a Full Reset (RIS).
            if (enablePowershellShim)
            {
                const auto currentBufferDimensions{ OutContext.GetBufferSize().Dimensions() };
                const auto wroteWholeBuffer = lengthToWrite == (currentBufferDimensions.area<size_t>());
                const auto startedAtOrigin = startingCoordinate == til::point{ 0, 0 };
                const auto wroteSpaces = character == UNICODE_SPACE;

                if (wroteWholeBuffer && startedAtOrigin && wroteSpaces)
                {
                    WriteCharsVT(OutContext, L"\033c");
                    cellsModified = lengthToWrite;
                    return S_OK;
                }
            }
        }

        FillConsoleImpl(OutContext, FillConsoleMode::FillCharacter, reinterpret_cast<const uint16_t*>(&character), lengthToWrite, startingCoordinate, cellsModified);
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
