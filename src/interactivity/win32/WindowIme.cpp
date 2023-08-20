// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/ServiceLocator.hpp"

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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenBuffer = gci.GetActiveOutputBuffer();

    const auto coordFont = screenBuffer.GetCurrentFont().GetSize();
    auto coordCursor = screenBuffer.GetTextBuffer().GetCursor().GetPosition();

    // Adjust the cursor position to be relative to the viewport.
    // This means that if the cursor is at row 30 in the buffer but the viewport is showing rows 20-40 right now on screen
    // that the "relative" position is that it is on the 11th line from the top (or 10th by index).
    // Correct by subtracting the top/left corner from the cursor's position.
    const auto srViewport = screenBuffer.GetViewport().ToInclusive();
    coordCursor.x -= srViewport.left;
    coordCursor.y -= srViewport.top;

    // Map the point to be just under the current cursor position. Convert from coordinate to pixels using font.
    POINT ptSuggestion;
    ptSuggestion.x = (coordCursor.x + 1) * coordFont.width;
    ptSuggestion.y = (coordCursor.y) * coordFont.height;

    // Adjust client point to screen point via HWND.
    ClientToScreen(ServiceLocator::LocateConsoleWindow()->GetWindowHandle(), &ptSuggestion);

    // Move into suggestion rectangle.
    RECT rcSuggestion{};
    rcSuggestion.top = rcSuggestion.bottom = ptSuggestion.y;
    rcSuggestion.left = rcSuggestion.right = ptSuggestion.x;

    // Add 1 line height and a few characters of width to represent the area where we're writing text.
    // This could be more exact by looking up the CONVAREA but it works well enough this way.
    // If there is a future issue with the pop-up window, tweak these metrics.
    rcSuggestion.bottom += coordFont.height;
    rcSuggestion.right += (coordFont.width * 10);

    return rcSuggestion;
}

// Routine Description:
// - This method gives a rectangle to where text box is currently rendered
//   such that the touch keyboard can pop up when the rectangle is tapped.
// Arguments:
// - <none>
// Return Value:
// - Rectangle specifying current text box area.
RECT GetTextBoxArea()
{
    return Microsoft::Console::Interactivity::ServiceLocator::LocateConsoleWindow()->GetWindowRect().to_win32_rect();
}
