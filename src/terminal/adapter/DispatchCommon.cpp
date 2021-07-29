// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DispatchCommon.hpp"
#include "../../types/inc/Viewport.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Method Description:
// - Resizes the window to the specified dimensions, in characters.
// Arguments:
// - conApi: The ConGetSet implementation to call back into.
// - width: The new width of the window, in columns
// - height: The new height of the window, in rows
// Return Value:
// True if handled successfully. False otherwise.
bool DispatchCommon::s_ResizeWindow(ConGetSet& conApi,
                                    const size_t width,
                                    const size_t height)
{
    SHORT sColumns = 0;
    SHORT sRows = 0;

    // We should do nothing if 0 is passed in for a size.
    bool success = SUCCEEDED(SizeTToShort(width, &sColumns)) &&
                   SUCCEEDED(SizeTToShort(height, &sRows)) &&
                   (width > 0 && height > 0);

    if (success)
    {
        CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
        csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
        success = conApi.GetConsoleScreenBufferInfoEx(csbiex);

        if (success)
        {
            const Viewport oldViewport = Viewport::FromInclusive(csbiex.srWindow);
            const Viewport newViewport = Viewport::FromDimensions(oldViewport.Origin(),
                                                                  sColumns,
                                                                  sRows);
            // Always resize the width of the console
            csbiex.dwSize.X = sColumns;
            // Only set the screen buffer's height if it's currently less than
            //  what we're requesting.
            if (sRows > csbiex.dwSize.Y)
            {
                csbiex.dwSize.Y = sRows;
            }

            // SetConsoleWindowInfo expect inclusive rects
            const auto sri = newViewport.ToInclusive();

            // SetConsoleScreenBufferInfoEx however expects exclusive rects
            const auto sre = newViewport.ToExclusive();
            csbiex.srWindow = sre;

            success = conApi.SetConsoleScreenBufferInfoEx(csbiex);
            if (success)
            {
                success = conApi.SetConsoleWindowInfo(true, sri);
            }
        }
    }
    return success;
}

// Routine Description:
// - Force the host to repaint the screen.
// Arguments:
// - conApi: The ConGetSet implementation to call back into.
// Return Value:
// True if handled successfully. False otherwise.
bool DispatchCommon::s_RefreshWindow(ConGetSet& conApi)
{
    return conApi.PrivateRefreshWindow();
}

// Routine Description:
// - Force the host to tell the renderer to not emit anything in response to the
//      next resize event. This is used by VT I/O to prevent a terminal from
//      requesting a resize, then having the renderer echo that to the terminal,
//      then having the terminal echo back to the host...
// Arguments:
// - conApi: The ConGetSet implementation to call back into.
// Return Value:
// True if handled successfully. False otherwise.
bool DispatchCommon::s_SuppressResizeRepaint(ConGetSet& conApi)
{
    return conApi.PrivateSuppressResizeRepaint();
}

bool DispatchCommon::s_EraseInDisplay(ConGetSet& conApi, const DispatchTypes::EraseType eraseType)
{
    RETURN_BOOL_IF_FALSE(eraseType <= DispatchTypes::EraseType::Scrollback);

    // First things first. If this is a "Scrollback" clear, then just do that.
    // Scrollback clears erase everything in the "scrollback" of a *nix terminal
    //      Everything that's scrolled off the screen so far.
    // Or if it's an Erase All, then we also need to handle that specially
    //      by moving the current contents of the viewport into the scrollback.
    if (eraseType == DispatchTypes::EraseType::Scrollback)
    {
        const bool eraseScrollbackResult = DispatchCommon::s_EraseScrollback(conApi);
        // GH#2715 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, we don't really
        // have a scrollback, but the attached terminal might.
        const bool isPty = conApi.IsConsolePty();
        return eraseScrollbackResult && (!isPty);
    }
    else if (eraseType == DispatchTypes::EraseType::All)
    {
        // GH#5683 - If this succeeded, but we're in a conpty, return `false` to
        // make the state machine propagate this ED sequence to the connected
        // terminal application. While we're in conpty mode, when the client
        // requests a Erase All operation, we need to manually tell the
        // connected terminal to do the same thing, so that the terminal will
        // move it's own buffer contents into the scrollback.
        const bool eraseAllResult = DispatchCommon::s_EraseAll(conApi);
        const bool isPty = conApi.IsConsolePty();
        return eraseAllResult && (!isPty);
    }

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (conApi.MoveToBottom() && conApi.GetConsoleScreenBufferInfoEx(csbiex));

    if (success)
    {
        // When erasing the display, every line that is erased in full should be
        // reset to single width. When erasing to the end, this could include
        // the current line, if the cursor is in the first column. When erasing
        // from the beginning, though, the current line would never be included,
        // because the cursor could never be in the rightmost column (assuming
        // the line is double width).
        if (eraseType == DispatchTypes::EraseType::FromBeginning)
        {
            const auto endRow = csbiex.dwCursorPosition.Y;
            conApi.PrivateResetLineRenditionRange(csbiex.srWindow.Top, endRow);
        }
        if (eraseType == DispatchTypes::EraseType::ToEnd)
        {
            const auto startRow = csbiex.dwCursorPosition.Y + (csbiex.dwCursorPosition.X > 0 ? 1 : 0);
            conApi.PrivateResetLineRenditionRange(startRow, csbiex.srWindow.Bottom);
        }

        // What we need to erase is grouped into 3 types:
        // 1. Lines before cursor
        // 2. Cursor Line
        // 3. Lines after cursor
        // We erase one or more of these based on the erase type:
        // A. FromBeginning - Erase 1 and Some of 2.
        // B. ToEnd - Erase some of 2 and 3.
        // C. All - Erase 1, 2, and 3.

        // 1. Lines before cursor line
        if (eraseType == DispatchTypes::EraseType::FromBeginning)
        {
            // For beginning and all, erase all complete lines before (above vertically) from the cursor position.
            for (SHORT startLine = csbiex.srWindow.Top; startLine < csbiex.dwCursorPosition.Y; startLine++)
            {
                success = DispatchCommon::s_EraseSingleLineHelper(conApi, csbiex, DispatchTypes::EraseType::All, startLine);

                if (!success)
                {
                    break;
                }
            }
        }

        if (success)
        {
            // 2. Cursor Line
            success = DispatchCommon::s_EraseSingleLineHelper(conApi, csbiex, eraseType, csbiex.dwCursorPosition.Y);
        }

        if (success)
        {
            // 3. Lines after cursor line
            if (eraseType == DispatchTypes::EraseType::ToEnd)
            {
                // For beginning and all, erase all complete lines after (below vertically) the cursor position.
                // Remember that the viewport bottom value is 1 beyond the viewable area of the viewport.
                for (SHORT startLine = csbiex.dwCursorPosition.Y + 1; startLine < csbiex.srWindow.Bottom; startLine++)
                {
                    success = DispatchCommon::s_EraseSingleLineHelper(conApi, csbiex, DispatchTypes::EraseType::All, startLine);

                    if (!success)
                    {
                        break;
                    }
                }
            }
        }
    }

    return success;
}

bool DispatchCommon::s_EraseAll(ConGetSet& conApi)
{
    return conApi.PrivateEraseAll();
}

// Routine Description:
// - Internal helper to erase one particular line of the buffer. Either from beginning to the cursor, from the cursor to the end, or the entire line.
// - Used by both erase line (used just once) and by erase screen (used in a loop) to erase a portion of the buffer.
// Arguments:
// - csbiex - Pointer to the console screen buffer that we will be erasing (and getting cursor data from within)
// - eraseType - Enumeration mode of which kind of erase to perform: beginning to cursor, cursor to end, or entire line.
// - lineId - The line number (array index value, starts at 0) of the line to operate on within the buffer.
//           - This is not aware of circular buffer. Line 0 is always the top visible line if you scrolled the whole way up the window.
// Return Value:
// - True if handled successfully. False otherwise.
bool DispatchCommon::s_EraseSingleLineHelper(ConGetSet& conApi,
                                             const CONSOLE_SCREEN_BUFFER_INFOEX& csbiex,
                                             const DispatchTypes::EraseType eraseType,
                                             const size_t lineId)
{
    COORD coordStartPosition = { 0 };
    if (FAILED(SizeTToShort(lineId, &coordStartPosition.Y)))
    {
        return false;
    }

    // determine start position from the erase type
    // remember that erases are inclusive of the current cursor position.
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
    case DispatchTypes::EraseType::All:
        coordStartPosition.X = 0; // from beginning and the whole line start from the left most edge of the buffer.
        break;
    case DispatchTypes::EraseType::ToEnd:
        coordStartPosition.X = csbiex.dwCursorPosition.X; // from the current cursor position (including it)
        break;
    }

    DWORD nLength = 0;

    // determine length of erase from erase type
    switch (eraseType)
    {
    case DispatchTypes::EraseType::FromBeginning:
        // +1 because if cursor were at the left edge, the length would be 0 and we want to paint at least the 1 character the cursor is on.
        nLength = csbiex.dwCursorPosition.X + 1;
        break;
    case DispatchTypes::EraseType::ToEnd:
    case DispatchTypes::EraseType::All:
        // Remember the .X value is 1 farther than the right most column in the buffer. Therefore no +1.
        nLength = conApi.PrivateGetLineWidth(lineId) - coordStartPosition.X;
        break;
    }

    // Note that the region is filled with the standard erase attributes.
    return conApi.PrivateFillRegion(coordStartPosition, nLength, L' ', true);
}

bool DispatchCommon::s_EraseScrollback(ConGetSet& conApi)
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(csbiex);
    // Make sure to reset the viewport (with MoveToBottom )to where it was
    //      before the user scrolled the console output
    bool success = (conApi.GetConsoleScreenBufferInfoEx(csbiex) && conApi.MoveToBottom());
    if (success)
    {
        const SMALL_RECT screen = csbiex.srWindow;
        const SHORT height = screen.Bottom - screen.Top;
        FAIL_FAST_IF(!(height > 0));
        const COORD cursor = csbiex.dwCursorPosition;

        // Rectangle to cut out of the existing buffer
        // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
        SMALL_RECT scroll = screen;
        scroll.Left = 0;
        scroll.Right = SHORT_MAX;
        // Paste coordinate for cut text above
        COORD destination;
        destination.X = 0;
        destination.Y = 0;

        // Typically a scroll operation should fill with standard erase attributes, but in
        // this case we need to use the default attributes, hence standardFillAttrs is false.
        success = conApi.PrivateScrollRegion(scroll, std::nullopt, destination, false);
        if (success)
        {
            // Clear everything after the viewport.
            const DWORD totalAreaBelow = csbiex.dwSize.X * (csbiex.dwSize.Y - height);
            const COORD coordBelowStartPosition = { 0, height };
            // Again we need to use the default attributes, hence standardFillAttrs is false.
            success = conApi.PrivateFillRegion(coordBelowStartPosition, totalAreaBelow, L' ', false);
            // Also reset the line rendition for all of the cleared rows.
            success = success && conApi.PrivateResetLineRenditionRange(height, csbiex.dwSize.Y);

            if (success)
            {
                // Move the viewport (CAN'T be done in one call with SetConsolescreenBufferInfoEx, because legacy)
                SMALL_RECT newViewport;
                newViewport.Left = screen.Left;
                newViewport.Top = 0;
                // SetConsoleWindowInfo uses an inclusive rect, while GetConsolescreenBufferInfo is exclusive
                newViewport.Right = screen.Right - 1;
                newViewport.Bottom = height - 1;
                success = conApi.SetConsoleWindowInfo(true, newViewport);

                if (success)
                {
                    // Move the cursor to the same relative location.
                    const COORD newcursor = { cursor.X, cursor.Y - screen.Top };
                    success = conApi.SetConsoleCursorPosition(newcursor);
                }
            }
        }
    }
    return success;
}
