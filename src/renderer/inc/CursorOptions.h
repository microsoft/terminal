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

#include "../../inc/conattrs.hpp"

namespace Microsoft::Console::Render
{
    struct CursorOptions
    {
        // Character cell in the grid to draw at
        // This is relative to the viewport, not the buffer.
        COORD coordCursor;

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
