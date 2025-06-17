// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "renderData.hpp"

#include "dbcs.h"
#include "handle.h"
#include "../interactivity/inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;
using Microsoft::Console::Interactivity::ServiceLocator;

// Routine Description:
// - Retrieves the viewport that applies over the data available in the GetTextBuffer() call
// Return Value:
// - Viewport describing rectangular region of TextBuffer that should be displayed.
Microsoft::Console::Types::Viewport RenderData::GetViewport() noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetViewport();
}

// Routine Description:
// - Retrieves the end position of the text buffer. We use
//   the cursor position as the text buffer end position
// Return Value:
// - til::point of the end position of the text buffer
til::point RenderData::GetTextBufferEndPosition() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto bufferSize = gci.GetActiveOutputBuffer().GetBufferSize();
    return { bufferSize.Width() - 1, bufferSize.BottomInclusive() };
}

// Routine Description:
// - Provides access to the text data that can be presented. Check GetViewport() for
//   the appropriate windowing.
// Return Value:
// - Text buffer with cell information for display
TextBuffer& RenderData::GetTextBuffer() const noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetTextBuffer();
}

// Routine Description:
// - Describes which font should be used for presenting text
// Return Value:
// - Font description structure
const FontInfo& RenderData::GetFontInfo() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetCurrentFont();
}

// Method Description:
// - Retrieves one span per line describing the area of the viewport
//   that should be highlighted in some way to represent a user-interactive selection
std::span<const til::point_span> RenderData::GetSelectionSpans() const noexcept
{
    return Selection::Instance().GetSelectionSpans();
}

// Method Description:
// - Retrieves one rectangle per line describing the area of the viewport
//   that should be highlighted
// Return Value:
// - Vector of rects describing the highlighted area
std::span<const til::point_span> RenderData::GetSearchHighlights() const noexcept
{
    return {};
}

// Method Description:
// - Lock the console for reading the contents of the buffer. Ensures that the
//      contents of the console won't be changed in the middle of a paint
//      operation.
//   Callers should make sure to also call RenderData::UnlockConsole once
//      they're done with any querying they need to do.
void RenderData::LockConsole() noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.LockConsole();
}

// Method Description:
// - Unlocks the console after a call to RenderData::LockConsole.
void RenderData::UnlockConsole() noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.UnlockConsole();
}

// Method Description:
// - Gets the cursor's position in the buffer, relative to the buffer origin.
// Arguments:
// - <none>
// Return Value:
// - the cursor's position in the buffer relative to the buffer origin.
til::point RenderData::GetCursorPosition() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.GetPosition();
}

// Method Description:
// - Returns whether the cursor is currently visible or not. If the cursor is
//      visible and blinking, this is true, even if the cursor has currently
//      blinked to the "off" state.
// Arguments:
// - <none>
// Return Value:
// - true if the cursor is set to the visible state, regardless of blink state
bool RenderData::IsCursorVisible() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.IsVisible();
}

// Method Description:
// - Returns whether the cursor is currently visually visible or not. If the
//      cursor is visible, and blinking, this will alternate between true and
//      false as the cursor blinks.
// Arguments:
// - <none>
// Return Value:
// - true if the cursor is currently visually visible, depending upon blink state
bool RenderData::IsCursorOn() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.IsVisible() && cursor.IsOn();
}

// Method Description:
// - The height of the cursor, out of 100, where 100 indicates the cursor should
//      be the full height of the cell.
// Arguments:
// - <none>
// Return Value:
// - height of the cursor, out of 100
ULONG RenderData::GetCursorHeight() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    // Determine cursor height
    auto ulHeight = cursor.GetSize();

    // Now adjust the height for the overwrite/insert mode. If we're in overwrite mode, IsDouble will be set.
    // When IsDouble is set, we either need to double the height of the cursor, or if it's already too big,
    // then we need to shrink it by half.
    if (cursor.IsDouble())
    {
        if (ulHeight > 50) // 50 because 50 percent is half of 100 percent which is the max size.
        {
            ulHeight >>= 1;
        }
        else
        {
            ulHeight <<= 1;
        }
    }

    return ulHeight;
}

// Method Description:
// - The CursorType of the cursor. The CursorType is used to determine what
//      shape the cursor should be.
// Arguments:
// - <none>
// Return Value:
// - the CursorType of the cursor.
CursorType RenderData::GetCursorStyle() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.GetType();
}

// Method Description:
// - Retrieves the operating system preference from Ease of Access for the pixel
//   width of the cursor. Useful for a bar-style cursor.
// Arguments:
// - <none>
// Return Value:
// - The suggested width of the cursor in pixels.
ULONG RenderData::GetCursorPixelWidth() const noexcept
{
    return ServiceLocator::LocateGlobals().cursorPixelWidth;
}

// Method Description:
// - Returns true if the cursor should be drawn twice as wide as usual because
//      the cursor is currently over a cell with a double-wide character in it.
// Arguments:
// - <none>
// Return Value:
// - true if the cursor should be drawn twice as wide as usual
bool RenderData::IsCursorDoubleWidth() const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().CursorIsDoubleWidth();
}

// Routine Description:
// - Checks the user preference as to whether grid line drawing is allowed around the edges of each cell.
// - This is for backwards compatibility with old behaviors in the legacy console.
// Return Value:
// - If true, line drawing information retrieved from the text buffer can/should be displayed.
// - If false, it should be ignored and never drawn
const bool RenderData::IsGridLineDrawingAllowed() noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto outputMode = gci.GetActiveOutputBuffer().OutputMode;
    // If virtual terminal output is set, grid line drawing is a must. It is also enabled
    // if someone explicitly asked for worldwide line drawing.
    if (WI_IsAnyFlagSet(outputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_LVB_GRID_WORLDWIDE))
    {
        return true;
    }
    else
    {
        // Otherwise, for compatibility reasons with legacy applications that used the additional CHAR_INFO bits by accident or for their own purposes,
        // we must enable grid line drawing only in a DBCS output codepage. (Line drawing historically only worked in DBCS codepages.)
        // The only known instance of this is Image for Windows by TeraByte, Inc. (TeraByte Unlimited) which used the bits accidentally and for no purpose
        //   (according to the app developer) in conjunction with the Borland Turbo C cgscrn library.
        return !!IsAvailableEastAsianCodePage(gci.OutputCP);
    }
}

// Routine Description:
// - Retrieves the title information to be displayed in the frame/edge of the window
// Return Value:
// - String with title information
const std::wstring_view RenderData::GetConsoleTitle() const noexcept
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetTitleAndPrefix();
}

// Method Description:
// - Get the hyperlink URI associated with a hyperlink ID
// Arguments:
// - The hyperlink ID
// Return Value:
// - The URI
const std::wstring RenderData::GetHyperlinkUri(uint16_t id) const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetTextBuffer().GetHyperlinkUriFromId(id);
}

// Method Description:
// - Get the custom ID associated with a hyperlink ID
// Arguments:
// - The hyperlink ID
// Return Value:
// - The custom ID if there was one, empty string otherwise
const std::wstring RenderData::GetHyperlinkCustomId(uint16_t id) const
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetTextBuffer().GetCustomIdFromId(id);
}

// For now, we ignore regex patterns in conhost
const std::vector<size_t> RenderData::GetPatternId(const til::point /*location*/) const
{
    return {};
}

// Routine Description:
// - Converts a text attribute into the RGB values that should be presented, applying
//   relevant table translation information and preferences.
// Return Value:
// - ARGB color values for the foreground and background
std::pair<COLORREF, COLORREF> RenderData::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    const auto& renderSettings = ServiceLocator::LocateGlobals().getConsoleInformation().GetRenderSettings();
    return renderSettings.GetAttributeColors(attr);
}

// Routine Description:
// - Determines whether the selection area is empty.
// Arguments:
// - <none>
// Return Value:
// - True if the selection variables contain valid selection data. False otherwise.
const bool RenderData::IsSelectionActive() const
{
    return Selection::Instance().IsAreaSelected();
}

const bool RenderData::IsBlockSelection() const noexcept
{
    return !Selection::Instance().IsLineSelection();
}

// Routine Description:
// - If a selection exists, clears it and restores the state.
//   Will also unblock a blocked write if one exists.
// Arguments:
// - <none> (Uses global state)
// Return Value:
// - <none>
void RenderData::ClearSelection()
{
    Selection::Instance().ClearSelection();
}

// Routine Description:
// - Resets the current selection and selects a new region from the start to end coordinates
// Arguments:
// - coordStart - Position to start selection area from
// - coordEnd - Position to select up to
// Return Value:
// - <none>
void RenderData::SelectNewRegion(const til::point coordStart, const til::point coordEnd)
{
    Selection::Instance().SelectNewRegion(coordStart, coordEnd);
}

const til::point_span* RenderData::GetSearchHighlightFocused() const noexcept
{
    return nullptr;
}

// Routine Description:
// - Gets the current selection anchor position
// Arguments:
// - none
// Return Value:
// - current selection anchor
const til::point RenderData::GetSelectionAnchor() const noexcept
{
    return Selection::Instance().GetSelectionAnchor();
}

// Routine Description:
// - Gets the current end selection anchor position
// Arguments:
// - none
// Return Value:
// - current selection anchor
const til::point RenderData::GetSelectionEnd() const noexcept
{
    // The selection area in ConHost is encoded as two things...
    //  - SelectionAnchor: the initial position where the selection was started
    //  - SelectionRect: the rectangular region denoting a portion of the buffer that is selected

    // The following is an excerpt from Selection::GetSelectionSpans
    // if the anchor (start of select) was in the top right or bottom left of the box,
    // we need to remove rectangular overlap in the middle.
    // e.g.
    // For selections with the anchor in the top left (A) or bottom right (B),
    // it is valid to maintain the inner rectangle (+) as part of the selection
    //               A+++++++================
    // ==============++++++++B
    // + and = are valid highlights in this scenario.
    // For selections with the anchor in in the top right (A) or bottom left (B),
    // we must remove a portion of the first/last line that lies within the rectangle (+)
    //               +++++++A=================
    // ==============B+++++++
    // Only = is valid for highlight in this scenario.
    // This is only needed for line selection. Box selection doesn't need to account for this.
    const auto selectionRect = Selection::Instance().GetSelectionRectangle();

    // To extract the end anchor from this rect, we need to know which corner of the rect is the SelectionAnchor
    // Then choose the opposite corner.
    const auto anchor = Selection::Instance().GetSelectionAnchor();

    const auto x_pos = (selectionRect.left == anchor.x) ? selectionRect.right : selectionRect.left;
    const auto y_pos = (selectionRect.top == anchor.y) ? selectionRect.bottom : selectionRect.top;

    return { x_pos, y_pos };
}
