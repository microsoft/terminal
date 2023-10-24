// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "terminalInput.hpp"

#include "../types/inc/IInputEvent.hpp"
#include "../types/inc/utils.hpp"

using namespace Microsoft::Console::VirtualTerminal;

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
// - true if button is a button message to translate
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
// - true if buttonCode is a hover event to translate
static constexpr bool _isHoverMsg(const unsigned int buttonCode) noexcept
{
    return buttonCode == WM_MOUSEMOVE;
}

// Routine Description:
// - Determines if the input windows message code describes a mouse wheel event
// Parameters:
// - buttonCode - the message to decode.
// Return value:
// - true if buttonCode is a wheel event to translate
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
// - true if button is a button down event
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
    auto xvalue = 0;
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
    auto xvalue = 0;
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
static constexpr til::point _winToVTCoord(const til::point coordWinCoordinate) noexcept
{
    return { coordWinCoordinate.x + 1, coordWinCoordinate.y + 1 };
}

// Routine Description:
// - Encodes the given value as a default (or utf-8) encoding value.
//     32 is added so that the value 0 can be emitted as the printable character ' '.
// Parameters:
// - sCoordinateValue - the value to encode.
// Return value:
// - the encoded value.
static constexpr til::CoordType _encodeDefaultCoordinate(const til::CoordType sCoordinateValue) noexcept
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
    return _inputMode.any(Mode::DefaultMouseTracking, Mode::ButtonEventMouseTracking, Mode::AnyEventMouseTracking);
}

// Routine Description:
// - Attempt to handle the given mouse coordinates and windows button as a VT-style mouse event.
//     If the event should be transmitted in the selected mouse mode, then we'll try and
//     encode the event according to the rules of the encoding mode, and insert those characters into the input buffer.
// Parameters:
// - position - The windows coordinates (top,left = 0,0) of the mouse event
// - button - the message to decode.
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// - state - the state of the mouse buttons at this moment
// Return value:
// - Returns an empty optional if we didn't handle the mouse event and the caller can opt to handle it in some other way.
// - Returns a string if we successfully translated it into a VT input sequence.
TerminalInput::OutputType TerminalInput::HandleMouse(const til::point position, const unsigned int button, const short modifierKeyState, const short delta, const MouseButtonState state)
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
            OutputType out;
            if (IsTrackingMouseInput() || ShouldSendAlternateScroll(button, delta))
            {
                // Emplacing an empty string marks the return value as "we handled it"
                // but since it's empty it will not produce any actual output.
                out.emplace();
            }
            return out;
        }

        // We're ready to send this event through, but first we need to clear the accumulated;
        // delta. Otherwise, we'll dispatch every subsequent sub-delta event as its own event.
        _mouseInputState.accumulatedDelta = 0;
    }

    if (ShouldSendAlternateScroll(button, delta))
    {
        return _makeAlternateScrollOutput(delta);
    }

    if (IsTrackingMouseInput())
    {
        // isHover is only true for WM_MOUSEMOVE events
        const auto isHover = _isHoverMsg(button);
        const auto isButton = _isButtonMsg(button);

        const auto sameCoord = (position.x == _mouseInputState.lastPos.x) &&
                               (position.y == _mouseInputState.lastPos.y) &&
                               (_mouseInputState.lastButton == button);

        // If we have a WM_MOUSEMOVE, we need to know if any of the mouse
        //      buttons are actually pressed. If they are,
        //      _GetPressedButton will return the first pressed mouse button.
        // If it returns WM_LBUTTONUP, then we can assume that the mouse
        //      moved without a button being pressed.
        const auto realButton = isHover ? s_GetPressedButton(state) : button;

        // In default mode, only button presses/releases are sent
        // In ButtonEvent mode, changing coord hovers WITH A BUTTON PRESSED
        //      (WM_LBUTTONUP is our sentinel that no button was pressed) are also sent.
        // In AnyEvent, all coord change hovers are sent
        const auto physicalButtonPressed = realButton != WM_LBUTTONUP;

        if (isButton ||
            (isHover && _inputMode.test(Mode::ButtonEventMouseTracking) && ((!sameCoord) && (physicalButtonPressed))) ||
            (isHover && _inputMode.test(Mode::AnyEventMouseTracking) && !sameCoord))
        {
            if (_inputMode.any(Mode::ButtonEventMouseTracking, Mode::AnyEventMouseTracking))
            {
                _mouseInputState.lastPos.x = position.x;
                _mouseInputState.lastPos.y = position.y;
                _mouseInputState.lastButton = button;
            }

            if (_inputMode.test(Mode::Utf8MouseEncoding))
            {
                return _GenerateUtf8Sequence(position, realButton, isHover, modifierKeyState, delta);
            }
            else if (_inputMode.test(Mode::SgrMouseEncoding))
            {
                // For SGR encoding, if no physical buttons were pressed,
                // then we want to handle hovers with WM_MOUSEMOVE.
                // However, if we're dragging (WM_MOUSEMOVE with a button pressed),
                //      then use that pressed button instead.
                return _GenerateSGRSequence(position, physicalButtonPressed ? realButton : button, _isButtonDown(realButton), isHover, modifierKeyState, delta);
            }
            else
            {
                return _GenerateDefaultSequence(position, realButton, isHover, modifierKeyState, delta);
            }
        }
    }

    return {};
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
TerminalInput::OutputType TerminalInput::_GenerateDefaultSequence(const til::point position, const unsigned int button, const bool isHover, const short modifierKeyState, const short delta)
{
    // In the default, non-extended encoding scheme, coordinates above 94 shouldn't be supported,
    //   because (95+32+1)=128, which is not an ASCII character.
    // There are more details in _GenerateUtf8Sequence, but basically, we can't put anything above x80 into the input
    //   stream without bash.exe trying to convert it into utf8, and generating extra bytes in the process.
    if (position.x <= s_MaxDefaultCoordinate && position.y <= s_MaxDefaultCoordinate)
    {
        const auto vtCoords = _winToVTCoord(position);
        const auto encodedX = _encodeDefaultCoordinate(vtCoords.x);
        const auto encodedY = _encodeDefaultCoordinate(vtCoords.y);

        StringType format{ L"\x1b[Mbxy" };
        til::at(format, 3) = gsl::narrow_cast<wchar_t>(L' ' + _windowsButtonToXEncoding(button, isHover, modifierKeyState, delta));
        til::at(format, 4) = gsl::narrow_cast<wchar_t>(encodedX);
        til::at(format, 5) = gsl::narrow_cast<wchar_t>(encodedY);
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
TerminalInput::OutputType TerminalInput::_GenerateUtf8Sequence(const til::point position, const unsigned int button, const bool isHover, const short modifierKeyState, const short delta)
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
    if (position.x <= (SHORT_MAX - 33) && position.y <= (SHORT_MAX - 33))
    {
        const auto vtCoords = _winToVTCoord(position);
        const auto encodedX = _encodeDefaultCoordinate(vtCoords.x);
        const auto encodedY = _encodeDefaultCoordinate(vtCoords.y);

        StringType format{ L"\x1b[Mbxy" };
        // The short cast is safe because we know s_WindowsButtonToXEncoding  never returns more than xff
        til::at(format, 3) = gsl::narrow_cast<wchar_t>(L' ' + _windowsButtonToXEncoding(button, isHover, modifierKeyState, delta));
        til::at(format, 4) = gsl::narrow_cast<wchar_t>(encodedX);
        til::at(format, 5) = gsl::narrow_cast<wchar_t>(encodedY);
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
// - isDown - true if a mouse button was pressed.
// - isHover - true if the sequence is generated in response to a mouse hover
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
TerminalInput::OutputType TerminalInput::_GenerateSGRSequence(const til::point position, const unsigned int button, const bool isDown, const bool isHover, const short modifierKeyState, const short delta)
{
    // Format for SGR events is:
    // "\x1b[<%d;%d;%d;%c", xButton, x+1, y+1, fButtonDown? 'M' : 'm'
    const auto xbutton = _windowsButtonToSGREncoding(button, isHover, modifierKeyState, delta);
    return fmt::format(FMT_COMPILE(L"\x1b[<{};{};{}{}"), xbutton, position.x + 1, position.y + 1, isDown ? L'M' : L'm');
}

// Routine Description:
// - Returns true if we should translate the input event (button, sScrollDelta)
//      into an alternate scroll event instead of the default scroll event,
//      depending on if alternate scroll mode is enabled and we're in the alternate buffer.
// Parameters:
// - button: The mouse event code of the input event
// - delta: The scroll wheel delta of the input event
// Return value:
// True if the alternate buffer is active and alternate scroll mode is enabled and the event is a mouse wheel event.
bool TerminalInput::ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept
{
    const auto inAltBuffer{ _mouseInputState.inAlternateBuffer };
    const auto inAltScroll{ _inputMode.test(Mode::AlternateScroll) };
    const auto wasMouseWheel{ (button == WM_MOUSEWHEEL || button == WM_MOUSEHWHEEL) && delta != 0 };
    return inAltBuffer && inAltScroll && wasMouseWheel;
}

// Routine Description:
// - Sends a sequence to the input corresponding to cursor up / down depending on the sScrollDelta.
// Parameters:
// - delta: The scroll wheel delta of the input event
TerminalInput::OutputType TerminalInput::_makeAlternateScrollOutput(const short delta) const
{
    if (delta > 0)
    {
        return MakeOutput(_inputMode.test(Mode::CursorKey) ? ApplicationUpSequence : CursorUpSequence);
    }
    else
    {
        return MakeOutput(_inputMode.test(Mode::CursorKey) ? ApplicationDownSequence : CursorDownSequence);
    }
}
