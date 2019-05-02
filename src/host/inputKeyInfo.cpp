// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "input.h"

INPUT_KEY_INFO::INPUT_KEY_INFO(const WORD wVirtualKeyCode, const ULONG ulControlKeyState) :
    _wVirtualKeyCode(wVirtualKeyCode),
    _ulControlKeyState(ulControlKeyState)
{
}

INPUT_KEY_INFO::~INPUT_KEY_INFO()
{
}

// Routine Description:
// - Gets the keyboard virtual key that was pressed.
//   This represents the actual keyboard key, not the modifiers (unless only the modifier was pressed).
// Arguments:
// - <none>
// Return Value:
// - WORD value of key which can be matched to VK_ constants.
const WORD INPUT_KEY_INFO::GetVirtualKey() const
{
    return _wVirtualKeyCode;
}

// Routine Description:
// - Specifies that the ctrl key was held when this particular input was received.
// Arguments:
// - <none>
// Return Value:
// - True if ctrl was held. False otherwise.
const bool INPUT_KEY_INFO::IsCtrlPressed() const
{
    return (_ulControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
}

// Routine Description:
// - Specifies that the alt key was held when this particular input was received.
// Arguments:
// - <none>
// Return Value:
// - True if alt was held. False otherwise.
const bool INPUT_KEY_INFO::IsAltPressed() const
{
    return (_ulControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
}

// Routine Description:
// - Specifies that the shift key was held when this particular input was received.
// Arguments:
// - <none>
// Return Value:
// - True if shift was held. False otherwise.
const bool INPUT_KEY_INFO::IsShiftPressed() const
{
    return (_ulControlKeyState & (SHIFT_PRESSED)) != 0;
}

// Routine Description:
// - Helps determine if this key input represents a ctrl+KEY combo
// Arguments:
// - <none>
// Return Value:
// - True if control only, not shift nor alt. False otherwise.
const bool INPUT_KEY_INFO::IsCtrlOnly() const
{
    return IsCtrlPressed() && !IsAltPressed() && !IsShiftPressed();
}

// Routine Description:
// - Helps determine if this key input represents a shift+KEY combo
// Arguments:
// - <none>
// Return Value:
// - True if shift only, not control nor alt. False otherwise.
const bool INPUT_KEY_INFO::IsShiftOnly() const
{
    return !IsCtrlPressed() && !IsAltPressed() && IsShiftPressed();
}

// Routine Description:
// - Helps determine if this key input represents a shift+ctrl+KEY combo
// Arguments:
// - <none>
// Return Value:
// - True if shift and control but not alt. False otherwise.
const bool INPUT_KEY_INFO::IsShiftAndCtrlOnly() const
{
    return IsCtrlPressed() && !IsAltPressed() && IsShiftPressed();
}

// Routine Description:
// - Helps determine if this key input represents a alt+KEY combo
// Arguments:
// - <none>
// Return Value:
// - True if alt but not shift or control. False otherwise.
const bool INPUT_KEY_INFO::IsAltOnly() const
{
    return IsAltPressed() && !IsCtrlPressed() && !IsShiftPressed();
}

// Routine Description:
// - Determines if there's any modifier for this key
// Arguments:
// - <none>
// Return Value:
// - True if no Alt, Ctrl, or Shift modifier is in place.
const bool INPUT_KEY_INFO::HasNoModifiers() const
{
    return !IsAltPressed() && !IsCtrlPressed() && !IsShiftPressed();
}
