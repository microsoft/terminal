// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"

using namespace Microsoft::Console::VirtualTerminal;

// Routine Description:
// - Enables alternate scroll mode. This sends Cursor Up/down sequences when in the alternate buffer
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::EnableAlternateScroll(const bool enable) noexcept
{
    _mouseInputState.alternateScroll = enable;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void TerminalInput::UseAlternateScreenBuffer() noexcept
{
    _mouseInputState.inAlternateBuffer = true;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void TerminalInput::UseMainScreenBuffer() noexcept
{
    _mouseInputState.inAlternateBuffer = false;
}
