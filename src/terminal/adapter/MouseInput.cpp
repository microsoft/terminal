// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include <windows.h>
#include "MouseInput.hpp"

#include "strsafe.h"
#include <stdio.h>
#include <stdlib.h>
using namespace Microsoft::Console::VirtualTerminal;

#define WIL_SUPPORT_BITOPERATION_PASCAL_NAMES
#include <wil\Common.h>

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "..\..\interactivity\inc\VtApiRedirection.hpp"
#endif

// This magic flag is "documented" at https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301(v=vs.85).aspx
// "If the high-order bit is 1, the key is down; otherwise, it is up."
#define KEY_PRESSED 0x8000

// Alternate scroll sequences
#define CURSOR_UP_SEQUENCE (L"\x1b[A")
#define CURSOR_DOWN_SEQUENCE (L"\x1b[B")
#define CCH_CURSOR_SEQUENCES (3)

MouseInput::MouseInput(const WriteInputEvents pfnWriteEvents) :
    _pfnWriteEvents(pfnWriteEvents),
    _coordLastPos{ -1, -1 },
    _lastButton{ 0 }
{
}

MouseInput::~MouseInput()
{
}

// Routine Description:
// - Determines if the input windows message code describes a button event
//     (left, middle, right button and any of up, down or double click)
//     Also returns true for wheel events, which are buttons in *nix terminals
// Parameters:
// - uiButton - the message to decode.
// Return value:
// - true iff uiButton is a button message to translate
bool MouseInput::s_IsButtonMsg(const unsigned int uiButton)
{
    bool fIsButton = false;
    switch (uiButton)
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
        fIsButton = true;
        break;
    }
    return fIsButton;
}

// Routine Description:
// - Determines if the input windows message code describes a hover event
// Parameters:
// - uiButtonCode - the message to decode.
// Return value:
// - true iff uiButtonCode is a hover enent to translate
bool MouseInput::s_IsHoverMsg(const unsigned int uiButtonCode)
{
    return uiButtonCode == WM_MOUSEMOVE;
}

// Routine Description:
// - Determines if the input windows message code describes a button press
//     (either down or doubleclick)
// Parameters:
// - uiButton - the message to decode.
// Return value:
// - true iff uiButton is a button down event
bool MouseInput::s_IsButtonDown(const unsigned int uiButton)
{
    bool fIsButtonDown = false;
    switch (uiButton)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        fIsButtonDown = true;
        break;
    }
    return fIsButtonDown;
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
// - uiButton - the message to decode.
// Return value:
// - the int representing the equivalent X button encoding.
int MouseInput::s_WindowsButtonToXEncoding(const unsigned int uiButton,
                                           const bool fIsHover,
                                           const short sModifierKeystate,
                                           const short sWheelDelta)
{
    int iXValue = 0;
    switch (uiButton)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
        iXValue = 0;
        break;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        iXValue = 3;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        iXValue = 2;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        iXValue = 1;
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        iXValue = sWheelDelta > 0 ? 0x40 : 0x41;
    }
    if (fIsHover)
    {
        iXValue += 0x20;
    }

    // Shift will never pass through to us, because shift is used by the host to skip VT mouse and use the default handler.
    // TODO: MSFT:8804719 Add an option to disable/remap shift as a bypass for VT mousemode handling
    // iXValue += (sModifierKeystate & MK_SHIFT) ? 0x04 : 0x00;
    iXValue += (sModifierKeystate & MK_CONTROL) ? 0x08 : 0x00;
    // Unfortunately, we don't get meta/alt as a part of mouse events. Only Ctrl and Shift.
    // iXValue += (sModifierKeystate & MK_META) ? 0x10 : 0x00;

    return iXValue;
}

// Routine Description:
// - translates the input windows mouse message into its equivalent SGR encoding.
// This is nearly identical to the X encoding, with an important difference.
//      The button is always encoded as 0, 1, 2.
//      3 is reserved for mouse hovers with _no_ buttons pressed.
//  See MSFT:19461988 and https://github.com/Microsoft/console/issues/296
// Parameters:
// - uiButton - the message to decode.
// Return value:
// - the int representing the equivalent X button encoding.
int MouseInput::s_WindowsButtonToSGREncoding(const unsigned int uiButton,
                                             const bool fIsHover,
                                             const short sModifierKeystate,
                                             const short sWheelDelta)
{
    int iXValue = 0;
    switch (uiButton)
    {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
        iXValue = 0;
        break;
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        iXValue = 2;
        break;
    case WM_MBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        iXValue = 1;
        break;
    case WM_MOUSEMOVE:
        iXValue = 3;
        break;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        iXValue = sWheelDelta > 0 ? 0x40 : 0x41;
    }
    if (fIsHover)
    {
        iXValue += 0x20;
    }

    // Shift will never pass through to us, because shift is used by the host to skip VT mouse and use the default handler.
    // TODO: MSFT:8804719 Add an option to disable/remap shift as a bypass for VT mousemode handling
    // iXValue += (sModifierKeystate & MK_SHIFT) ? 0x04 : 0x00;
    iXValue += (sModifierKeystate & MK_CONTROL) ? 0x08 : 0x00;
    // Unfortunately, we don't get meta/alt as a part of mouse events. Only Ctrl and Shift.
    // iXValue += (sModifierKeystate & MK_META) ? 0x10 : 0x00;

    return iXValue;
}

// Routine Description:
// - Attempt to handle the given mouse coordinates and windows button as a VT-style mouse event.
//     If the event should be transmitted in the selected mouse mode, then we'll try and
//     encode the event according to the rules of the selected ExtendedMode, and insert those characters into the input buffer.
// Parameters:
// - coordMousePosition - The windows coordinates (top,left = 0,0) of the mouse event
// - uiButton - the message to decode.
// - sModifierKeystate - the modifier keys pressed with this button
// - sWheelDelta - the amount that the scroll wheel changed (should be 0 unless uiButton is a WM_MOUSE*WHEEL)
// Return value:
// - true if the event was handled and we should stop event propagation to the default window handler.
bool MouseInput::HandleMouse(const COORD coordMousePosition,
                             const unsigned int uiButton,
                             const short sModifierKeystate,
                             const short sWheelDelta)
{
    bool fSuccess = false;
    if (_ShouldSendAlternateScroll(uiButton, sWheelDelta))
    {
        fSuccess = _SendAlternateScroll(sWheelDelta);
    }
    else
    {
        fSuccess = (_TrackingMode != TrackingMode::None);
        if (fSuccess)
        {
            // fIsHover is only true for WM_MOUSEMOVE events
            const bool fIsHover = s_IsHoverMsg(uiButton);
            const bool fIsButton = s_IsButtonMsg(uiButton);

            const bool fSameCoord = (coordMousePosition.X == _coordLastPos.X) &&
                                    (coordMousePosition.Y == _coordLastPos.Y) &&
                                    (_lastButton == uiButton);

            // If we have a WM_MOUSEMOVE, we need to know if any of the mouse
            //      buttons are actually pressed. If they are,
            //      s_GetPressedButton will return the first pressed mouse button.
            // If it returns WM_LBUTTONUP, then we can assume that the mouse
            //      moved without a button being pressed.
            const unsigned int uiRealButton = fIsHover ? s_GetPressedButton() : uiButton;

            // In default mode, only button presses/releases are sent
            // In ButtonEvent mode, changing coord hovers WITH A BUTTON PRESSED
            //      (WM_LBUTTONUP is our sentinel that no button was pressed) are also sent.
            // In AnyEvent, all coord change hovers are sent
            const bool physicalButtonPressed = uiRealButton != WM_LBUTTONUP;

            fSuccess = (fIsButton && _TrackingMode != TrackingMode::None) ||
                       (fIsHover && _TrackingMode == TrackingMode::ButtonEvent && ((!fSameCoord) && (physicalButtonPressed))) ||
                       (fIsHover && _TrackingMode == TrackingMode::AnyEvent && !fSameCoord);
            if (fSuccess)
            {
                wchar_t* pwchSequence = nullptr;
                size_t cchSequenceLength = 0;
                switch (_ExtendedMode)
                {
                case ExtendedMode::None:
                    fSuccess = _GenerateDefaultSequence(coordMousePosition,
                                                        uiRealButton,
                                                        fIsHover,
                                                        sModifierKeystate,
                                                        sWheelDelta,
                                                        &pwchSequence,
                                                        &cchSequenceLength);
                    break;
                case ExtendedMode::Utf8:
                    fSuccess = _GenerateUtf8Sequence(coordMousePosition,
                                                     uiRealButton,
                                                     fIsHover,
                                                     sModifierKeystate,
                                                     sWheelDelta,
                                                     &pwchSequence,
                                                     &cchSequenceLength);
                    break;
                case ExtendedMode::Sgr:
                    // For SGR encoding, if no physical buttons were pressed,
                    // then we want to handle hovers with WM_MOUSEMOVE.
                    // However, if we're dragging (WM_MOUSEMOVE with a button pressed),
                    //      then use that pressed button instead.
                    fSuccess = _GenerateSGRSequence(coordMousePosition,
                                                    physicalButtonPressed ? uiRealButton : uiButton,
                                                    s_IsButtonDown(uiRealButton), // Use uiRealButton here, to properly get the up/down state
                                                    fIsHover,
                                                    sModifierKeystate,
                                                    sWheelDelta,
                                                    &pwchSequence,
                                                    &cchSequenceLength);
                    break;
                case ExtendedMode::Urxvt:
                default:
                    fSuccess = false;
                    break;
                }
                if (fSuccess)
                {
                    _SendInputSequence(pwchSequence, cchSequenceLength);
                    delete[] pwchSequence;
                    fSuccess = true;
                }
                if (_TrackingMode == TrackingMode::ButtonEvent || _TrackingMode == TrackingMode::AnyEvent)
                {
                    _coordLastPos.X = coordMousePosition.X;
                    _coordLastPos.Y = coordMousePosition.Y;
                    _lastButton = uiButton;
                }
            }
        }
    }
    return fSuccess;
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the default scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking
// Parameters:
// - coordMousePosition - The windows coordinates (top,left = 0,0) of the mouse event
// - uiButton - the message to decode.
// - fIsHover - true if the sequence is generated in response to a mouse hover
// - sModifierKeystate - the modifier keys pressed with this button
// - sWheelDelta - the amount that the scroll wheel changed (should be 0 unless uiButton is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
// Return value:
// - true if we were able to successfully generate a sequence.
// On success, caller is responsible for delete[]ing *ppwchSequence.
bool MouseInput::_GenerateDefaultSequence(const COORD coordMousePosition,
                                          const unsigned int uiButton,
                                          const bool fIsHover,
                                          const short sModifierKeystate,
                                          const short sWheelDelta,
                                          _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                          _Out_ size_t* const pcchLength) const
{
    bool fSuccess = false;

    // In the default, non-extended encoding scheme, coordinates above 94 shouldn't be supported,
    //   because (95+32+1)=128, which is not an ASCII character.
    // There are more details in _GenerateUtf8Sequence, but basically, we can't put anything above x80 into the input
    //   stream without bash.exe trying to convert it into utf8, and generating extra bytes in the process.
    if (coordMousePosition.X <= MouseInput::s_MaxDefaultCoordinate && coordMousePosition.Y <= MouseInput::s_MaxDefaultCoordinate)
    {
        const COORD coordVTCoords = s_WinToVTCoord(coordMousePosition);
        const short sEncodedX = s_EncodeDefaultCoordinate(coordVTCoords.X);
        const short sEncodedY = s_EncodeDefaultCoordinate(coordVTCoords.Y);
        wchar_t* pwchFormat = new (std::nothrow) wchar_t[7]{ L"\x1b[Mbxy" };
        if (pwchFormat != nullptr)
        {
            pwchFormat[3] = ' ' + (short)s_WindowsButtonToXEncoding(uiButton, fIsHover, sModifierKeystate, sWheelDelta);
            pwchFormat[4] = sEncodedX;
            pwchFormat[5] = sEncodedY;

            *ppwchSequence = pwchFormat;
            *pcchLength = 7;
            fSuccess = true;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the UTF8 Extended scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Extended-coordinates
// Parameters:
// - coordMousePosition - The windows coordinates (top,left = 0,0) of the mouse event
// - uiButton - the message to decode.
// - fIsHover - true if the sequence is generated in response to a mouse hover
// - sModifierKeystate - the modifier keys pressed with this button
// - sWheelDelta - the amount that the scroll wheel changed (should be 0 unless uiButton is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
// Return value:
// - true if we were able to successfully generate a sequence.
// On success, caller is responsible for delete[]ing *ppwchSequence.
bool MouseInput::_GenerateUtf8Sequence(const COORD coordMousePosition,
                                       const unsigned int uiButton,
                                       const bool fIsHover,
                                       const short sModifierKeystate,
                                       const short sWheelDelta,
                                       _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                       _Out_ size_t* const pcchLength) const
{
    bool fSuccess = false;

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
    if (coordMousePosition.X <= (SHORT_MAX - 33) && coordMousePosition.Y <= (SHORT_MAX - 33))
    {
        const COORD coordVTCoords = s_WinToVTCoord(coordMousePosition);
        const short sEncodedX = s_EncodeDefaultCoordinate(coordVTCoords.X);
        const short sEncodedY = s_EncodeDefaultCoordinate(coordVTCoords.Y);
        wchar_t* pwchFormat = new (std::nothrow) wchar_t[7]{ L"\x1b[Mbxy" };
        if (pwchFormat != nullptr)
        {
            // The short cast is safe because we know s_WindowsButtonToXEncoding  never returns more than xff
            pwchFormat[3] = ' ' + (short)s_WindowsButtonToXEncoding(uiButton, fIsHover, sModifierKeystate, sWheelDelta);
            pwchFormat[4] = sEncodedX;
            pwchFormat[5] = sEncodedY;

            *ppwchSequence = pwchFormat;
            *pcchLength = 7;
            fSuccess = true;
        }
    }

    return fSuccess;
}

// Routine Description:
// - Generates a sequence encoding the mouse event according to the SGR Extended scheme.
//     see http://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Extended-coordinates
// Parameters:
// - coordMousePosition - The windows coordinates (top,left = 0,0) of the mouse event
// - uiButton - the message to decode. WM_MOUSERMOVE is used for mouse hovers with no buttons pressed.
// - isDown - true iff a mouse button was pressed.
// - fIsHover - true if the sequence is generated in response to a mouse hover
// - sModifierKeystate - the modifier keys pressed with this button
// - sWheelDelta - the amount that the scroll wheel changed (should be 0 unless uiButton is a WM_MOUSE*WHEEL)
// - ppwchSequence - On success, where to put the pointer to the generated sequence
// - pcchLength - On success, where to put the length of the generated sequence
// Return value:
// - true if we were able to successfully generate a sequence.
// On success, caller is responsible for delete[]ing *ppwchSequence.
bool MouseInput::_GenerateSGRSequence(const COORD coordMousePosition,
                                      const unsigned int uiButton,
                                      const bool isDown,
                                      const bool fIsHover,
                                      const short sModifierKeystate,
                                      const short sWheelDelta,
                                      _Outptr_result_buffer_(*pcchLength) wchar_t** const ppwchSequence,
                                      _Out_ size_t* const pcchLength) const
{
    // Format for SGR events is:
    // "\x1b[<%d;%d;%d;%c", xButton, x+1, y+1, fButtonDown? 'M' : 'm'
    bool fSuccess = false;
    const int iXButton = s_WindowsButtonToSGREncoding(uiButton, fIsHover, sModifierKeystate, sWheelDelta);

#pragma warning(push)
#pragma warning(disable : 4996)
// Disable 4996 - The _s version of _snprintf doesn't return the cch if the buffer is null, and we need the cch
#pragma prefast(suppress : 28719, "Using the output of _snwprintf to determine cch. _snwprintf_s used below.")
    int iNeededChars = _snwprintf(nullptr, 0, L"\x1b[<%d;%d;%d%c", iXButton, coordMousePosition.X + 1, coordMousePosition.Y + 1, isDown ? L'M' : L'm');

#pragma warning(pop)

    iNeededChars += 1; // for null

    wchar_t* pwchFormat = new (std::nothrow) wchar_t[iNeededChars];
    if (pwchFormat != nullptr)
    {
        int iTakenChars = _snwprintf_s(pwchFormat,
                                       iNeededChars,
                                       iNeededChars,
                                       L"\x1b[<%d;%d;%d%c",
                                       iXButton,
                                       coordMousePosition.X + 1,
                                       coordMousePosition.Y + 1,
                                       isDown ? L'M' : L'm');
        if (iTakenChars == iNeededChars - 1) // again, adjust for null
        {
            *ppwchSequence = pwchFormat;
            *pcchLength = iTakenChars;
            fSuccess = true;
        }
        else
        {
            delete[] pwchFormat;
        }
    }
    return fSuccess;
}

// Routine Description:
// - Either enables or disables UTF-8 extended mode encoding. This *should* cause
//      the coordinates of a mouse event to be encoded as a UTF-8 byte stream, however, because windows' input is
//      typically UTF-16 encoded, it emits a UTF-16 stream.
//   Does NOT enable or disable mouse mode by itself. This matches the behavior I found in Ubuntu terminals.
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::SetUtf8ExtendedMode(const bool fEnable)
{
    _ExtendedMode = fEnable ? ExtendedMode::Utf8 : ExtendedMode::None;
}

// Routine Description:
// - Either enables or disables SGR extended mode encoding. This causes the
//      coordinates of a mouse event to be emitted in a human readable format,
//      eg, x,y=203,504 -> "^[[<B;203;504M". This way, applications don't need to worry about character encoding.
//   Does NOT enable or disable mouse mode by itself. This matches the behavior I found in Ubuntu terminals.
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::SetSGRExtendedMode(const bool fEnable)
{
    _ExtendedMode = fEnable ? ExtendedMode::Sgr : ExtendedMode::None;
}

// Routine Description:
// - Either enables or disables mouse mode handling. Leaves the extended mode alone,
//      so if we disable then re-enable mouse mode without toggling an extended mode, the mode will persist.
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableDefaultTracking(const bool fEnable)
{
    _TrackingMode = fEnable ? TrackingMode::Default : TrackingMode::None;
    _coordLastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
}

// Routine Description:
// - Either enables or disables ButtonEvent mouse handling. Button Event mode
//      sends additional sequences when a button is pressed and the mouse changes character cells.
//   Leaves the extended mode alone, so if we disable then re-enable mouse mode
//      without toggling an extended mode, the mode will persist.
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableButtonEventTracking(const bool fEnable)
{
    _TrackingMode = fEnable ? TrackingMode::ButtonEvent : TrackingMode::None;
    _coordLastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
}

// Routine Description:
// - Either enables or disables AnyEvent mouse handling. Any Event mode sends sequences
//      for any and every mouse event, regardless if a button is pressed or not.
//   Leaves the extended mode alone, so if we disable then re-enable mouse mode
//      without toggling an extended mode, the mode will persist.
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableAnyEventTracking(const bool fEnable)
{
    _TrackingMode = fEnable ? TrackingMode::AnyEvent : TrackingMode::None;
    _coordLastPos = { -1, -1 }; // Clear out the last saved mouse position & button.
    _lastButton = 0;
}

// Routine Description:
// - Sends the given sequence into the input callback specified by _pfnWriteEvents.
//      Typically, this inserts the characters into the input buffer as KeyDown KEY_EVENTs.
// Parameters:
// - pwszSequence - sequence to send to _pfnWriteEvents
// - cchLength - the length of pwszSequence
// Return value:
// <none>
void MouseInput::_SendInputSequence(_In_reads_(cchLength) const wchar_t* const pwszSequence,
                                    const size_t cchLength) const
{
    size_t cch = 0;
    // + 1 to max sequence length for null terminator count which is required by StringCchLengthW
    if (SUCCEEDED(StringCchLengthW(pwszSequence, cchLength + 1, &cch)) && cch > 0 && cch < DWORD_MAX)
    {
        std::deque<std::unique_ptr<IInputEvent>> events;
        try
        {
            for (size_t i = 0; i < cch; ++i)
            {
                events.push_back(std::make_unique<KeyEvent>(true, 1ui16, 0ui16, 0ui16, pwszSequence[i], 0));
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
// - Translates the given coord from windows coordinate space (origin=0,0) to VT space (origin=1,1)
// Parameters:
// - coordWinCoordinate - the coordinate to translate
// Return value:
// - the translated coordinate.
COORD MouseInput::s_WinToVTCoord(const COORD coordWinCoordinate)
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
short MouseInput::s_EncodeDefaultCoordinate(const short sCoordinateValue)
{
    return sCoordinateValue + 32;
}

// Routine Description:
// - Retrieves which mouse button is currently pressed. This is needed because
//      MOUSEMOVE events do not also tell us if any mouse buttons are pressed during the move.
// Parameters:
// <none>
// Return value:
// - a uiButton corresponding to any pressed mouse buttons, else WM_LBUTTONUP if none are pressed.
unsigned int MouseInput::s_GetPressedButton()
{
    unsigned int uiButton = WM_LBUTTONUP; // Will be treated as a release, or no button pressed.
    if (WI_IsFlagSet(GetKeyState(VK_LBUTTON), KEY_PRESSED))
    {
        uiButton = WM_LBUTTONDOWN;
    }
    else if (WI_IsFlagSet(GetKeyState(VK_MBUTTON), KEY_PRESSED))
    {
        uiButton = WM_MBUTTONDOWN;
    }
    else if (WI_IsFlagSet(GetKeyState(VK_RBUTTON), KEY_PRESSED))
    {
        uiButton = WM_RBUTTONDOWN;
    }
    return uiButton;
}

// Routine Description:
// - Enables alternate scroll mode. This sends Cursor Up/down sequences when in the alternate buffer
// Parameters:
// - fEnable - either enable or disable.
// Return value:
// <none>
void MouseInput::EnableAlternateScroll(const bool fEnable)
{
    _fAlternateScroll = fEnable;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void MouseInput::UseAlternateScreenBuffer()
{
    _fInAlternateBuffer = true;
}

// Routine Description:
// - Notify the MouseInput handler that the screen buffer has been swapped to the alternate buffer
// Parameters:
// <none>
// Return value:
// <none>
void MouseInput::UseMainScreenBuffer()
{
    _fInAlternateBuffer = false;
}

// Routine Description:
// - Returns true if we should translate the input event (uiButton, sScrollDelta)
//      into an alternate scroll event instead of the default scroll event,
//      dependiong on if alternate scroll mode is enabled and we're in the alternate buffer.
// Parameters:
// - uiButton: The mouse event code of the input event
// - sScrollDelta: The scroll wheel delta of the input event
// Return value:
// True iff the alternate buffer is active and alternate scroll mode is enabled and the event is a mouse wheel event.
bool MouseInput::_ShouldSendAlternateScroll(_In_ unsigned int uiButton, _In_ short sScrollDelta) const
{
    return _fInAlternateBuffer &&
           _fAlternateScroll &&
           (uiButton == WM_MOUSEWHEEL || uiButton == WM_MOUSEHWHEEL) && sScrollDelta != 0;
}

// Routine Description:
// - Sends a sequence to the input coresponding to cursor up / down depending on the sScrollDelta.
// Parameters:
// - sScrollDelta: The scroll wheel delta of the input event
// Return value:
// True iff the input sequence was sent successfully.
bool MouseInput::_SendAlternateScroll(_In_ short sScrollDelta) const
{
    const wchar_t* const pwchSequence = sScrollDelta > 0 ? CURSOR_UP_SEQUENCE : CURSOR_DOWN_SEQUENCE;
    _SendInputSequence(pwchSequence, CCH_CURSOR_SEQUENCES);

    return true;
}
