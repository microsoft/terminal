// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\inc\ServiceLocator.hpp"

#include "window.hpp"

#pragma hdrstop

// Routine Description:
// - This method gives a rectangle to where the command edit line text is currently rendered
//   such that the IME suggestion window can pop up in a suitable location adjacent to the given rectangle.
// Arguments:
// - <none>
// Return Value:
// - Rectangle specifying current command line edit area.
RECT GetImeSuggestionWindowPos()
{
    using Microsoft::Console::Interactivity::ServiceLocator;
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenBuffer = gci.GetActiveOutputBuffer();

    const COORD coordFont = screenBuffer.GetCurrentFont().GetSize();
    COORD coordCursor = screenBuffer.GetTextBuffer().GetCursor().GetPosition();

    // Adjust the cursor position to be relative to the viewport.
    // This means that if the cursor is at row 30 in the buffer but the viewport is showing rows 20-40 right now on screen
    // that the "relative" position is that it is on the 11th line from the top (or 10th by index).
    // Correct by subtracting the top/left corner from the cursor's position.
    SMALL_RECT const srViewport = screenBuffer.GetViewport().ToInclusive();
    coordCursor.X -= srViewport.Left;
    coordCursor.Y -= srViewport.Top;

    // Map the point to be just under the current cursor position. Convert from coordinate to pixels using font.
    POINT ptSuggestion;
    ptSuggestion.x = (coordCursor.X + 1) * coordFont.X;
    ptSuggestion.y = (coordCursor.Y) * coordFont.Y;

    // Adjust client point to screen point via HWND.
    ClientToScreen(ServiceLocator::LocateConsoleWindow()->GetWindowHandle(), &ptSuggestion);

    // Move into suggestion rectangle.
    RECT rcSuggestion = { 0 };
    rcSuggestion.top = rcSuggestion.bottom = ptSuggestion.y;
    rcSuggestion.left = rcSuggestion.right = ptSuggestion.x;

    // Add 1 line height and a few characters of width to represent the area where we're writing text.
    // This could be more exact by looking up the CONVAREA but it works well enough this way.
    // If there is a future issue with the pop-up window, tweak these metrics.
    rcSuggestion.bottom += coordFont.Y;
    rcSuggestion.right += (coordFont.X * 10);

    return rcSuggestion;
}
