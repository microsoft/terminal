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

    THROW_IF_FAILED(SizeTToShort(width, &sColumns));
    THROW_IF_FAILED(SizeTToShort(height, &sRows));
    // We should do nothing if 0 is passed in for a size.
    RETURN_BOOL_IF_FALSE(width > 0 && height > 0);

    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    conApi.GetConsoleScreenBufferInfoEx(csbiex);

    const Viewport oldViewport = Viewport::FromInclusive(csbiex.srWindow);
    const Viewport newViewport = Viewport::FromDimensions(oldViewport.Origin(), sColumns, sRows);
    // Always resize the width of the console
    csbiex.dwSize.X = sColumns;
    // Only set the screen buffer's height if it's currently less than
    //  what we're requesting.
    if (sRows > csbiex.dwSize.Y)
    {
        csbiex.dwSize.Y = sRows;
    }

    // SetWindowInfo expect inclusive rects
    const auto sri = newViewport.ToInclusive();

    // SetConsoleScreenBufferInfoEx however expects exclusive rects
    const auto sre = newViewport.ToExclusive();
    csbiex.srWindow = sre;

    conApi.SetConsoleScreenBufferInfoEx(csbiex);
    conApi.SetWindowInfo(true, sri);
    return true;
}

// Routine Description:
// - Force the host to repaint the screen.
// Arguments:
// - conApi: The ConGetSet implementation to call back into.
// Return Value:
// - <none>
void DispatchCommon::s_RefreshWindow(ConGetSet& conApi)
{
    conApi.RefreshWindow();
}

// Routine Description:
// - Force the host to tell the renderer to not emit anything in response to the
//      next resize event. This is used by VT I/O to prevent a terminal from
//      requesting a resize, then having the renderer echo that to the terminal,
//      then having the terminal echo back to the host...
// Arguments:
// - conApi: The ConGetSet implementation to call back into.
// Return Value:
// - <none>
void DispatchCommon::s_SuppressResizeRepaint(ConGetSet& conApi)
{
    conApi.SuppressResizeRepaint();
}
