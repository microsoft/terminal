// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "terminalInput.hpp"
#include "../types/inc/utils.hpp"

using namespace Microsoft::Console::VirtualTerminal;

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif
static constexpr int s_MaxDefaultCoordinate = 94;

// Alternate scroll sequences
static constexpr std::wstring_view CursorUpSequence{ L"\x1b[A" };
static constexpr std::wstring_view CursorDownSequence{ L"\x1b[B" };
static constexpr std::wstring_view ApplicationUpSequence{ L"\x1bOA" };
static constexpr std::wstring_view ApplicationDownSequence{ L"\x1bOB" };

// Routine Description:
// - Determines if the input windows message code describes a button event
//     (left, middle, right button and any of up, down or double click)
//     Also returns true for wheel events, which are buttons in *nix terminals
// Parameters:
// - button - the message to decode.
// Return value:
// - true iff button is a button message to translate
static constexpr bool _isButtonMsg(const unsigned int button) noexcept
{
    switch (button)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - Determines if the input windows message code describes a hover event
// Parameters:
// - buttonCode - the message to decode.
// Return value:
// - true iff buttonCode is a hover enent to translate
static constexpr bool _isHoverMsg(const unsigned int buttonCode) noexcept
{
    return buttonCode == WM_MOUSEMOVE;
}

// Routine Description:
// - Determines if the input windows message code describes a mouse wheel event
// Parameters:
// - buttonCode - the message to decode.
// Return value:
// - true iff buttonCode is a wheel event to translate
static constexpr bool _isWheelMsg(const unsigned int buttonCode) noexcept
{
    return buttonCode == WM_MOUSEWHEEL || buttonCode == WM_MOUSEHWHEEL;
}

// Routine Description:
// - Determines if the input windows message code describes a button press
//     (either down or doubleclick)
// Parameters:
// - button - the message to decode.
// Return value:
// - true iff button is a button down event
static constexpr bool _isButtonDown(const unsigned int button) noexcept
{
    switch (button)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        return true;
    default:
        return false;
    }
}

// Routine Description:
// - Retrieves which mouse button is currently pressed. This is needed because
//      MOUSEMOVE events do not also tell us if any mouse buttons are pressed during the move.
// Parameters:
// - state - the current state of which mouse buttons are pressed
// Return value:
// - a button corresponding to any pressed mouse buttons, else WM_LBUTTONUP if none are pressed.
constexpr unsigned int TerminalInput::s_GetPressedButton(const MouseButtonState state) noexcept
{
    // Will be treated as a release, or no button pressed.
    unsigned int button = WM_LBUTTONUP;
    if (state.isLeftButtonDown)
    {
        button = WM_LBUTTONDOWN;
    }
    else if (state.isMiddleButtonDown)
    {
        button = WM_MBUTTONDOWN;
    }
    else if (state.isRightButtonDown)
    {
        button = WM_RBUTTONDOWN;
    }
    return button;
}

// Routine Description:
// - translates the input windows mouse message into its equivalent X11 encoding.
// X Button Encoding:
// |7|6|5|4|3|2|1|0|
// | |W|H|M|C|S|B|B|
//  bits 0 and 1 are used for button:
//      00 - MB1 pressed (left)
//      01 - MB2 pressed (middle)
//      10 - MB3 pressed (right)
//      11 - released (none)
//  Next three bits indicate modifier keys:
//      0x04 - shift (This never makes it through, as our emulator is skipped when shift is pressed.)
//      0x08 - meta
//      0x10 - ctrl
//  32 (x20) is added for "hover" events:
//     "For example, motion into cell x,y with button 1 down is reported as `CSI M @ CxCy`.
//          ( @  = 32 + 0 (button 1) + 32 (motion indicator) ).
//      Similarly, motion with button 3 down is reported as `CSI M B CxCy`.
//          ( B  = 32 + 2 (button 3) + 32 (motion indicator) ).
//  64 (x40) is added for wheel events.
//      so wheel up? is 64, and wheel down? is 65.
//
// Parameters:
// - button - the message to decode.
// - isHover - whether or not this is a hover event
// - modifierKeyState - the modifier keys _in console format_
// - delta - scroll wheel delta
// Return value:
// - the int representing the equivalent X button encoding.
static constexpr int _windowsButtonToXEncoding(const unsigned int button,
                                               const bool isHover,
                                               const short modifierKeyState,
                                               const short delta) noexcept
{
    int xvalue = 0;
    switch (button)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
        xvalue = 0;
        break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        xvalue = 3;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        xvalue = 2;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        xvalue = 1;
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        xvalue = delta > 0 ? 0x40 : 0x41;
        break;
    default:
        xvalue = 0;
        break;
    }
    if (isHover)
    {
        xvalue += 0x20;
    }

    // Use Any here with the multi-flag constants -- they capture left/right key state
    WI_UpdateFlag(xvalue, 0x04, WI_IsAnyFlagSet(modifierKeyState, SHIFT_PRESSED));
    WI_UpdateFlag(xvalue, 0x08, WI_IsAnyFlagSet(modifierKeyState, ALT_PRESSED));
    WI_UpdateFlag(xvalue, 0x10, WI_IsAnyFlagSet(modifierKeyState, CTRL_PRESSED));

    return xvalue;
}

// Routine Description:
// - translates the input windows mouse message into its equivalent SGR encoding.
// This is nearly identical to the X encoding, with an important difference.
//      The button is always encoded as 0, 1, 2.
//      3 is reserved for mouse hovers with _no_ buttons pressed.
//  See MSFT:19461988 and https://github.com/Microsoft/console/issues/296
// Parameters:
// - button - the message to decode.
// - modifierKeyState - the modifier keys _in console format_
// Return value:
// - the int representing the equivalent X button encoding.
static constexpr int _windowsButtonToSGREncoding(const unsigned int button,
                                                 const bool isHover,
                                                 const short modifierKeyState,
                                                 const short delta) noexcept
{
    int xvalue = 0;
    switch (button)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        xvalue = 0;
        break;
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        xvalue = 2;
        break;
    case WM_MBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        xvalue = 1;
        break;
    case WM_MOUSEMOVE:
        xvalue = 3;
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        xvalue = delta > 0 ? 0x40 : 0x41;
        break;
    default:
        xvalue = 0;
        break;
    }
    if (isHover)
    {
        xvalue += 0x20;
    }

    // Use Any here with the multi-flag constants -- they capture left/right key state
    WI_UpdateFlag(xvalue, 0x04, WI_IsAnyFlagSet(modifierKeyState, SHIFT_PRESSED));
    WI_UpdateFlag(xvalue, 0x08, WI_IsAnyFlagSet(modifierKeyState, ALT_PRESSED));
    WI_UpdateFlag(xvalue, 0x10, WI_IsAnyFlagSet(modifierKeyState, CTRL_PRESSED));

    return xvalue;
}

// Routine Description:
// - Translates the given coord from windows coordinate space (origin=0,0) to VT space (origin=1,1)
// Parameters:
// - coordWinCoordinate - the coordinate to translate
// Return value:
// - the translated coordinate.
static constexpr COORD _winToVTCoord(const COORD coordWinCoordinate) noexcept
{
    return { coordWinCoordinate.X + 1, coordWinCoordinate.Y + 1 };
}

// Routine Description:
// - Encodes the given value as a default (or utf-8) encoding value.
//     32 is added so that the value 0 can be emitted as the printable character ' '.
// Parameters:
// - sCoordinateValue - the value to encode.
// Return value:
// - the encoded value.
static constexpr short _encodeDefaultCoordinate(const short sCoordinateValue) noexcept
{
    return sCoordinateValue + 32;
}

// Routine Description:
// - Relays if we are tracking mouse input
// Parameters:
// - <none>
// Return value:
// - true, if we are tracking mouse input. False, otherwise
bool TerminalInput::IsTrackingMouseInput() const noexcept
{
    return (_mouseInputState.trackingMode != TrackingMode::None);
}

// Routine Description:
// - Attempt to handle the given mouse coordinates and windows button as a VT-style mouse event.
//     If the event should be transmitted in the selected mouse mode, then we'll try and
//     encode the event according to the rules of the selected ExtendedMode, and insert those characters into the input buffer.
// Parameters:
// - position - The windows coordinates (top,left = 0,0) of the mouse event
// - button - the message to decode.
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// - state - the state of the mouse buttons at this moment
// Return value:
// - true if the event was handled and we should stop event propagation to the default window handler.
bool TerminalInput::HandleMouse(const COORD position,
                                const unsigned int button,
                                const short modifierKeyState,
                                const short delta,
                                const MouseButtonState state)
{
    if (Utils::Sign(delta) != Utils::Sign(_mouseInputState.accumulatedDelta))
    {
        // This works for wheel and non-wheel events and transitioning between wheel/non-wheel.
        // Non-wheel events have a delta of 0, which will fail to match the sign on
        // a real wheel event or the accumulated delta. Wheel events will be either + or -
        // and we only want to accumulate them if they match in sign.
        _mouseInputState.accumulatedDelta = 0;
    }

    if (_isWheelMsg(button))
    {
        _mouseInputState.accumulatedDelta += delta;
        if (std::abs(_mouseInputState.accumulatedDelta) < WHEEL_DELTA)
        {
            // If we're accumulating button presses of the same type, *and* those presses are
            // on the wheel, accumulate delta until we hit the amount required to dispatch one
            // "line" worth of scroll.
            // Mark the event as "handled" if we would have otherwise emitted a scroll event.
            return IsTrackingMouseInput() || _ShouldSendAlternateScroll(button, delta);
        }

        // We're ready to send this event through, but first we need to clear the accumulated;
        // delta. Otherwise, we'll dispatch every subsequent sub-delta event as its own event.
        _mouseInputState.accumulatedDelta = 0;
    }

    bool success = false;
    if (_ShouldSendAlternateScroll(button, delta))
    {
        success = _SendAlternateScroll(delta);
    }
    else
    {
        success = (_mouseInputState.trackingMode != TrackingMode::None);
        if (success)
        {
            // isHover is only true for WM_MOUSEMOVE events
            const bool isHover = _isHoverMsg(button);
            const bool isButton = _isButtonMsg(button);

            const bool sameCoord = (position.X == _mouseInputState.lastPos.X) &&
                                   (position.Y == _mouseInputState.lastPos.Y) &&
                                   (_mouseInputState.lastButton == button);

            // If we have a WM_MOUSEMOVE, we need to know if any of the mouse
            //      buttons are actually pressed. If they are,
            //      _GetPressedButton will return the first pressed mouse button.
            // If it returns WM_LBUTTONUP, then we can assume that the mouse
            //      moved without a button being pressed.
            const unsigned int realButton = isHover ? s_GetPressedButton(state) : button;

            // In default mode, only button presses/releases are sent
            // In ButtonEvent mode, changing coord hovers WITH A BUTTON PRESSED
            //      (WM_LBUTTONUP is our sentinel that no button was pressed) are also sent.
            // In AnyEvent, all coord change hovers are sent
            const bool physicalButtonPressed = realButton != WM_LBUTTONUP;

            success = (isButton && _mouseInputState.trackingMode != TrackingMode::None) ||
                      (isHover && _mouseInputState.trackingMode == TrackingMode::ButtonEvent && ((!sameCoord) && (physicalButtonPressed))) ||
                      (isHover && _mouseInputState.trackingMode == TrackingMode::AnyEvent && !sameCoord);

            if (success)
            {
                std::wstring sequence;
                switch (_mouseInputState.extendedMode)
                {
                case ExtendedMode::None:
                    sequence = _GenerateDefaultSequence(position,
                                                        realButton,
                                                        isHover,
                                                        modifierKeyState,
                                                        delta);
                    break;
                case ExtendedMode::Utf8:
                    sequence = _GenerateUtf8Sequence(position,
                                                     realButton,
                                                     isHover,
                                                     modifierKeyState,
                                                     delta);
                    break;
                case ExtendedMode::Sgr:
                    // For SGR encoding, if no physical buttons were pressed,
                    // then we want to handle hovers with WM_MOUSEMOVE.
                    // However, if we're dragging (WM_MOUSEMOVE with a button pressed),
                    //      then use that pressed button instead.
                    sequence = _GenerateSGRSequence(position,
                                                    physicalButtonPressed ? realButton : button,
                                                    _isButtonDown(realButton), // Use realButton here, to properly get the up/down state
                                                    isHover,
                                                    modifierKeyState,
                                                    delta);
                    break;
                case ExtendedMode::Urxvt:
                default:
                    success = false;
                    break;
                }

                success = !sequence.empty();

                if (success)
                {
                    _SendInputSequence(sequence);
                    success = true;
                }
                if (_mouseInputState.trackingMode == TrackingMode::ButtonEvent || _mouseInputState.trackingMode == TrackingMode::AnyEvent)
                {
                    _mouseInputState.lastPos.X = position.X;
                    _mouseInputState.lastPos.Y = position.Y;
                    _mouseInputState.lastButton = button;
                }
            }
        }
    }
    return success;
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the default scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking
// Parameters:
// - position - The windows coordinates (top,left = 0,0) of the mouse event
// - button - the message to decode.
// - isHover - true if the sequence is generated in response to a mouse hover
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// Return value:
// - The generated sequence. Will be empty if we couldn't generate.
std::wstring TerminalInput::_GenerateDefaultSequence(const COORD position,
                                                     const unsigned int button,
                                                     const bool isHover,
                                                     const short modifierKeyState,
                                                     const short delta)
{
    // In the default, non-extended encoding scheme, coordinates above 94 shouldn't be supported,
    //   because (95+32+1)=128, which is not an ASCII character.
    // There are more details in _GenerateUtf8Sequence, but basically, we can't put anything above x80 into the input
    //   stream without bash.exe trying to convert it into utf8, and generating extra bytes in the process.
    if (position.X <= s_MaxDefaultCoordinate && position.Y <= s_MaxDefaultCoordinate)
    {
        const COORD vtCoords = _winToVTCoord(position);
        const short encodedX = _encodeDefaultCoordinate(vtCoords.X);
        const short encodedY = _encodeDefaultCoordinate(vtCoords.Y);

        std::wstring format{ L"\x1b[Mbxy" };
        format.at(3) = ' ' + gsl::narrow_cast<short>(_windowsButtonToXEncoding(button, isHover, modifierKeyState, delta));
        format.at(4) = encodedX;
        format.at(5) = encodedY;
        return format;
    }

    return {};
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the UTF8 Extended scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Extended-coordinates
// Parameters:
// - position - The windows coordinates (top,left = 0,0) of the mouse event
// - button - the message to decode.
// - isHover - true if the sequence is generated in response to a mouse hover
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// Return value:
// - The generated sequence. Will be empty if we couldn't generate.
std::wstring TerminalInput::_GenerateUtf8Sequence(const COORD position,
                                                  const unsigned int button,
                                                  const bool isHover,
                                                  const short modifierKeyState,
                                                  const short delta)
{
    // So we have some complications here.
    // The windows input stream is typically encoded as UTF16.
    // Bash.exe knows this, and converts the utf16 input, character by character, into utf8, to send to wsl.
    // So, if we want to emit a char > x80 here, great. bash.exe will convert the x80 into xC280 and pass that along, which is great.
    //   The *nix application was expecting a utf8 stream, and it got one.
    // However, a normal windows program asks for utf8 mode, then it gets the utf16 encoded result. This is not what it wanted.
    //   It was looking for \x1b[M#\xC280y and got \x1b[M#\x0080y
    // Now, I'd argue that in requesting utf8 mode, the application should be enlightened enough to not want the utf16 input stream,
    //   and convert it the same way bash.exe does.
    // Though, the point could be made to place the utf8 bytes into the input, and read them that way.
    //   However, if we did this, bash.exe would translate those bytes thinking they're utf16, and x80->xC280->xC382C280
    //   So bash would also need to change, but how could it tell the difference between them? no real good way.
    // I'm going to emit a utf16 encoded value for now. Besides, if a windows program really wants it, just use the SGR mode, which is unambiguous.
    // TODO: Followup once the UTF-8 input stack is ready, MSFT:8509613
    if (position.X <= (SHORT_MAX - 33) && position.Y <= (SHORT_MAX - 33))
    {
        const COORD vtCoords = _winToVTCoord(position);
        const short encodedX = _encodeDefaultCoordinate(vtCoords.X);
        const short encodedY = _encodeDefaultCoordinate(vtCoords.Y);
        std::wstring format{ L"\x1b[Mbxy" };
        // The short cast is safe because we know s_WindowsButtonToXEncoding  never returns more than xff
        format.at(3) = ' ' + gsl::narrow_cast<short>(_windowsButtonToXEncoding(button, isHover, modifierKeyState, delta));
        format.at(4) = encodedX;
        format.at(5) = encodedY;
        return format;
    }

    return {};
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the SGR Extended scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Extended-coordinates
// Parameters:
// - position - The windows coordinates (top,left = 0,0) of the mouse event
// - button - the message to decode. WM_MOUSEMOVE is used for mouse hovers with no buttons pressed.
// - isDown - true iff a mouse button was pressed.
// - isHover - true if the sequence is generated in response to a mouse hover
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
// Return value:
// - true if we were able to successfully generate a sequence.
// On success, caller is responsible for delete[]ing *ppwchSequence.
std::wstring TerminalInput::_GenerateSGRSequence(const COORD position,
                                                 const unsigned int button,
                                                 const bool isDown,
                                                 const bool isHover,
                                                 const short modifierKeyState,
                                                 const short delta)
{
    // Format for SGR events is:
    // "\x1b[<%d;%d;%d;%c", xButton, x+1, y+1, fButtonDown? 'M' : 'm'
    const int xbutton = _windowsButtonToSGREncoding(button, isHover, modifierKeyState, delta);

    auto format = wil::str_printf<std::wstring>(L"\x1b[<%d;%d;%d%c", xbutton, position.X + 1, position.Y + 1, isDown ? L'M' : L'm');

    return format;
}

// Routine Description:
// - Returns true if we should translate the input event (button, sScrollDelta)
//      into an alternate scroll event instead of the default scroll event,
//      depending on if alternate scroll mode is enabled and we're in the alternate buffer.
// Parameters:
// - button: The mouse event code of the input event
// - delta: The scroll wheel delta of the input event
// Return value:
// True iff the alternate buffer is active and alternate scroll mode is enabled and the event is a mouse wheel event.
bool TerminalInput::_ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept
{
    return _mouseInputState.inAlternateBuffer &&
           _mouseInputState.alternateScroll &&
           (button == WM_MOUSEWHEEL || button == WM_MOUSEHWHEEL) && delta != 0;
}

// Routine Description:
// - Sends a sequence to the input corresponding to cursor up / down depending on the sScrollDelta.
// Parameters:
// - delta: The scroll wheel delta of the input event
// Return value:
// True iff the input sequence was sent successfully.
bool TerminalInput::_SendAlternateScroll(const short delta) const noexcept
{
    if (delta > 0)
    {
        _SendInputSequence(_cursorApplicationMode ? ApplicationUpSequence : CursorUpSequence);
    }
    else
    {
        _SendInputSequence(_cursorApplicationMode ? ApplicationDownSequence : CursorDownSequence);
    }
    return true;
}
