// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"

using namespace Microsoft::Console::VirtualTerminal;

// Routine Description:
// - Either enables or disables UTF-8 extended mode encoding. This *should* cause
//      the coordinates of a mouse event to be encoded as a UTF-8 byte stream, however, because windows' input is
//      typically UTF-16 encoded, it emits a UTF-16 stream.
//   Does NOT enable or disable mouse mode by itself. This matches the behavior I found in Ubuntu terminals.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::SetUtf8ExtendedMode(const bool enable) noexcept
{
    _mouseInputState.extendedMode = enable ? ExtendedMode::Utf8 : ExtendedMode::None;
}

// Routine Description:
// - Either enables or disables SGR extended mode encoding. This causes the
//      coordinates of a mouse event to be emitted in a human readable format,
//      eg, x,y=203,504 -> "^[[<B;203;504M". This way, applications don't need to worry about character encoding.
//   Does NOT enable or disable mouse mode by itself. This matches the behavior I found in Ubuntu terminals.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::SetSGRExtendedMode(const bool enable) noexcept
{
    _mouseInputState.extendedMode = enable ? ExtendedMode::Sgr : ExtendedMode::None;
}

// Routine Description:
// - Either enables or disables mouse mode handling. Leaves the extended mode alone,
//      so if we disable then re-enable mouse mode without toggling an extended mode, the mode will persist.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::EnableDefaultTracking(const bool enable) noexcept
{
    _mouseInputState.trackingMode = enable ? TrackingMode::Default : TrackingMode::None;
    _mouseInputState.lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _mouseInputState.lastButton = 0;
}

// Routine Description:
// - Either enables or disables ButtonEvent mouse handling. Button Event mode
//      sends additional sequences when a button is pressed and the mouse changes character cells.
//   Leaves the extended mode alone, so if we disable then re-enable mouse mode
//      without toggling an extended mode, the mode will persist.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::EnableButtonEventTracking(const bool enable) noexcept
{
    _mouseInputState.trackingMode = enable ? TrackingMode::ButtonEvent : TrackingMode::None;
    _mouseInputState.lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _mouseInputState.lastButton = 0;
}

// Routine Description:
// - Either enables or disables AnyEvent mouse handling. Any Event mode sends sequences
//      for any and every mouse event, regardless if a button is pressed or not.
//   Leaves the extended mode alone, so if we disable then re-enable mouse mode
//      without toggling an extended mode, the mode will persist.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void TerminalInput::EnableAnyEventTracking(const bool enable) noexcept
{
    _mouseInputState.trackingMode = enable ? TrackingMode::AnyEvent : TrackingMode::None;
    _mouseInputState.lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _mouseInputState.lastButton = 0;
}

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
