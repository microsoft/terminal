// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"

using namespace Microsoft::Console::VirtualTerminal;

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
