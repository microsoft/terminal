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
    return conApi.ResizeWindow(width, height);
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
