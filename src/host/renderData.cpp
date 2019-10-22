// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "renderData.hpp"

#include "dbcs.h"
#include "handle.h"
#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;
using Microsoft::Console::Interactivity::ServiceLocator;

#pragma region IBaseData
// Routine Description:
// - Retrieves the viewport that applies over the data available in the GetTextBuffer() call
// Return Value:
// - Viewport describing rectangular region of TextBuffer that should be displayed.
Microsoft::Console::Types::Viewport RenderData::GetViewport() noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetViewport();
}

// Routine Description:
// - Provides access to the text data that can be presented. Check GetViewport() for
//   the appropriate windowing.
// Return Value:
// - Text buffer with cell information for display
const TextBuffer& RenderData::GetTextBuffer() noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetTextBuffer();
}

// Routine Description:
// - Describes which font should be used for presenting text
// Return Value:
// - Font description structure
const FontInfo& RenderData::GetFontInfo() noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetCurrentFont();
}

// Method Description:
// - Retrieves one rectangle per line describing the area of the viewport
//   that should be highlighted in some way to represent a user-interactive selection
// Return Value:
// - Vector of Viewports describing the area selected
std::vector<Viewport> RenderData::GetSelectionRects() noexcept
{
    std::vector<Viewport> result;

    try
    {
        for (const auto& select : Selection::Instance().GetSelectionRects())
        {
            result.emplace_back(Viewport::FromInclusive(select));
        }
    }
    CATCH_LOG();

    return result;
}

// Method Description:
// - Lock the console for reading the contents of the buffer. Ensures that the
//      contents of the console won't be changed in the middle of a paint
//      operation.
//   Callers should make sure to also call RenderData::UnlockConsole once
//      they're done with any querying they need to do.
void RenderData::LockConsole() noexcept
{
    ::LockConsole();
}

// Method Description:
// - Unlocks the console after a call to RenderData::LockConsole.
void RenderData::UnlockConsole() noexcept
{
    ::UnlockConsole();
}

#pragma endregion

#pragma region IRenderData
// Routine Description:
// - Retrieves the brush colors that should be used in absence of any other color data from
//   cells in the text buffer.
// Return Value:
// - TextAttribute containing the foreground and background brush color data.
const TextAttribute RenderData::GetDefaultBrushColors() noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetActiveOutputBuffer().GetAttributes();
}

// Method Description:
// - Gets the cursor's position in the buffer, relative to the buffer origin.
// Arguments:
// - <none>
// Return Value:
// - the cursor's position in the buffer relative to the buffer origin.
COORD RenderData::GetCursorPosition() const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.IsVisible() && !cursor.IsPopupShown();
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
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    // Determine cursor height
    ULONG ulHeight = cursor.GetSize();

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
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
// - Get the color of the cursor. If the color is INVALID_COLOR, the cursor
//      should be drawn by inverting the color of the cursor.
// Arguments:
// - <none>
// Return Value:
// - the color of the cursor.
COLORREF RenderData::GetCursorColor() const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& cursor = gci.GetActiveOutputBuffer().GetTextBuffer().GetCursor();
    return cursor.GetColor();
}

// Routine Description:
// - Retrieves overlays to be drawn on top of the main screen buffer area.
// - Overlays are drawn from first to last
//  (the highest overlay should be given last)
// Return Value:
// - Iterable set of overlays
const std::vector<Microsoft::Console::Render::RenderOverlay> RenderData::GetOverlays() const noexcept
{
    std::vector<Microsoft::Console::Render::RenderOverlay> overlays;

    try
    {
        // First retrieve the IME information and build overlays.
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& ime = gci.ConsoleIme;

        for (const auto& composition : ime.ConvAreaCompStr)
        {
            // Only send the overlay to the renderer on request if it's not supposed to be hidden at this moment.
            if (!composition.IsHidden())
            {
                // This is holding the data.
                const auto& textBuffer = composition.GetTextBuffer();

                // The origin of the text buffer above (top left corner) is supposed to sit at this
                // point within the visible viewport of the current window.
                const auto origin = composition.GetAreaBufferInfo().coordConView;

                // This is the area of the viewport that is actually in use relative to the text buffer itself.
                // (e.g. 0,0 is the origin of the text buffer above, not the placement within the visible viewport)
                const auto used = Viewport::FromInclusive(composition.GetAreaBufferInfo().rcViewCaWindow);

                overlays.emplace_back(Microsoft::Console::Render::RenderOverlay{ textBuffer, origin, used });
            }
        }
    }
    CATCH_LOG();

    return overlays;
}

// Method Description:
// - Returns true if the cursor should be drawn twice as wide as usual because
//      the cursor is currently over a cell with a double-wide character in it.
// Arguments:
// - <none>
// Return Value:
// - true if the cursor should be drawn twice as wide as usual
bool RenderData::IsCursorDoubleWidth() const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    // If virtual terminal output is set, grid line drawing is a must. It is always allowed.
    if (WI_IsFlagSet(gci.GetActiveOutputBuffer().OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING))
    {
        return true;
    }
    else
    {
        // If someone explicitly asked for worldwide line drawing, enable it.
        if (gci.IsGridRenderingAllowedWorldwide())
        {
            return true;
        }
        else
        {
            // Otherwise, for compatibility reasons with legacy applications that used the additional CHAR_INFO bits by accident or for their own purposes,
            // we must enable grid line drawing only in a DBCS output codepage. (Line drawing historically only worked in DBCS codepages.)
            // The only known instance of this is Image for Windows by TeraByte, Inc. (TeryByte Unlimited) which used the bits accidentally and for no purpose
            //   (according to the app developer) in conjunction with the Borland Turbo C cgscrn library.
            return !!IsAvailableEastAsianCodePage(gci.OutputCP);
        }
    }
}

// Routine Description:
// - Retrieves the title information to be displayed in the frame/edge of the window
// Return Value:
// - String with title information
const std::wstring RenderData::GetConsoleTitle() const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.GetTitleAndPrefix();
}

// Routine Description:
// - Converts a text attribute into the foreground RGB value that should be presented, applying
//   relevant table translation information and preferences.
// Return Value:
// - ARGB color value
const COLORREF RenderData::GetForegroundColor(const TextAttribute& attr) const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.LookupForegroundColor(attr);
}

// Routine Description:
// - Converts a text attribute into the background RGB value that should be presented, applying
//   relevant table translation information and preferences.
// Return Value:
// - ARGB color value
const COLORREF RenderData::GetBackgroundColor(const TextAttribute& attr) const noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    return gci.LookupBackgroundColor(attr);
}
#pragma endregion

#pragma region IUiaData
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
void RenderData::SelectNewRegion(const COORD coordStart, const COORD coordEnd)
{
    Selection::Instance().SelectNewRegion(coordStart, coordEnd);
}

// Routine Description:
// - Gets the current selection anchor position
// Arguments:
// - none
// Return Value:
// - current selection anchor
const COORD RenderData::GetSelectionAnchor() const
{
    return Selection::Instance().GetSelectionAnchor();
}

// Routine Description:
// - Given two points in the buffer space, color the selection between the two with the given attribute.
// - This will create an internal selection rectangle covering the two points, assume a line selection,
//   and use the first point as the anchor for the selection (as if the mouse click started at that point)
// Arguments:
// - coordSelectionStart - Anchor point (start of selection) for the region to be colored
// - coordSelectionEnd - Other point referencing the rectangle inscribing the selection area
// - attr - Color to apply to region.
void RenderData::ColorSelection(const COORD coordSelectionStart, const COORD coordSelectionEnd, const TextAttribute attr)
{
    Selection::Instance().ColorSelection(coordSelectionStart, coordSelectionEnd, attr);
}
#pragma endregion
