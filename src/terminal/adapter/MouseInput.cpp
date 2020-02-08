// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "MouseInput.hpp"

using namespace Microsoft::Console::VirtualTerminal;

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\..\interactivity\inc\VtApiRedirection.hpp"
#endif

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
static constexpr short KeyPressed{ gsl::narrow_cast<short>(0x8000) };

// Alternate scroll sequences
static constexpr std::wstring_view CursorUpSequence{ L"\x1b[A" };
static constexpr std::wstring_view CursorDownSequence{ L"\x1b[B" };

MouseInput::MouseInput(const WriteInputEvents pfnWriteEvents) noexcept :
    _pfnWriteEvents(pfnWriteEvents),
    _lastPos{ -1, -1 },
    _lastButton{ 0 }
{
}

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
    bool isButton = false;
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
        isButton = true;
        break;
    }
    return isButton;
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
// - Determines if the input windows message code describes a button press
//     (either down or doubleclick)
// Parameters:
// - button - the message to decode.
// Return value:
// - true iff button is a button down event
static constexpr bool _isButtonDown(const unsigned int button) noexcept
{
    bool isButtonDown = false;
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
        isButtonDown = true;
        break;
    }
    return isButtonDown;
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
//      0x08 - ctrl
//      0x10 - meta
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
// - modifierKeyState - alt/ctrl/shift key hold state
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
    }
    if (isHover)
    {
        xvalue += 0x20;
    }

    // Shift will never pass through to us, because shift is used by the host to skip VT mouse and use the default handler.
    // TODO: MSFT:8804719 Add an option to disable/remap shift as a bypass for VT mousemode handling
    // xvalue += (modifierKeyState & MK_SHIFT) ? 0x04 : 0x00;
    xvalue += (modifierKeyState & MK_CONTROL) ? 0x08 : 0x00;
    // Unfortunately, we don't get meta/alt as a part of mouse events. Only Ctrl and Shift.
    // xvalue += (modifierKeyState & MK_META) ? 0x10 : 0x00;

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
    }
    if (isHover)
    {
        xvalue += 0x20;
    }

    // Shift will never pass through to us, because shift is used by the host to skip VT mouse and use the default handler.
    // TODO: MSFT:8804719 Add an option to disable/remap shift as a bypass for VT mousemode handling
    // xvalue += (modifierKeyState & MK_SHIFT) ? 0x04 : 0x00;
    xvalue += (modifierKeyState & MK_CONTROL) ? 0x08 : 0x00;
    // Unfortunately, we don't get meta/alt as a part of mouse events. Only Ctrl and Shift.
    // xvalue += (modifierKeyState & MK_META) ? 0x10 : 0x00;

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
//     32 is added so that the value 0 can be emitted as the printable characher ' '.
// Parameters:
// - sCoordinateValue - the value to encode.
// Return value:
// - the encoded value.
static constexpr short _encodeDefaultCoordinate(const short sCoordinateValue) noexcept
{
    return sCoordinateValue + 32;
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
// Return value:
// - true if the event was handled and we should stop event propagation to the default window handler.
bool MouseInput::HandleMouse(const COORD position,
                             const unsigned int button,
                             const short modifierKeyState,
                             const short delta)
{
    bool success = false;
    if (_ShouldSendAlternateScroll(button, delta))
    {
        success = _SendAlternateScroll(delta);
    }
    else
    {
        success = (_trackingMode != TrackingMode::None);
        if (success)
        {
            // isHover is only true for WM_MOUSEMOVE events
            const bool isHover = _isHoverMsg(button);
            const bool isButton = _isButtonMsg(button);

            const bool sameCoord = (position.X == _lastPos.X) &&
                                   (position.Y == _lastPos.Y) &&
                                   (_lastButton == button);

            // If we have a WM_MOUSEMOVE, we need to know if any of the mouse
            //      buttons are actually pressed. If they are,
            //      s_GetPressedButton will return the first pressed mouse button.
            // If it returns WM_LBUTTONUP, then we can assume that the mouse
            //      moved without a button being pressed.
            const unsigned int realButton = isHover ? s_GetPressedButton() : button;

            // In default mode, only button presses/releases are sent
            // In ButtonEvent mode, changing coord hovers WITH A BUTTON PRESSED
            //      (WM_LBUTTONUP is our sentinel that no button was pressed) are also sent.
            // In AnyEvent, all coord change hovers are sent
            const bool physicalButtonPressed = realButton != WM_LBUTTONUP;

            success = (isButton && _trackingMode != TrackingMode::None) ||
                      (isHover && _trackingMode == TrackingMode::ButtonEvent && ((!sameCoord) && (physicalButtonPressed))) ||
                      (isHover && _trackingMode == TrackingMode::AnyEvent && !sameCoord);
            if (success)
            {
                std::wstring sequence;
                switch (_extendedMode)
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
                if (_trackingMode == TrackingMode::ButtonEvent || _trackingMode == TrackingMode::AnyEvent)
                {
                    _lastPos.X = position.X;
                    _lastPos.Y = position.Y;
                    _lastButton = button;
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
std::wstring MouseInput::_GenerateDefaultSequence(const COORD position,
                                                  const unsigned int button,
                                                  const bool isHover,
                                                  const short modifierKeyState,
                                                  const short delta)
{
    // In the default, non-extended encoding scheme, coordinates above 94 shouldn't be supported,
    //   because (95+32+1)=128, which is not an ASCII character.
    // There are more details in _GenerateUtf8Sequence, but basically, we can't put anything above x80 into the input
    //   stream without bash.exe trying to convert it into utf8, and generating extra bytes in the process.
    if (position.X <= MouseInput::s_MaxDefaultCoordinate && position.Y <= MouseInput::s_MaxDefaultCoordinate)
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
std::wstring MouseInput::_GenerateUtf8Sequence(const COORD position,
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
// - button - the message to decode. WM_MOUSERMOVE is used for mouse hovers with no buttons pressed.
// - isDown - true iff a mouse button was pressed.
// - isHover - true if the sequence is generated in response to a mouse hover
// - modifierKeyState - the modifier keys pressed with this button
// - delta - the amount that the scroll wheel changed (should be 0 unless button is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
// Return value:
// - true if we were able to successfully generate a sequence.
// On success, caller is responsible for delete[]ing *ppwchSequence.
std::wstring MouseInput::_GenerateSGRSequence(const COORD position,
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
// - Either enables or disables UTF-8 extended mode encoding. This *should* cause
//      the coordinates of a mouse event to be encoded as a UTF-8 byte stream, however, because windows' input is
//      typically UTF-16 encoded, it emits a UTF-16 stream.
//   Does NOT enable or disable mouse mode by itself. This matches the behavior I found in Ubuntu terminals.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void MouseInput::SetUtf8ExtendedMode(const bool enable) noexcept
{
    _extendedMode = enable ? ExtendedMode::Utf8 : ExtendedMode::None;
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
void MouseInput::SetSGRExtendedMode(const bool enable) noexcept
{
    _extendedMode = enable ? ExtendedMode::Sgr : ExtendedMode::None;
}

// Routine Description:
// - Either enables or disables mouse mode handling. Leaves the extended mode alone,
//      so if we disable then re-enable mouse mode without toggling an extended mode, the mode will persist.
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableDefaultTracking(const bool enable) noexcept
{
    _trackingMode = enable ? TrackingMode::Default : TrackingMode::None;
    _lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
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
void MouseInput::EnableButtonEventTracking(const bool enable) noexcept
{
    _trackingMode = enable ? TrackingMode::ButtonEvent : TrackingMode::None;
    _lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
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
void MouseInput::EnableAnyEventTracking(const bool enable) noexcept
{
    _trackingMode = enable ? TrackingMode::AnyEvent : TrackingMode::None;
    _lastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
}

// Routine Description:
// - Sends the given sequence into the input callback specified by _pfnWriteEvents.
//      Typically, this inserts the characters into the input buffer as KeyDown KEY_EVENTs.
// Parameters:
// - sequence - sequence to send to _pfnWriteEvents
// Return value:
// <none>
void MouseInput::_SendInputSequence(const std::wstring_view sequence) const noexcept
{
    if (!sequence.empty())
    {
        std::deque<std::unique_ptr<IInputEvent>> events;
        try
        {
            for (const auto& wch : sequence)
            {
                events.push_back(std::make_unique<KeyEvent>(true, 1ui16, 0ui16, 0ui16, wch, 0));
            }

            _pfnWriteEvents(events);
        }
        catch (...)
        {
            LOG_HR(wil::ResultFromCaughtException());
        }
    }
}

// Routine Description:
// - Retrieves which mouse button is currently pressed. This is needed because
//      MOUSEMOVE events do not also tell us if any mouse buttons are pressed during the move.
// Parameters:
// <none>
// Return value:
// - a button corresponding to any pressed mouse buttons, else WM_LBUTTONUP if none are pressed.
unsigned int MouseInput::s_GetPressedButton() noexcept
{
    unsigned int button = WM_LBUTTONUP; // Will be treated as a release, or no button pressed.
    if (WI_IsFlagSet(GetKeyState(VK_LBUTTON), KeyPressed))
    {
        button = WM_LBUTTONDOWN;
    }
    else if (WI_IsFlagSet(GetKeyState(VK_MBUTTON), KeyPressed))
    {
        button = WM_MBUTTONDOWN;
    }
    else if (WI_IsFlagSet(GetKeyState(VK_RBUTTON), KeyPressed))
    {
        button = WM_RBUTTONDOWN;
    }
    return button;
}

// Routine Description:
// - Enables alternate scroll mode. This sends Cursor Up/down sequences when in the alternate buffer
// Parameters:
// - enable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableAlternateScroll(const bool enable) noexcept
{
    _alternateScroll = enable;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void MouseInput::UseAlternateScreenBuffer() noexcept
{
    _inAlternateBuffer = true;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void MouseInput::UseMainScreenBuffer() noexcept
{
    _inAlternateBuffer = false;
}

// Routine Description:
// - Returns true if we should translate the input event (button, sScrollDelta)
//      into an alternate scroll event instead of the default scroll event,
//      dependiong on if alternate scroll mode is enabled and we're in the alternate buffer.
// Parameters:
// - button: The mouse event code of the input event
// - delta: The scroll wheel delta of the input event
// Return value:
// True iff the alternate buffer is active and alternate scroll mode is enabled and the event is a mouse wheel event.
bool MouseInput::_ShouldSendAlternateScroll(const unsigned int button, const short delta) const noexcept
{
    return _inAlternateBuffer &&
           _alternateScroll &&
           (button == WM_MOUSEWHEEL || button == WM_MOUSEHWHEEL) && delta != 0;
}

// Routine Description:
// - Sends a sequence to the input coresponding to cursor up / down depending on the sScrollDelta.
// Parameters:
// - delta: The scroll wheel delta of the input event
// Return value:
// True iff the input sequence was sent successfully.
bool MouseInput::_SendAlternateScroll(const short delta) const noexcept
{
    if (delta > 0)
    {
        _SendInputSequence(CursorUpSequence);
    }
    else
    {
        _SendInputSequence(CursorDownSequence);
    }
    return true;
}
