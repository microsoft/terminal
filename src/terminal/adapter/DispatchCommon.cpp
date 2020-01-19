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
