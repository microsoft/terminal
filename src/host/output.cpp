// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "_output.h"
#include "output.h"
#include "handle.h"

#include "getset.h"
#include "misc.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/Viewport.hpp"
#include "../types/inc/convert.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;

// This routine figures out what parameters to pass to CreateScreenBuffer based on the data from STARTUPINFO and the
// registry defaults, and then calls CreateScreenBuffer.
[[nodiscard]] NTSTATUS DoCreateScreenBuffer()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    FontInfo fiFont(gci.GetFaceName(),
                    gsl::narrow_cast<unsigned char>(gci.GetFontFamily()),
                    gci.GetFontWeight(),
                    gci.GetFontSize(),
                    gci.GetCodePage());

    // For East Asian version, we want to get the code page from the registry or shell32, so we can specify console
    // codepage by console.cpl or shell32. The default codepage is OEMCP.
    gci.CP = gci.GetCodePage();
    gci.OutputCP = gci.GetCodePage();
    gci.DefaultCP = gci.GetCodePage();
    gci.DefaultOutputCP = gci.GetCodePage();

    gci.Flags |= CONSOLE_USE_PRIVATE_FLAGS;

    auto Status = SCREEN_INFORMATION::CreateInstance(gci.GetWindowSize(),
                                                     fiFont,
                                                     gci.GetScreenBufferSize(),
                                                     TextAttribute{},
                                                     TextAttribute{ gci.GetPopupFillAttribute() },
                                                     gci.GetCursorSize(),
                                                     &gci.ScreenBuffers);

    // TODO: MSFT 9355013: This needs to be resolved. We increment it once with no handle to ensure it's never cleaned up
    // and one always exists for the renderer (and potentially other functions.)
    // It's currently a load-bearing piece of code. http://osgvsowi/9355013
    if (SUCCEEDED_NTSTATUS(Status))
    {
        gci.ScreenBuffers[0].IncrementOriginalScreenBuffer();
    }

    return Status;
}

// Routine Description:
// - This routine copies a rectangular region from the screen buffer to the screen buffer.
// Arguments:
// - screenInfo - reference to screen info
// - source - rectangle in source buffer to copy
// - targetOrigin - upper left coordinates of new location rectangle
static void _CopyRectangle(SCREEN_INFORMATION& screenInfo,
                           const Viewport& source,
                           const til::point targetOrigin)
{
    const auto sourceOrigin = source.Origin();

    // 0. If the source and the target are the same... we have nothing to do. Leave.
    if (sourceOrigin == targetOrigin)
    {
        return;
    }

    // 1. If we're copying entire rows of the buffer and moving them directly up or down,
    //    then we can send a rotate command to the underlying buffer to just adjust the
    //    row locations instead of copying or moving anything.
    {
        const auto bufferSize = screenInfo.GetBufferSize().Dimensions();
        const auto sourceFullRows = source.Width() == bufferSize.width;
        const auto verticalCopyOnly = source.Left() == 0 && targetOrigin.x == 0;
        if (sourceFullRows && verticalCopyOnly)
        {
            const auto delta = targetOrigin.y - source.Top();

            screenInfo.GetTextBuffer().ScrollRows(source.Top(), source.Height(), delta);

            return;
        }
    }

    // 2. We can move any other scenario in-place without copying. We just have to carefully
    //    choose which direction we walk through filling up the target so it doesn't accidentally
    //    erase the source material before it can be copied/moved to the new location.
    {
        const auto target = Viewport::FromDimensions(targetOrigin, source.Dimensions());
        const auto walkDirection = Viewport::DetermineWalkDirection(source, target);

        auto sourcePos = source.GetWalkOrigin(walkDirection);
        auto targetPos = target.GetWalkOrigin(walkDirection);

        // Note that we read two cells from the source before we start writing
        // to the target, so a two-cell DBCS character can't accidentally delete
        // itself when moving one cell horizontally.
        auto next = OutputCell(*screenInfo.GetCellDataAt(sourcePos));
        do
        {
            const auto current = next;
            source.WalkInBounds(sourcePos, walkDirection);
            next = OutputCell(*screenInfo.GetCellDataAt(sourcePos));
            screenInfo.GetTextBuffer().WriteLine(OutputCellIterator({ &current, 1 }), targetPos);
        } while (target.WalkInBounds(targetPos, walkDirection));

        auto& textBuffer = screenInfo.GetTextBuffer();
        ImageSlice::CopyBlock(textBuffer, source.ToExclusive(), textBuffer, target.ToExclusive());
    }
}

// Routine Description:
// - This routine reads a sequence of attributes from the screen buffer.
// Arguments:
// - screenInfo - reference to screen buffer information.
// - coordRead - Screen buffer coordinate to begin reading from.
// - amountToRead - the number of elements to read
// Return Value:
// - vector of attribute data
std::vector<WORD> ReadOutputAttributes(const SCREEN_INFORMATION& screenInfo,
                                       const til::point coordRead,
                                       const size_t amountToRead)
{
    // Short circuit. If nothing to read, leave early.
    if (amountToRead == 0)
    {
        return {};
    }

    // Short circuit, if reading out of bounds, leave early.
    if (!screenInfo.GetBufferSize().IsInBounds(coordRead))
    {
        return {};
    }

    // Get iterator to the position we should start reading at.
    auto it = screenInfo.GetCellDataAt(coordRead);
    // Count up the number of cells we've attempted to read.
    ULONG amountRead = 0;
    // Prepare the return value string.
    std::vector<WORD> retVal;
    // Reserve the number of cells. If we have >U+FFFF, it will auto-grow later and that's OK.
    retVal.reserve(amountToRead);

    // While we haven't read enough cells yet and the iterator is still valid (hasn't reached end of buffer)
    while (amountRead < amountToRead && it)
    {
        const auto legacyAttributes = it->TextAttr().GetLegacyAttributes();

        // If the first thing we read is trailing, pad with a space.
        // OR If the last thing we read is leading, pad with a space.
        if ((amountRead == 0 && it->DbcsAttr() == DbcsAttribute::Trailing) ||
            (amountRead == (amountToRead - 1) && it->DbcsAttr() == DbcsAttribute::Leading))
        {
            retVal.push_back(legacyAttributes);
        }
        else
        {
            retVal.push_back(legacyAttributes | GeneratePublicApiAttributeFormat(it->DbcsAttr()));
        }

        amountRead++;
        it++;
    }

    return retVal;
}

// Routine Description:
// - This routine reads a sequence of unicode characters from the screen buffer
// Arguments:
// - screenInfo - reference to screen buffer information.
// - coordRead - Screen buffer coordinate to begin reading from.
// - amountToRead - the number of elements to read
// Return Value:
// - wstring
std::wstring ReadOutputStringW(const SCREEN_INFORMATION& screenInfo,
                               const til::point coordRead,
                               const size_t amountToRead)
{
    // Short circuit. If nothing to read, leave early.
    if (amountToRead == 0)
    {
        return {};
    }

    // Short circuit, if reading out of bounds, leave early.
    if (!screenInfo.GetBufferSize().IsInBounds(coordRead))
    {
        return {};
    }

    // Get iterator to the position we should start reading at.
    auto it = screenInfo.GetCellDataAt(coordRead);

    // Count up the number of cells we've attempted to read.
    ULONG amountRead = 0;

    // Prepare the return value string.
    std::wstring retVal;
    retVal.reserve(amountToRead); // Reserve the number of cells. If we have >U+FFFF, it will auto-grow later and that's OK.

    // While we haven't read enough cells yet and the iterator is still valid (hasn't reached end of buffer)
    while (amountRead < amountToRead && it)
    {
        // If the first thing we read is trailing, pad with a space.
        // OR If the last thing we read is leading, pad with a space.
        if ((amountRead == 0 && it->DbcsAttr() == DbcsAttribute::Trailing) ||
            (amountRead == (amountToRead - 1) && it->DbcsAttr() == DbcsAttribute::Leading))
        {
            retVal += UNICODE_SPACE;
        }
        else
        {
            // Otherwise, add anything that isn't a trailing cell. (Trailings are duplicate copies of the leading.)
            if (it->DbcsAttr() != DbcsAttribute::Trailing)
            {
                auto chars = it->Chars();
                if (chars.size() > 1)
                {
                    chars = { &UNICODE_REPLACEMENT, 1 };
                }
                retVal += chars;
            }
        }

        amountRead++;
        it++;
    }

    return retVal;
}

// Routine Description:
// - This routine reads a sequence of ascii characters from the screen buffer
// Arguments:
// - screenInfo - reference to screen buffer information.
// - coordRead - Screen buffer coordinate to begin reading from.
// - amountToRead - the number of elements to read
// Return Value:
// - string of char data
std::string ReadOutputStringA(const SCREEN_INFORMATION& screenInfo,
                              const til::point coordRead,
                              const size_t amountToRead)
{
    const auto wstr = ReadOutputStringW(screenInfo, coordRead, amountToRead);

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return ConvertToA(gci.OutputCP, wstr);
}

void ScreenBufferSizeChange(const til::size coordNewSize)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        gci.pInputBuffer->Write(SynthesizeWindowBufferSizeEvent(coordNewSize));
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

// Routine Description:
// - This is simply a notifier method to let accessibility and renderers know that a region of the buffer
//   has been copied/moved to another location in a block fashion.
// Arguments:
// - screenInfo - The relevant screen buffer where data was moved
// - source - The viewport describing the region where data was copied from
// - fill - The viewport describing the area that was filled in with the fill character (uncovered area)
// - target - The viewport describing the region where data was copied to
static void _ScrollScreen(SCREEN_INFORMATION& screenInfo, const Viewport& source, const Viewport& fill, const Viewport& target)
{
    if (screenInfo.IsActiveScreenBuffer())
    {
        auto pNotifier = ServiceLocator::LocateAccessibilityNotifier();
        if (pNotifier != nullptr)
        {
            pNotifier->NotifyConsoleUpdateScrollEvent(target.Origin().x - source.Left(), target.Origin().y - source.RightInclusive());
        }
    }

    // Get the text buffer and send it commands.
    // It will figure out whether or not we're active and where the messages need to go.
    auto& textBuffer = screenInfo.GetTextBuffer();
    // Redraw anything in the target area
    textBuffer.TriggerRedraw(target);
    // Also redraw anything that was filled.
    textBuffer.TriggerRedraw(fill);
}

// Routine Description:
// - This routine copies ScrollRectangle to DestinationOrigin then fills in ScrollRectangle with Fill.
// - The scroll region is copied to a third buffer, the scroll region is filled, then the original contents of the scroll region are copied to the destination.
// Arguments:
// - screenInfo - reference to screen buffer info.
// - scrollRectGiven - Region to copy/move (source and size)
// - clipRectGiven - Optional clip region to contain buffer change effects
// - destinationOriginGiven - Upper left corner of target region.
// - fillCharGiven - Character to fill source region with.
// - fillAttrsGiven - Attribute to fill source region with.
// NOTE: Throws exceptions
void ScrollRegion(SCREEN_INFORMATION& screenInfo,
                  const til::inclusive_rect scrollRectGiven,
                  const std::optional<til::inclusive_rect> clipRectGiven,
                  const til::point destinationOriginGiven,
                  const wchar_t fillCharGiven,
                  const TextAttribute fillAttrsGiven)
{
    // ------ 1. PREP SOURCE ------
    // Set up the source viewport.
    auto source = Viewport::FromInclusive(scrollRectGiven);
    const auto originalSourceOrigin = source.Origin();

    // Alright, let's make sure that our source fits inside the buffer.
    const auto buffer = screenInfo.GetBufferSize();
    source = Viewport::Intersect(source, buffer);

    // If the source is no longer valid, then there's nowhere we can copy from
    // and also nowhere we can fill. We're done. Return early.
    if (!source.IsValid())
    {
        return;
    }

    // ------ 2. PREP CLIP ------
    // Now figure out our clipping area. If we have clipping specified, it will limit
    // the area that can be affected (targeted or filling) throughout this operation.
    // If there was no clip rect, we'll clip to the entire buffer size.
    auto clip = Viewport::FromInclusive(clipRectGiven.value_or(buffer.ToInclusive()));

    // OK, make sure that the clip rectangle also fits inside the buffer
    clip = Viewport::Intersect(buffer, clip);

    // ------ 3. PREP FILL ------
    // Then think about fill. We will fill in any area of the source that we copied from
    // with the fill character as long as it falls inside the clip region (the area
    // that is allowed to be affected).
    auto fill = Viewport::Intersect(clip, source);

    // If fill is no longer valid, then there is no area that we're allowed to write to
    // within the buffer. So we can just exit early.
    if (!fill.IsValid())
    {
        return;
    }

    // Determine the cell we will use to fill in any revealed/uncovered space.
    // We generally use exactly what was given to us.
    OutputCellIterator fillData(fillCharGiven, fillAttrsGiven);

    // However, if the character is null and we were given a null attribute (represented as legacy 0),
    // then we'll just fill with spaces and whatever the buffer's default colors are.
    if (fillCharGiven == UNICODE_NULL && fillAttrsGiven == TextAttribute{ 0 })
    {
        fillData = OutputCellIterator(UNICODE_SPACE, screenInfo.GetAttributes());
    }

    // ------ 4. PREP TARGET ------
    // Now it's time to think about the target. We're only given the origin of the target
    // because it is assumed that it will have the same relative dimensions as the original source.
    auto targetOrigin = destinationOriginGiven;

    // However, if we got to this point, we may have clipped the source because some part of it
    // fell outside of the buffer.
    // Apply any delta between the original source rectangle's origin and its current position to
    // the target origin.
    {
        auto currentSourceOrigin = source.Origin();
        targetOrigin.x += currentSourceOrigin.x - originalSourceOrigin.x;
        targetOrigin.y += currentSourceOrigin.y - originalSourceOrigin.y;
    }

    // And now the target viewport is the same size as the source viewport but at the different position.
    auto target = Viewport::FromDimensions(targetOrigin, source.Dimensions());

    // However, this might mean that the target is falling outside of the region we're allowed to edit
    // (the clip area). So we need to reduce the target to only inside the clip.
    // But backup the original target origin first, because we need to know how it has changed.
    const auto originalTargetOrigin = target.Origin();
    target = Viewport::Intersect(clip, target);

    // OK, if the target became smaller than before, we need to also adjust the source accordingly
    // so we don't waste time loading up/copying things that have no place to go within the target.
    {
        const auto currentTargetOrigin = target.Origin();
        auto sourceOrigin = source.Origin();
        sourceOrigin.x += currentTargetOrigin.x - originalTargetOrigin.x;
        sourceOrigin.y += currentTargetOrigin.y - originalTargetOrigin.y;

        source = Viewport::FromDimensions(sourceOrigin, target.Dimensions());
    }

    // ------ 5. COPY ------
    // If the target region is valid, let's do this.
    if (target.IsValid())
    {
        // Perform the copy from the source to the target.
        _CopyRectangle(screenInfo, source, target.Origin());

        // Notify the renderer and accessibility as to what moved and where.
        _ScrollScreen(screenInfo, source, fill, target);
    }

    // ------ 6. FILL ------
    // Now fill in anything that wasn't already touched by the copy above.
    // Fill as a single viewport represents the entire region we were allowed to
    // write into. But since we already copied, filling the whole thing might
    // overwrite what we just placed at the target.
    // So use the special subtraction function to get the viewports that fall
    // within the fill area but outside of the target area.
    const auto remaining = Viewport::Subtract(fill, target);

    // Apply the fill data to each of the viewports we're given here.
    for (size_t i = 0; i < remaining.size(); i++)
    {
        const auto& view = remaining.at(i);
        screenInfo.WriteRect(fillData, view);

        // If the region has image content it needs to be erased.
        ImageSlice::EraseBlock(screenInfo.GetTextBuffer(), view.ToExclusive());

        // If we're scrolling an area that encompasses the full buffer width,
        // then the filled rows should also have their line rendition reset.
        if (view.Width() == buffer.Width() && destinationOriginGiven.x == 0)
        {
            screenInfo.GetTextBuffer().ResetLineRenditionRange(view.Top(), view.BottomExclusive());
        }
    }
}

void SetActiveScreenBuffer(SCREEN_INFORMATION& screenInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.SetActiveOutputBuffer(screenInfo);

    // initialize cursor GH#4102 - Typically, the cursor is set to on by the
    // cursor blinker. Unfortunately, in conpty mode, there is no cursor
    // blinker. So, in conpty mode, we need to leave the cursor on always. The
    // cursor can still be set to hidden, and whether the cursor should be
    // blinking will still be passed through to the terminal, but internally,
    // the cursor should always be on.
    //
    // In particular, some applications make use of a calling
    // `SetConsoleScreenBuffer` and `SetCursorPosition` without printing any
    // text in between these calls. If we initialize the cursor to Off in conpty
    // mode, then the cursor will remain off until they print text. This can
    // lead to alignment problems in the terminal, because we won't move the
    // terminal's cursor in this _exact_ scenario.
    screenInfo.GetTextBuffer().GetCursor().SetIsOn(gci.IsInVtIoMode());

    // set font
    screenInfo.RefreshFontWithRenderer();

    // Empty input buffer.
    gci.pInputBuffer->FlushAllButKeys();

    // Set window size.
    screenInfo.PostUpdateWindowSize();

    // Write data to screen.
    WriteToScreen(screenInfo, screenInfo.GetViewport());
}

// TODO: MSFT 9450717 This should join the ProcessList class when CtrlEvents become moved into the server. https://osgvsowi/9450717
void CloseConsoleProcessState()
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // If there are no connected processes, sending control events is pointless as there's no one do send them to. In
    // this case we'll just exit conhost.

    // N.B. We can get into this state when a process has a reference to the console but hasn't connected. For example,
    //      when it's created suspended and never resumed.
    if (gci.ProcessHandleList.IsEmpty())
    {
        ServiceLocator::RundownAndExit(STATUS_SUCCESS);
    }

    HandleCtrlEvent(CTRL_CLOSE_EVENT);
}
