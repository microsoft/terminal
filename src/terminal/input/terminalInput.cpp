// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "terminalInput.hpp"

#include <til/unicode.h>

#include "../types/inc/IInputEvent.hpp"

// Throughout portions of this file will be "KKP>" comments,
// which refer to the kitty keyboard protocol documentation:
//   https://sw.kovidgoyal.net/kitty/keyboard-protocol/
//
// Like other Kitty protocol specifications at the time (2026-01-30),
// it is unfortunately defined in a very informal, narrative specification.
// As such, you'll find that I've heavily "[editorialized]" many citations below.
// It's still a hard read at times.

// A significant portion of the following code doesn't care
// about the difference between left/right modifier keys, so this
// Simple Key State bitfield can be used to simplify some expressions.
//
// NOTE: The values for Shift/Alt/Ctrl intentionally match the KKP spec.
// Don't break this unintentionally.
#define SKS_SHIFT 1
#define SKS_ALT 2
#define SKS_CTRL 4
#define SKS_ENHANCED 8

using namespace std::string_view_literals;
using namespace Microsoft::Console::VirtualTerminal;

TerminalInput::TerminalInput() noexcept
{
    _initKeyboardMap();
}

void TerminalInput::UseMainScreenBuffer() noexcept
{
    if (!_inAlternateBuffer)
    {
        return;
    }

    _inAlternateBuffer = false;
    _kittyAltStack.clear();
    _kittyFlags = _kittyMainStack.empty() ? 0 : _kittyMainStack.back();
}

void TerminalInput::UseAlternateScreenBuffer() noexcept
{
    if (_inAlternateBuffer)
    {
        return;
    }

    _inAlternateBuffer = true;
    _kittyAltStack.clear();
    _kittyFlags = 0;
}

void TerminalInput::SetInputMode(const Mode mode, const bool enabled) noexcept
{
    // If we're changing a tracking mode, we always clear other tracking modes first.
    // We also clear out the last saved mouse position & button.
    if (mode == Mode::DefaultMouseTracking || mode == Mode::ButtonEventMouseTracking || mode == Mode::AnyEventMouseTracking)
    {
        _inputMode.reset(Mode::DefaultMouseTracking, Mode::ButtonEventMouseTracking, Mode::AnyEventMouseTracking);
        _mouseInputState.lastPos = { -1, -1 };
        _mouseInputState.lastButton = 0;
    }

    // But if we're changing the encoding, we only clear out the other encoding modes
    // when enabling a new encoding - not when disabling.
    if ((mode == Mode::Utf8MouseEncoding || mode == Mode::SgrMouseEncoding) && enabled)
    {
        _inputMode.reset(Mode::Utf8MouseEncoding, Mode::SgrMouseEncoding);
    }

    _inputMode.set(mode, enabled);

    // If we've changed one of the modes that alter the VT input sequences,
    // we'll need to regenerate our keyboard map.
    static constexpr auto keyMapModes = til::enumset<Mode>{ Mode::LineFeed, Mode::Ansi, Mode::Keypad, Mode::CursorKey, Mode::BackarrowKey, Mode::SendC1 };
    if (keyMapModes.test(mode))
    {
        _initKeyboardMap();
    }
}

bool TerminalInput::GetInputMode(const Mode mode) const noexcept
{
    return _inputMode.test(mode);
}

void TerminalInput::ResetInputModes() noexcept
{
    _inputMode = { Mode::Ansi, Mode::AutoRepeat, Mode::AlternateScroll };
    _mouseInputState.lastPos = { -1, -1 };
    _mouseInputState.lastButton = 0;
    ResetKittyKeyboardProtocols();
    _initKeyboardMap();
}

void TerminalInput::ForceDisableWin32InputMode(const bool win32InputMode) noexcept
{
    _forceDisableWin32InputMode = win32InputMode;
}

void TerminalInput::ForceDisableKittyKeyboardProtocol(const bool disable) noexcept
{
    _forceDisableKittyKeyboardProtocol = disable;
    if (disable)
    {
        _kittyFlags = 0;
        _kittyMainStack.clear();
        _kittyAltStack.clear();
    }
}

void TerminalInput::SetKittyKeyboardProtocol(uint8_t flags, const KittyKeyboardProtocolMode mode) noexcept
{
    if (_forceDisableKittyKeyboardProtocol)
    {
        return;
    }

    flags &= KittyKeyboardProtocolFlags::All;

    switch (mode)
    {
    case KittyKeyboardProtocolMode::Replace:
        _kittyFlags = flags;
        break;
    case KittyKeyboardProtocolMode::Set:
        _kittyFlags |= flags;
        break;
    case KittyKeyboardProtocolMode::Reset:
        _kittyFlags &= ~flags;
        break;
    }
}

uint8_t TerminalInput::GetKittyFlags() const noexcept
{
    return _kittyFlags;
}

void TerminalInput::PushKittyFlags(const uint8_t flags)
{
    if (_forceDisableKittyKeyboardProtocol)
    {
        return;
    }

    auto& stack = _getKittyStack();
    // KKP> If a push request is received and the stack is full,
    // KKP> the oldest entry from the stack must be evicted.
    if (stack.size() >= KittyStackMaxSize)
    {
        stack.erase(stack.begin());
    }
    stack.push_back(_kittyFlags);
    _kittyFlags = flags & KittyKeyboardProtocolFlags::All;
}

void TerminalInput::PopKittyFlags(size_t count)
{
    // NOTE: It's not just an optimization to return early here.
    if (count == 0)
    {
        return;
    }

    auto& stack = _getKittyStack();

    if (count >= stack.size())
    {
        // KKP> If a pop request is received that empties the stack, all flags are reset.
        _kittyFlags = 0;
        stack.clear();
    }
    else
    {
        _kittyFlags = stack.at(stack.size() - count);
        stack.erase(stack.end() - count, stack.end());
    }
}

void TerminalInput::ResetKittyKeyboardProtocols() noexcept
{
    _kittyFlags = 0;
    _kittyMainStack.clear();
    _kittyAltStack.clear();
}

TerminalInput::OutputType TerminalInput::MakeUnhandled() noexcept
{
    return {};
}

TerminalInput::OutputType TerminalInput::MakeOutput(const std::wstring_view& str)
{
    return { StringType{ str } };
}

// Routine Description:
// - Sends the given input event to the shell.
// - The caller should attempt to fill the char data in pInEvent if possible.
//   The char data should already be translated in accordance to Ctrl/Alt/Shift
//   modifiers, like the characters given by the WM_CHAR event.
// - The caller doesn't need to fill in any char data for:
//   - Tab key
//   - Alt+key combinations
// - This method will alias Ctrl+Space as a synonym for Ctrl+@ - the null byte.
// Arguments:
// - keyEvent - Key event to translate
// Return Value:
// - Returns an empty optional if we didn't handle the key event and the caller can opt to handle it in some other way.
// - Returns a string if we successfully translated it into a VT input sequence.
TerminalInput::OutputType TerminalInput::HandleKey(const INPUT_RECORD& event)
{
    // On key presses, prepare to translate to VT compatible sequences
    if (event.EventType != KEY_EVENT)
    {
        return MakeUnhandled();
    }

    const auto keyEvent = event.Event.KeyEvent;

    // GH#4999 - If we're in win32-input mode, skip straight to doing that.
    // Since this mode handles all types of key events, do nothing else.
    //
    // The kitty keyboard protocol takes precedence, because it's cross-platform.
    if (_inputMode.test(Mode::Win32) && !_forceDisableWin32InputMode && !_kittyFlags)
    {
        return _makeWin32Output(keyEvent);
    }

    const auto controlKeyState = _trackControlKeyState(keyEvent);
    const auto virtualKeyCode = keyEvent.wVirtualKeyCode;

    uint32_t codepoint = keyEvent.uChar.UnicodeChar;
    // Swallow lone leading surrogates...
    if (til::is_leading_surrogate(keyEvent.uChar.UnicodeChar))
    {
        _leadingSurrogate = keyEvent.uChar.UnicodeChar;
        return _makeNoOutput();
    }
    // ...and combine them with trailing surrogates.
    if (_leadingSurrogate != 0 && til::is_trailing_surrogate(keyEvent.uChar.UnicodeChar))
    {
        codepoint = til::combine_surrogates(_leadingSurrogate, keyEvent.uChar.UnicodeChar);
    }
    _leadingSurrogate = 0;

    if (keyEvent.bKeyDown)
    {
        // If this is a VK_PACKET or 0 virtual key, it's likely a synthesized
        // keyboard event, so the UnicodeChar is transmitted as is. This must be
        // handled before the Auto Repeat test, other we'll end up dropping chars.
        if (virtualKeyCode == VK_PACKET || virtualKeyCode == 0)
        {
            return _makeCharOutput(codepoint);
        }

        // If it's a numeric keypad key, and Alt is pressed (but not Ctrl),
        // then this is an Alt-Numpad composition, and we should ignore these keys.
        // The generated character will be transmitted when the Alt is released.
        if (virtualKeyCode >= VK_NUMPAD0 && virtualKeyCode <= VK_NUMPAD9 &&
            (controlKeyState == LEFT_ALT_PRESSED || controlKeyState == RIGHT_ALT_PRESSED))
        {
            return _makeNoOutput();
        }
    }

    // Keep track of key repeats.
    const auto isKeyRepeat = _lastVirtualKeyCode == virtualKeyCode;
    if (keyEvent.bKeyDown)
    {
        _lastVirtualKeyCode = virtualKeyCode;
    }
    else if (isKeyRepeat)
    {
        _lastVirtualKeyCode = std::nullopt;
    }

    // If this is a repeat of the last recorded key press, and Auto Repeat Mode
    // is disabled, then we should suppress this event.
    if (isKeyRepeat && !_inputMode.test(Mode::AutoRepeat))
    {
        return _makeNoOutput();
    }

    // There's a bunch of early returns we can place on key-up events,
    // before we run our more complex encoding logic below.
    if (!keyEvent.bKeyDown)
    {
        // If NumLock is on, and this is an Alt release with a unicode char,
        // it must be the generated character from an Alt-Numpad composition.
        if (WI_IsFlagSet(controlKeyState, NUMLOCK_ON) && virtualKeyCode == VK_MENU && codepoint != 0)
        {
            return _makeCharOutput(codepoint);
        }

        // KKP> Normally only key press events are reported [...].
        //
        // ...or put differently: If ReportEventTypes is disabled,
        // and this is a key-up, we can return early.
        if (WI_IsFlagClear(_kittyFlags, KittyKeyboardProtocolFlags::ReportEventTypes))
        {
            return _makeNoOutput();
        }

        // From a side note (?!) on the "Report event types" section:
        //
        // KKP> NOTE: The Enter, Tab and Backspace keys will not have release
        // KKP> events unless Report all keys as escape codes is also set [...].
        if (WI_IsFlagClear(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes) &&
            (virtualKeyCode == VK_RETURN || virtualKeyCode == VK_TAB || virtualKeyCode == VK_BACK))
        {
            return _makeNoOutput();
        }
    }

    // Keyboards that have an AltGr key will generate both a RightAlt key press
    // and a fake LeftCtrl key press. In order to support key combinations where
    // the Ctrl key is manually pressed in addition to the AltGr key, we have to
    // be able to detect when the Ctrl key isn't genuine. We do so by tracking
    // the time between the Alt and Ctrl key presses, and only consider the Ctrl
    // key to really be pressed if the difference is more than 50ms.
    auto leftCtrlIsReallyPressed = WI_IsFlagSet(controlKeyState, LEFT_CTRL_PRESSED);
    if (WI_AreAllFlagsSet(controlKeyState, LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED))
    {
        const auto max = std::max(_lastLeftCtrlTime, _lastRightAltTime);
        const auto min = std::min(_lastLeftCtrlTime, _lastRightAltTime);
        leftCtrlIsReallyPressed = (max - min) > 50;
    }

    const auto ctrlIsPressed = WI_IsAnyFlagSet(controlKeyState, CTRL_PRESSED);
    const auto ctrlIsReallyPressed = leftCtrlIsReallyPressed || WI_IsFlagSet(controlKeyState, RIGHT_CTRL_PRESSED);
    const auto shiftIsPressed = WI_IsFlagSet(controlKeyState, SHIFT_PRESSED);
    const auto altIsPressed = WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED);
    const auto altGrIsPressed = altIsPressed && ctrlIsPressed;
    const auto enhanced = WI_IsFlagSet(controlKeyState, ENHANCED_KEY);

    DWORD simpleKeyState = 0;
    WI_SetFlagIf(simpleKeyState, SKS_CTRL, ctrlIsReallyPressed);
    WI_SetFlagIf(simpleKeyState, SKS_ALT, altIsPressed);
    WI_SetFlagIf(simpleKeyState, SKS_SHIFT, shiftIsPressed);
    WI_SetFlagIf(simpleKeyState, SKS_ENHANCED, enhanced);

    const auto info = _getKeyEncodingInfo(keyEvent, simpleKeyState);

    // > CSI unicode-key-code:alternate-key-codes ; modifiers:event-type ; text-as-codepoints u
    //
    // unicode-key-code:
    // > [..] the [lowercase] Unicode codepoint representing the key, as a decimal number.
    //
    // alternate-key-codes:
    // > [...] the terminal can send [...] the shifted key and base layout key, separated by colons.
    //   shifted key:
    //   > [...] the shifted key and base layout key, separated by colons.
    //   > The shifted key is [...] [the result of Shift + unicode-key-code],
    //   > in the currently active keyboard layout.
    //   base layout key:
    //   > [...] the base layout key is the key corresponding to
    //   > the physical key in the standard PC-101 key layout.
    //
    // modifiers:
    // > Modifiers are encoded as a bit field with:
    // > [...]
    // For bit encoding see above.
    // > [...] the modifier value is encoded as [...] 1 + actual modifiers.
    //
    // event-type:
    // > There are three key event types: press, repeat and release.
    // To summarize unnecessary prose: press=1, repeat=2, release=3.
    // > If [event-types are requested and] no modifiers are present,
    // > the modifiers field must have the value 1 [...].
    //
    // text-as-codepoints:
    // > [...] the text associated with key events as a sequence of Unicode code points.
    // > If multiple code points are present, they must be separated by colons.
    // > If no known key is associated with the text the key number 0 must be used.
    // > The associated text must not contain control codes [...].
    int32_t kittyKeyCode = 0;
    int32_t kittyAltKeyCodeShifted = 0;
    int32_t kittyAltKeyCodeBase = 0;
    int32_t kittyTextAsCodepoint = 0;

    // Get the codepoint that would be generated without modifiers.
    //
    // _getKeyEncodingInfo() only returns anything for non-text keys.
    // So, using the kittyKeyCode == 0 check we filter down to (possible) text keys.
    // The logic is also not required if no modifiers are pressed,
    // since no modifiers means that we can just use the codepoint as is.
    int32_t codepointWithoutModifiers = codepoint;
    if (info.kittyKeyCode == 0)
    {
        // We need the current keyboard layout and state to look up the character
        // that would be transmitted in that state (via the ToUnicodeEx API).
        const auto hkl = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
        auto keyState = _getKeyboardState(virtualKeyCode, controlKeyState);
        constexpr UINT flags = 4; // Don't modify the state in the ToUnicodeEx call.
        constexpr int bufferSize = 4;
        wchar_t buffer[bufferSize];

        // However, we first need to query the key with the original state, to check
        // whether it's a dead key. If that is the case, ToUnicodeEx should return a
        // negative number, although in practice it's more likely to return a string
        // of length two, with two identical characters. This is because the system
        // sees this as a second press of the dead key, which would typically result
        // in the combining character representation being transmitted twice.
        auto length = ToUnicodeEx(virtualKeyCode, 0, keyState.data(), buffer, bufferSize, flags, hkl);
        if (length < 0 || (length == 2 && buffer[0] == buffer[1]))
        {
            return _makeNoOutput();
        }

        // Once we know it's not a dead key, we run the query again, but with the
        // Ctrl and Alt modifiers disabled to obtain the base character mapping.
        keyState.at(VK_CONTROL) = keyState.at(VK_LCONTROL) = keyState.at(VK_RCONTROL) = 0;
        keyState.at(VK_MENU) = keyState.at(VK_LMENU) = keyState.at(VK_RMENU) = 0;
        length = ToUnicodeEx(virtualKeyCode, 0, keyState.data(), buffer, bufferSize, flags, hkl);
        if (length == 1 || length == 2)
        {
            codepointWithoutModifiers = _bufferToCodepoint(buffer);
        }
    }

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::DisambiguateEscapeCodes))
    {
        // KKP> Turning on [DisambiguateEscapeCodes] will cause the terminal to
        // KKP> report the Esc, alt+key, ctrl+key, ctrl+alt+key, shift+alt+key
        // KKP> keys using CSI u sequences instead of legacy ones.
        // KKP> Here key is any ASCII key as described in Legacy text keys. [...]
        //
        // NOTE: The specification fails to mention ctrl+shift+key, but Kitty does handle it.
        // So really, it's actually "any modifiers, except for shift+key".

        // KKP> Legacy text keys:
        // KKP> For legacy compatibility, the keys a-z 0-9 ` - = [ ] \ ; ' , . /    [...]
        //
        // NOTE: The list of legacy keys doesn't really make any sense.
        // My interpretation is that the author meant all printable keys,
        // because as before, that's also how Kitty handles it.

        // KKP> Additionally, all non text keypad keys will be reported [...] with CSI u encoding, [...].

        if (virtualKeyCode == VK_ESCAPE ||
            (virtualKeyCode >= VK_NUMPAD0 && virtualKeyCode <= VK_DIVIDE) ||
            ((simpleKeyState & ~SKS_ENHANCED) > SKS_SHIFT && _codepointIsText(codepointWithoutModifiers)))
        {
            kittyKeyCode = info.kittyKeyCode;
        }
    }

    int32_t kittyEventType = 1;
    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportEventTypes))
    {
        // KKP> This [...] causes the terminal to report key repeat and key release events.
        if (!keyEvent.bKeyDown)
        {
            kittyEventType = 3; // release event
        }
        else if (isKeyRepeat)
        {
            kittyEventType = 2; // repeat event
        }
    }

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAlternateKeys))
    {
        // KKP> This [...] causes the terminal to report alternate key values [...]
        // KKP> See Key codes for details [...]
        //
        // KKP> Key codes:
        // KKP> The unicode-key-code above is the Unicode codepoint representing the key [...]
        // KKP> Note that the codepoint used is always the lower-case [...] version of the key.

        // KKP> Note that [...] only key events represented as escape codes due to the
        // KKP> other enhancements in effect will be affected by this enhancement. [...]

        // KKP> [...] the terminal can [...] [additionally send] the shifted key and base layout key [...].
        // KKP> The shifted key is [...] the upper-case version of unicode-codepoint,
        // KKP> or more technically, the shifted version, in the currently active keyboard layout.
        //
        // !!NOTE!! that the 2nd line is wrong. On a US layout, Shift+3 is not an "uppercase 3",
        // it's "#", which is exactly what kitty sends as the "shifted key" parameter.
        // (This is in addition to "unicode-codepoint" never being defined throughout the spec...)

        // KKP> Note that the shifted key must be present only if shift is also present in the modifiers.

        if ((simpleKeyState & SKS_SHIFT) != 0 && _codepointIsText(codepoint))
        {
            // I'm assuming that codepoint is already the shifted version if shift is pressed.
            kittyAltKeyCodeShifted = codepoint;
        }

        kittyAltKeyCodeBase = _getBaseLayoutCodepoint(virtualKeyCode);
    }

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes))
    {
        // KKP> This [...] turns on key reporting even for key events that generate text.
        // KKP> [...] text will not be sent, instead only key events are sent.
        //
        // KKP> [...] with this mode, events for pressing modifier keys are reported.
        kittyKeyCode = info.kittyKeyCode;
        if (kittyKeyCode == 0 && _codepointIsText(codepointWithoutModifiers))
        {
            kittyKeyCode = codepointWithoutModifiers;
        }

        if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAssociatedText))
        {
            // KKP> This [...] causes key events that generate text to be reported
            // KKP> as CSI u escape codes with the text embedded in the escape code.
            //
            // KKP> Note that this flag is an enhancement to Report all keys as escape codes [...].
            //
            // KKP> The associated text must not contain control codes [...].
            if (_codepointIsText(codepoint))
            {
                kittyTextAsCodepoint = codepoint;
            }
        }
    }

    // These modifiers apply to all CSI encodings, including KKP.
    // As per KKP: shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
    int32_t csiModifiers = simpleKeyState & (SKS_SHIFT | SKS_ALT | SKS_CTRL);
    // KKP> Lock modifiers are not reported for text producing keys, [...].
    // KKP> To get lock modifiers for all keys use the Report all keys as escape codes enhancement.
    if (!_codepointIsText(codepoint) || WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes))
    {
        if (WI_IsFlagSet(controlKeyState, CAPSLOCK_ON))
        {
            csiModifiers |= 64;
        }
        if (WI_IsFlagSet(controlKeyState, NUMLOCK_ON))
        {
            csiModifiers |= 128;
        }
    }

    // > Terminals may choose what they want to do about functional keys that have no legacy encoding.
    //
    // Specification doesn't specify, so I'm doing it whenever any mode is set.

    std::wstring seq;

    if (kittyKeyCode)
    {
        seq.append(_csi);
        fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), kittyKeyCode);

        if (kittyAltKeyCodeShifted != 0 || kittyAltKeyCodeBase != 0)
        {
            seq.push_back(L':');
            if (kittyAltKeyCodeShifted != 0)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), kittyAltKeyCodeShifted);
            }
            if (kittyAltKeyCodeBase != 0)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L":{}"), kittyAltKeyCodeBase);
            }
        }
    }
    else if (info.csiParam1)
    {
        seq.append(_csi);
        fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), info.csiParam1);
    }

    if (csiModifiers != 0 || kittyKeyCode && (kittyEventType != 0 || kittyTextAsCodepoint != 0))
    {
        fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L";{}"), 1 + csiModifiers);
        if (kittyKeyCode)
        {
            if (kittyEventType != 0)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L":{}"), kittyEventType);
            }
            if (kittyTextAsCodepoint != 0)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L";{}"), kittyTextAsCodepoint);
            }
        }
    }

    if (kittyKeyCode)
    {
        seq.push_back(L'u');
    }
    else if (info.csiFinal)
    {
        seq.push_back(info.csiFinal);
    }
    else if (info.ss3Final)
    {
    }
    else if (!info.plain.empty())
    {
        if (info.altPrefix && (simpleKeyState & SKS_ALT))
        {
            seq.push_back(L'\x1b');
        }
        seq.append(info.plain);
    }

    /*
    // If it's not in the key map, we'll use the UnicodeChar, if provided,
    // except in the case of Ctrl+Space, which is often mapped incorrectly as
    // a space character when it's expected to be mapped to NUL. We need to
    // let that fall through to the standard mapping algorithm below.
    const auto ctrlSpaceKey = ctrlIsReallyPressed && virtualKeyCode == VK_SPACE;
    if (codepoint != 0 && !ctrlSpaceKey)
    {
        // In the case of an AltGr key, we may still need to apply a Ctrl
        // modifier to the char, either because both Ctrl keys were pressed,
        // or we got a LeftCtrl that was distinctly separate from the RightAlt.
        const auto bothCtrlsArePressed = WI_AreAllFlagsSet(controlKeyState, CTRL_PRESSED);
        const auto rightAltIsPressed = WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED);
        auto wch = codepoint;

        if (altGrIsPressed && (bothCtrlsArePressed || (rightAltIsPressed && leftCtrlIsReallyPressed)))
        {
            wch = _makeCtrlChar(codepoint);
        }

        auto charSequence = _makeCharOutput(wch);
        // We may also need to apply an Alt prefix to the char sequence, but
        // if this is an AltGr key, we only do so if both Alts are pressed.
        const auto bothAltsArePressed = WI_AreAllFlagsSet(controlKeyState, ALT_PRESSED);
        _escapeOutput(charSequence, altGrIsPressed ? bothAltsArePressed : altIsPressed);
        return charSequence;
    }

    // If we don't have a UnicodeChar, we'll try and determine what the key
    // would have transmitted without any Ctrl or Alt modifiers applied. But
    // this only makes sense if there were actually modifiers pressed.
    if (!altIsPressed && !ctrlIsPressed)
    {
        return _makeNoOutput();
    }

    // Once we've got the base character, we can apply the Ctrl modifier.
    if (ctrlIsReallyPressed)
    {
        auto ch = _makeCtrlChar(codepoint);
        // If we haven't found a Ctrl mapping for the key, and it's one of
        // the alphanumeric keys, we try again using the virtual key code.
        // On keyboard layouts where the alphanumeric keys are not mapped to
        // their typical ASCII values, this provides a simple fallback.
        if (ch >= L' ' && virtualKeyCode >= '2' && virtualKeyCode <= 'Z')
        {
            ch = _makeCtrlChar(virtualKeyCode);
        }
        codepoint = ch;
    }
    // If Alt is pressed, that also needs to be applied to the sequence.
    _escapeOutput(charSequence, altIsPressed);
    */

    return seq;
}

TerminalInput::OutputType TerminalInput::HandleFocus(const bool focused) const
{
    if (!_inputMode.test(Mode::FocusEvent))
    {
        return MakeUnhandled();
    }

    return MakeOutput(focused ? _focusInSequence : _focusOutSequence);
}

void TerminalInput::_initKeyboardMap() noexcept
try
{
    _focusInSequence = fmt::format(FMT_COMPILE(L"{}I", _csi));
    _focusOutSequence = fmt::format(FMT_COMPILE(L"{}O", _csi));
}
CATCH_LOG()

DWORD TerminalInput::_trackControlKeyState(const KEY_EVENT_RECORD& key) noexcept
{
    // First record which key state bits were previously off but are now on.
    const auto pressedKeyState = ~_lastControlKeyState & key.dwControlKeyState;
    // Then save the new key state so we can determine future state changes.
    _lastControlKeyState = key.dwControlKeyState;
    // But if this latest change has set the RightAlt bit, without having
    // received a RightAlt key press, then we need to clear that bit. This
    // can happen when pressing the AltGr key on the On-Screen keyboard. It
    // actually generates LeftCtrl and LeftAlt key presses, but also sets
    // the RightAlt bit on the final key state. If we don't clear that, it
    // can be misinterpreted as an Alt+AltGr key combination.
    const auto rightAltDown = key.bKeyDown && key.wVirtualKeyCode == VK_MENU && WI_IsFlagSet(key.dwControlKeyState, ENHANCED_KEY);
    WI_ClearFlagIf(_lastControlKeyState, RIGHT_ALT_PRESSED, WI_IsFlagSet(pressedKeyState, RIGHT_ALT_PRESSED) && !rightAltDown);
    // We also take this opportunity to record the time at which the LeftCtrl
    // and RightAlt keys are pressed. This is needed to determine whether the
    // Ctrl key was pressed by the user, or fabricated by an AltGr key press.
    if (key.bKeyDown)
    {
        if (WI_IsFlagSet(pressedKeyState, LEFT_CTRL_PRESSED))
        {
            _lastLeftCtrlTime = GetTickCount64();
        }
        if (WI_IsFlagSet(pressedKeyState, RIGHT_ALT_PRESSED))
        {
            _lastRightAltTime = GetTickCount64();
        }
    }
    return _lastControlKeyState;
}

// Returns a simplified representation of the keyboard state, based on the most
// recent key press and associated control key state (which is all we need for
// our ToUnicodeEx queries). This is a substitute for the GetKeyboardState API,
// which can't be used when serving as a conpty host.
std::array<byte, 256> TerminalInput::_getKeyboardState(const WORD virtualKeyCode, const DWORD controlKeyState)
{
    auto keyState = std::array<byte, 256>{};
    if (virtualKeyCode < keyState.size())
    {
        keyState.at(virtualKeyCode) = 0x80;
    }
    keyState.at(VK_LCONTROL) = WI_IsFlagSet(controlKeyState, LEFT_CTRL_PRESSED) ? 0x80 : 0;
    keyState.at(VK_RCONTROL) = WI_IsFlagSet(controlKeyState, RIGHT_CTRL_PRESSED) ? 0x80 : 0;
    keyState.at(VK_CONTROL) = keyState.at(VK_LCONTROL) | keyState.at(VK_RCONTROL);
    keyState.at(VK_LMENU) = WI_IsFlagSet(controlKeyState, LEFT_ALT_PRESSED) ? 0x80 : 0;
    keyState.at(VK_RMENU) = WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED) ? 0x80 : 0;
    keyState.at(VK_MENU) = keyState.at(VK_LMENU) | keyState.at(VK_RMENU);
    keyState.at(VK_SHIFT) = keyState.at(VK_LSHIFT) = WI_IsFlagSet(controlKeyState, SHIFT_PRESSED) ? 0x80 : 0;
    keyState.at(VK_CAPITAL) = WI_IsFlagSet(controlKeyState, CAPSLOCK_ON);
    return keyState;
}

wchar_t TerminalInput::_makeCtrlChar(const wchar_t ch)
{
    if (ch >= L'@' && ch <= L'~')
    {
        return ch & 0b11111;
    }
    if (ch == L' ')
    {
        return 0x00;
    }
    if (ch == L'/')
    {
        return 0x1F;
    }
    if (ch == L'?')
    {
        return 0x7F;
    }
    if (ch >= L'2' && ch <= L'8')
    {
        constexpr auto numericCtrls = std::array<wchar_t, 7>{ 0, 27, 28, 29, 30, 31, 127 };
        return numericCtrls.at(ch - L'2');
    }
    return ch;
}

// Turns the given character into StringType.
// If it encounters a surrogate pair, it'll buffer the leading character until a
// trailing one has been received and then flush both of them simultaneously.
// Surrogate pairs should always be handled as proper pairs after all.
TerminalInput::StringType TerminalInput::_makeCharOutput(const uint32_t cp)
{
    const auto buf = _codepointToBuffer(cp);
    return { buf.buf, buf.len };
}

TerminalInput::StringType TerminalInput::_makeNoOutput() noexcept
{
    return {};
}

// Sends the given char as a sequence representing Alt+char, also the same as Meta+char.
void TerminalInput::_escapeOutput(StringType& charSequence, const bool altIsPressed) const
{
    // Alt+char combinations are only applicable in ANSI mode.
    if (altIsPressed && _inputMode.test(Mode::Ansi))
    {
        charSequence.insert(0, 1, L'\x1b');
    }
}

// Turns an KEY_EVENT_RECORD into a win32-input-mode VT sequence.
// It allows us to send KEY_EVENT_RECORD data losslessly to conhost.
TerminalInput::OutputType TerminalInput::_makeWin32Output(const KEY_EVENT_RECORD& key) const
{
    // .uChar.UnicodeChar must be cast to an integer because we want its numerical value.
    // Casting the rest to uint16_t as well doesn't hurt because that's MAX_PARAMETER_VALUE anyways.
    const auto kd = gsl::narrow_cast<uint16_t>(key.bKeyDown ? 1 : 0);
    const auto rc = gsl::narrow_cast<uint16_t>(key.wRepeatCount);
    const auto vk = gsl::narrow_cast<uint16_t>(key.wVirtualKeyCode);
    const auto sc = gsl::narrow_cast<uint16_t>(key.wVirtualScanCode);
    const auto uc = gsl::narrow_cast<uint16_t>(key.uChar.UnicodeChar);
    const auto cs = gsl::narrow_cast<uint16_t>(key.dwControlKeyState);

    // Sequences are formatted as follows:
    //
    // CSI Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    //
    //      Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.
    //      Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.
    //      Uc: the decimal value of UnicodeChar - for example, NUL is "0", LF is
    //          "10", the character 'A' is "65". If omitted, defaults to '0'.
    //      Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.
    //      Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.
    //      Rc: the value of wRepeatCount - any number. If omitted, defaults to '1'.
    return fmt::format(FMT_COMPILE(L"{}{};{};{};{};{};{}_"), _csi, vk, sc, uc, kd, cs, rc);
}

TerminalInput::KeyEncodingInfo TerminalInput::_getKeyEncodingInfo(const KEY_EVENT_RECORD& key, DWORD simpleKeyState) const noexcept
{
    const auto virtualKeyCode = key.wVirtualKeyCode;
    const auto modified = (simpleKeyState & ~SKS_ENHANCED) != 0;
    const auto enhanced = (simpleKeyState & SKS_ENHANCED) != 0;
    KeyEncodingInfo info;

    switch (virtualKeyCode)
    {
    // BACKSPACE maps to either DEL or BS, depending on the Backarrow Key mode.
    // The Ctrl modifier inverts the active mode, swapping BS and DEL (this is
    // not standard, but a modern terminal convention). The Alt modifier adds
    // an ESC prefix (also not standard).
    case VK_BACK:
        info.kittyKeyCode = 127; // BACKSPACE
        info.altPrefix = true;
        switch (simpleKeyState & ~(SKS_ALT | SKS_SHIFT))
        {
        default:
            info.plain = _inputMode.test(Mode::BackarrowKey) ? L"\b"sv : L"\x7F"sv;
            break;
        case SKS_CTRL:
            info.plain = _inputMode.test(Mode::BackarrowKey) ? L"\x7f"sv : L"\b"sv;
            break;
        }
        break;
    // TAB maps to HT, and Shift+TAB to CBT. The Ctrl modifier has no effect.
    // The Alt modifier adds an ESC prefix, although in practice all the Alt
    // mappings are likely to be system hotkeys.
    case VK_TAB:
        info.kittyKeyCode = 9; // TAB
        info.altPrefix = true;
        switch (simpleKeyState & ~(SKS_ALT | SKS_CTRL))
        {
        default:
            info.plain = L"\t"sv;
            break;
        case SKS_SHIFT:
            info.csiFinal = L'Z';
            break;
        }
        break;
    // RETURN maps to either CR or CR LF, depending on the Line Feed mode. With
    // a Ctrl modifier it maps to LF, because that's the expected behavior for
    // most PC keyboard layouts. The Alt modifier adds an ESC prefix.
    case VK_RETURN:
        info.kittyKeyCode = enhanced ? 57414 : 13; // KP_RETURN : ENTER
        // The keypad RETURN key works different.
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad) && enhanced)
        {
            if (_inputMode.test(Mode::Ansi))
            {
                info.ss3Final = L'M';
            }
            else
            {
                info.plain = L"\033?M"sv;
            }
        }
        else
        {
            info.altPrefix = true;
            switch (simpleKeyState & ~(SKS_ALT | SKS_SHIFT | SKS_ENHANCED))
            {
            default:
                info.plain = _inputMode.test(Mode::LineFeed) ? L"\r\n"sv : L"\r"sv;
                break;
            case SKS_CTRL:
                info.plain = L"\n"sv;
                break;
            }
        }
        break;
    case VK_ESCAPE:
        info.kittyKeyCode = 27; // ESCAPE

    case VK_CAPITAL:
        info.kittyKeyCode = 57358; // CAPS_LOCK
        break;
    case VK_SCROLL:
        info.kittyKeyCode = 57359; // SCROLL_LOCK
        break;
    case VK_NUMLOCK:
        info.kittyKeyCode = 57360; // NUM_LOCK
        break;
    case VK_SNAPSHOT:
        info.kittyKeyCode = 57361; // PRINT_SCREEN
        break;
    // PAUSE doesn't have a VT mapping, but traditionally we've mapped it to ^Z,
    // regardless of modifiers.
    case VK_PAUSE:
        info.kittyKeyCode = 57362; // PAUSE
        info.plain = L"\x1A"sv;
        break;
    case VK_APPS:
        info.kittyKeyCode = 57363; // MENU
        break;

    // F1 to F4 map to the VT keypad function keys, which are SS3 sequences.
    // When combined with a modifier, we use CSI sequences with the modifier
    // embedded as a parameter (not standard - a modern terminal extension).
    // VT52 only supports PF1 through PF4 function keys with ESC prefix.
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
        if (_inputMode.test(Mode::Ansi))
        {
            // KKP> The original version of this specification allowed F3 to
            // KKP> be encoded as both CSI R and CSI ~ [and now it doesn't].
            if (virtualKeyCode == VK_F3 &&
                WI_IsAnyFlagSet(
                    _kittyFlags,
                    KittyKeyboardProtocolFlags::DisambiguateEscapeCodes |
                        KittyKeyboardProtocolFlags::ReportEventTypes |
                        KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes))
            {
                info.csiFinal = L'~';
                info.csiParam1 = 13;
            }
            else
            {
                auto& dst = modified ? info.csiFinal : info.ss3Final;
                dst = L'P' + (virtualKeyCode - VK_F1);
            }
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033P", L"\033Q", L"\033R", L"\033S" };
            info.plain = lut[virtualKeyCode - VK_F1];
        }
        break;

    // F5 through F20 map to the top row VT function keys. They use standard
    // DECFNK sequences with the modifier embedded as a parameter. The first
    // five function keys on a VT terminal are typically local functions, so
    // there's not much need to support mappings for them.
    // In VT52 mode, only F11-F13 are defined (as control keys).
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
    case VK_F13:
    case VK_F14:
    case VK_F15:
    case VK_F16:
    case VK_F17:
    case VK_F18:
    case VK_F19:
    case VK_F20:
        if (virtualKeyCode >= VK_F13)
        {
            info.kittyKeyCode = 57376 + (virtualKeyCode - VK_F13);
        }
        if (_inputMode.test(Mode::Ansi))
        {
            static constexpr uint8_t lut[] = { 15, 17, 18, 19, 20, 21, 23, 24, 25, 26, 28, 29, 31, 32, 33, 34 };
            info.csiFinal = L'~';
            info.csiParam1 = lut[virtualKeyCode - VK_F5];
        }
        else
        {
            switch (virtualKeyCode)
            {
            case VK_F11:
                info.plain = L"\033"sv;
                break;
            case VK_F12:
                info.plain = L"\b"sv;
                break;
            case VK_F13:
                info.plain = L"\n"sv;
                break;
            default:
                break;
            }
        }
        break;
    case VK_F21:
    case VK_F22:
    case VK_F23:
    case VK_F24:
        info.kittyKeyCode = 57376 + (virtualKeyCode - VK_F13);
        break;

    // Cursor keys follow a similar pattern to the VT keypad function keys,
    // although they only use an SS3 prefix when the Cursor Key mode is set.
    // When combined with a modifier, they'll use CSI sequences with the
    // modifier embedded as a parameter (again not standard).
    // In VT52 mode, cursor keys use ESC prefix.
    case VK_LEFT: // 0x25 = D
    case VK_UP: // 0x26 = A
    case VK_RIGHT: // 0x27 = C
    case VK_DOWN: // 0x28 = B
    {
        static constexpr uint8_t lut[] = { 3, 0, 2, 1 };
        const auto idx = lut[virtualKeyCode - VK_LEFT];
        if (!enhanced)
        {
            // If enhanced is not set, they should indicate keypad arrow keys.
            info.kittyKeyCode = 57417 + idx; // KP_LEFT, KP_RIGHT, KP_UP, KP_DOWN
        }
        if (_inputMode.test(Mode::Ansi))
        {
            auto& dst = modified ? info.csiFinal : info.ss3Final;
            dst = L'A' + idx;
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033D", L"\033A", L"\033C", L"\033B" };
            info.plain = lut[virtualKeyCode - VK_LEFT];
        }
        break;
    }
    case VK_CLEAR:
        info.kittyKeyCode = 57427; // KP_BEGIN
        if (_inputMode.test(Mode::Ansi))
        {
            auto& dst = modified ? info.csiFinal : info.ss3Final;
            dst = L'E';
        }
        else
        {
            info.plain = L"\033E"sv;
        }
        break;
    case VK_HOME:
        if (!enhanced)
        {
            info.kittyKeyCode = 57423; // KP_HOME
        }
        if (_inputMode.test(Mode::Ansi))
        {
            auto& dst = modified ? info.csiFinal : info.ss3Final;
            dst = L'H';
        }
        else
        {
            info.plain = L"\033H"sv;
        }
        break;
    case VK_END:
        if (!enhanced)
        {
            info.kittyKeyCode = 57424; // KP_END
        }
        if (_inputMode.test(Mode::Ansi))
        {
            auto& dst = modified ? info.csiFinal : info.ss3Final;
            dst = L'F';
        }
        else
        {
            info.plain = L"\033F"sv;
        }
        break;

    // Editing keys follow the same pattern as the top row VT function
    // keys, using standard DECFNK sequences with the modifier embedded.
    // These are not defined in VT52 mode.
    case VK_INSERT: // 0x2D = 2
    case VK_DELETE: // 0x2E = 3
        if (!enhanced)
        {
            info.kittyKeyCode = 57425 + (virtualKeyCode - VK_INSERT); // KP_INSERT, KP_DELETE
        }
        if (_inputMode.test(Mode::Ansi))
        {
            info.csiFinal = L'~';
            info.csiParam1 = 2 + (virtualKeyCode - VK_INSERT);
        }
        break;
    case VK_PRIOR: // 0x21 = 5
    case VK_NEXT: // 0x22 = 6
        if (!enhanced)
        {
            info.kittyKeyCode = 57421 + (virtualKeyCode - VK_PRIOR); // KP_PAGE_UP, KP_PAGE_DOWN
        }
        if (_inputMode.test(Mode::Ansi))
        {
            info.csiFinal = L'~';
            info.csiParam1 = 5 + (virtualKeyCode - VK_PRIOR);
        }
        break;

    // Keypad keys depend on the Keypad mode. When reset, they transmit
    // the ASCII character assigned by the keyboard layout, but when set
    // they transmit SS3 escape sequences. When used with a modifier, the
    // modifier is embedded as a parameter value (not standard).
    // In VT52 mode, keypad keys use ESC ? prefix instead of SS3.
    case VK_NUMPAD0: // 0x60 = p
    case VK_NUMPAD1: // 0x61 = q
    case VK_NUMPAD2: // 0x62 = r
    case VK_NUMPAD3: // 0x63 = s
    case VK_NUMPAD4: // 0x64 = t
    case VK_NUMPAD5: // 0x65 = u
    case VK_NUMPAD6: // 0x66 = v
    case VK_NUMPAD7: // 0x67 = w
    case VK_NUMPAD8: // 0x68 = x
    case VK_NUMPAD9: // 0x69 = y
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            if (_inputMode.test(Mode::Ansi))
            {
                info.ss3Final = L'p' + (virtualKeyCode - VK_NUMPAD0);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?p", L"\033?q", L"\033?r", L"\033?s", L"\033?t", L"\033?u", L"\033?v", L"\033?w", L"\033?x", L"\033?y" };
                info.plain = lut[virtualKeyCode - VK_NUMPAD0];
            }
        }
        break;
    case VK_MULTIPLY: // 0x6A = j
    case VK_ADD: // 0x6B = k
    case VK_SEPARATOR: // 0x6C = l
    case VK_SUBTRACT: // 0x6D = m
    case VK_DECIMAL: // 0x6E = n
    case VK_DIVIDE: // 0x6F = o
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            if (_inputMode.test(Mode::Ansi))
            {
                info.ss3Final = L'j' + (virtualKeyCode - VK_MULTIPLY);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?j", L"\033?k", L"\033?l", L"\033?m", L"\033?n", L"\033?o" };
                info.plain = lut[virtualKeyCode - VK_MULTIPLY];
            }
        }
        break;

    case VK_MEDIA_PLAY_PAUSE:
        info.kittyKeyCode = 57430; // MEDIA_PLAY_PAUSE
        break;
    case VK_MEDIA_STOP:
        info.kittyKeyCode = 57432; // MEDIA_STOP
        break;
    case VK_MEDIA_NEXT_TRACK:
        info.kittyKeyCode = 57435; // MEDIA_TRACK_NEXT
        break;
    case VK_MEDIA_PREV_TRACK:
        info.kittyKeyCode = 57436; // MEDIA_TRACK_PREVIOUS
        break;
    case VK_VOLUME_DOWN:
        info.kittyKeyCode = 57438; // LOWER_VOLUME
        break;
    case VK_VOLUME_UP:
        info.kittyKeyCode = 57439; // RAISE_VOLUME
        break;
    case VK_VOLUME_MUTE:
        info.kittyKeyCode = 57440; // MUTE_VOLUME
        break;

    // Modifier keys
    case VK_SHIFT:
        info.kittyKeyCode = key.wVirtualScanCode == 0x2A ? 57441 : 57447; // LEFT_SHIFT : RIGHT_SHIFT
        break;
    case VK_LSHIFT:
        info.kittyKeyCode = 57441; // LEFT_SHIFT
        break;
    case VK_RSHIFT:
        info.kittyKeyCode = 57447; // RIGHT_SHIFT
        break;
    case VK_CONTROL:
        info.kittyKeyCode = enhanced ? 57448 : 57442; // RIGHT_CONTROL : LEFT_CONTROL
        break;
    case VK_LCONTROL:
        info.kittyKeyCode = 57442; // LEFT_CONTROL
        break;
    case VK_RCONTROL:
        info.kittyKeyCode = 57448; // RIGHT_CONTROL
        break;
    case VK_MENU:
        info.kittyKeyCode = enhanced ? 57449 : 57443; // RIGHT_ALT : LEFT_ALT
        break;
    case VK_LMENU:
        info.kittyKeyCode = 57443; // LEFT_ALT
        break;
    case VK_RMENU:
        info.kittyKeyCode = 57449; // RIGHT_ALT
        break;
    case VK_LWIN:
        info.kittyKeyCode = 57444; // LEFT_SUPER
        break;
    case VK_RWIN:
        info.kittyKeyCode = 57450; // RIGHT_SUPER
        break;

    default:
        break;
    }

    return info;
}

std::vector<uint8_t>& TerminalInput::_getKittyStack() noexcept
{
    return _inAlternateBuffer ? _kittyAltStack : _kittyMainStack;
}

bool TerminalInput::_codepointIsText(uint32_t cp) noexcept
{
    return cp > 0x1f && (cp < 0x7f || cp > 0x9f) && (cp < 57344 || cp > 63743);
}

TerminalInput::CodepointBuffer TerminalInput::_codepointToBuffer(uint32_t cp) noexcept
{
    CodepointBuffer cb;
    if (cp <= 0xFFFF)
    {
        cb.buf[0] = static_cast<wchar_t>(cp);
        cb.buf[1] = 0;
        cb.len = 1;
    }
    else
    {
        cp -= 0x10000;
        cb.buf[0] = static_cast<wchar_t>((cp >> 10) + 0xD800);
        cb.buf[1] = static_cast<wchar_t>((cp & 0x3FF) + 0xDC00);
        cb.buf[2] = 0;
        cb.len = 2;
    }
    return cb;
}

uint32_t TerminalInput::_bufferToCodepoint(const wchar_t* str) noexcept
{
    if (til::is_leading_surrogate(str[0]) && til::is_trailing_surrogate(str[1]))
    {
        return til::combine_surrogates(str[0], str[1]);
    }
    return str[0];
}

uint32_t TerminalInput::_codepointToLower(uint32_t cp) noexcept
{
    if (cp < 0x80)
    {
        return til::tolower_ascii(cp);
    }

    auto cb = _codepointToBuffer(cp);
    return _bufferToLowerCodepoint(cb.buf, gsl::narrow_cast<int>(std::size(cb.buf)));
}

uint32_t TerminalInput::_bufferToLowerCodepoint(wchar_t* buf, int cap) noexcept
{
    // NOTE: MSDN states that `lpSrcStr == lpDestStr` is valid for LCMAP_LOWERCASE.
    const auto len = LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE, buf, -1, buf, cap);
    // NOTE: LCMapStringW returns the length including the null terminator.
    if (len == 0)
    {
        return 0;
    }
    return _bufferToCodepoint(buf);
}

uint32_t TerminalInput::_getBaseLayoutCodepoint(const WORD vkey) noexcept
{
    // > The base layout key is the key corresponding to the physical key in the standard PC-101 key layout.
    static const auto usLayout = LoadKeyboardLayoutW(L"00000409", 0);

    if (!usLayout || !vkey)
    {
        return 0;
    }

    wchar_t baseChar[4];
    const auto keyState = _getKeyboardState(vkey, 0);
    const auto result = ToUnicodeEx(vkey, 0, keyState.data(), baseChar, 4, 4, usLayout);

    if (result == 0)
    {
        return 0;
    }

    // > [...] pressing the ctrl+С key will be ctrl+c in the standard layout.
    // > So the terminal should send the base layout key as 99 corresponding to the c key.
    //
    // Why use many words when few do trick? base-layout = lowercase.
    return _bufferToLowerCodepoint(baseChar, gsl::narrow_cast<int>(std::size(baseChar)));
}
