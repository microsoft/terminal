/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CursorOptions.h

Abstract:
- A collection of state about the cursor that a render engine might need to display the cursor correctly.

Author(s):
- Mike Griese, 03-Jun-2020
--*/

#pragma once

#include "../../buffer/out/LineRendition.hpp"
#include "../../inc/conattrs.hpp"

namespace Microsoft::Console::Render
{
    struct CursorOptions
    {
        // Character cell in the grid to draw at
        // This is relative to the top of the viewport, not the buffer
        til::point coordCursor;

        // Left offset of the viewport, which may alter the horizontal position
        til::CoordType viewportLeft;

        // Line rendition of the current row, which can affect the cursor width
        LineRendition lineRendition;

        // For an underscore type _ cursor, how tall it should be as a % of cell height
        ULONG ulCursorHeightPercent;

        // For a vertical bar type | cursor, how many pixels wide it should be per ease of access preferences
        ULONG cursorPixelWidth;

        // Whether to draw the cursor 2 cells wide (+X from the coordinate given)
        bool fIsDoubleWidth;

        // Chooses a special cursor type like a full box, a vertical bar, etc.
        CursorType cursorType;

        // Specifies to use the color below instead of the default color
        bool fUseColor;

        // Color to use for drawing instead of the default
        COLORREF cursorColor;

        // Is the cursor currently visually visible?
        // If the cursor has blinked off, this is false.
        // if the cursor has blinked on, this is true.
        bool isOn;
    };
}
