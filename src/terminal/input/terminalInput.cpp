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

// These are the basic CSI sequence modifier bits (2nd parameter).
#define CSI_SHIFT 1
#define CSI_ALT 2
#define CSI_CTRL 4

static constexpr uint32_t InvalidCodepoint = 0x110000;
static constexpr uint8_t KittyKeyCodeTextSentinel = 0; // Indicates that the key is not a "functional key"
static constexpr uint8_t KittyKeyCodeLegacySentinel = 1; // Indicates to fall back to legacy handling, aka _encodeRegular

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

    auto& stack = _inAlternateBuffer ? _kittyAltStack : _kittyMainStack;
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

    auto& stack = _inAlternateBuffer ? _kittyAltStack : _kittyMainStack;

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

    // GH#4999 - If we're in win32-input mode, skip straight to doing that.
    // Since this mode handles all types of key events, do nothing else.
    //
    // The kitty keyboard protocol takes precedence, because it's cross-platform.
    if (_inputMode.test(Mode::Win32) && !_forceDisableWin32InputMode && !_kittyFlags)
    {
        return _makeWin32Output(event.Event.KeyEvent);
    }

    SanitizedKeyEvent key{
        .virtualKey = event.Event.KeyEvent.wVirtualKeyCode,
        .scanCode = event.Event.KeyEvent.wVirtualScanCode,
        .codepoint = event.Event.KeyEvent.uChar.UnicodeChar,
        .controlKeyState = _trackControlKeyState(event.Event.KeyEvent),
        .keyDown = event.Event.KeyEvent.bKeyDown != FALSE,
    };

    // Swallow lone leading surrogates...
    if (til::is_leading_surrogate(event.Event.KeyEvent.uChar.UnicodeChar))
    {
        _leadingSurrogate = event.Event.KeyEvent.uChar.UnicodeChar;
        return _makeNoOutput();
    }
    // ...and combine them with trailing surrogates.
    if (_leadingSurrogate != 0 && til::is_trailing_surrogate(event.Event.KeyEvent.uChar.UnicodeChar))
    {
        key.codepoint = til::combine_surrogates(_leadingSurrogate, event.Event.KeyEvent.uChar.UnicodeChar);
    }
    _leadingSurrogate = 0;

    if (key.keyDown)
    {
        // If this is a VK_PACKET or 0 virtual key, it's likely a synthesized
        // keyboard event, so the UnicodeChar is transmitted as is. This must be
        // handled before the Auto Repeat test, other we'll end up dropping chars.
        if (key.virtualKey == VK_PACKET || key.virtualKey == 0)
        {
            return _makeCharOutput(key.codepoint);
        }

        // If it's a numeric keypad key, and Alt is pressed (but not Ctrl),
        // then this is an Alt-Numpad composition, and we should ignore these keys.
        // The generated character will be transmitted when the Alt is released.
        if (key.virtualKey >= VK_NUMPAD0 && key.virtualKey <= VK_NUMPAD9 &&
            (key.controlKeyState == LEFT_ALT_PRESSED || key.controlKeyState == RIGHT_ALT_PRESSED))
        {
            return _makeNoOutput();
        }
    }

    // Keep track of key repeats.
    key.keyRepeat = _lastVirtualKeyCode == key.virtualKey;
    if (key.keyDown)
    {
        _lastVirtualKeyCode = key.virtualKey;
    }
    else if (key.keyRepeat)
    {
        _lastVirtualKeyCode = std::nullopt;
    }

    // If this is a repeat of the last recorded key press, and Auto Repeat Mode
    // is disabled, then we should suppress this event.
    if (key.keyRepeat && !_inputMode.test(Mode::AutoRepeat))
    {
        return _makeNoOutput();
    }

    // There's a bunch of early returns we can place on key-up events,
    // before we run our more complex encoding logic below.
    if (!key.keyDown)
    {
        // If NumLock is on, and this is an Alt release with a unicode char,
        // it must be the generated character from an Alt-Numpad composition.
        if (WI_IsFlagSet(key.controlKeyState, NUMLOCK_ON) && key.virtualKey == VK_MENU && key.codepoint != 0)
        {
            return _makeCharOutput(key.codepoint);
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
            (key.virtualKey == VK_RETURN || key.virtualKey == VK_TAB || key.virtualKey == VK_BACK))
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
    key.leftCtrlIsReallyPressed = WI_IsFlagSet(key.controlKeyState, LEFT_CTRL_PRESSED);
    if (WI_AreAllFlagsSet(key.controlKeyState, LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED))
    {
        const auto max = std::max(_lastLeftCtrlTime, _lastRightAltTime);
        const auto min = std::min(_lastLeftCtrlTime, _lastRightAltTime);
        key.leftCtrlIsReallyPressed = (max - min) > 50;
    }

    KeyboardHelper kbd;
    EncodingHelper enc;
    WI_SetFlagIf(enc.csiModifier, CSI_CTRL, key.leftCtrlIsReallyPressed || WI_IsFlagSet(key.controlKeyState, RIGHT_CTRL_PRESSED));
    WI_SetFlagIf(enc.csiModifier, CSI_ALT, WI_IsAnyFlagSet(key.controlKeyState, ALT_PRESSED));
    WI_SetFlagIf(enc.csiModifier, CSI_SHIFT, WI_IsFlagSet(key.controlKeyState, SHIFT_PRESSED));

    if (_kittyFlags == 0 || !_encodeKitty(kbd, enc, key))
    {
        _encodeRegular(enc, key);
    }

    std::wstring seq;
    if (!_formatEncodingHelper(enc, seq))
    {
        _formatFallback(kbd, enc, key, seq);
    }
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
{
    // The CSI and SS3 introducers are C1 control codes, which can either be
    // sent as a single codepoint, or as a two character escape sequence.
    if (_inputMode.test(Mode::SendC1))
    {
        _csi = L"\x9B";
        _ss3 = L"\x8F";
        _focusInSequence = L"\x9BI";
        _focusOutSequence = L"\x9BO";
    }
    else
    {
        _csi = L"\x1B[";
        _ss3 = L"\x1BO";
        _focusInSequence = L"\x1B[I";
        _focusOutSequence = L"\x1B[O";
    }
}

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

uint32_t TerminalInput::_makeCtrlChar(const uint32_t ch) noexcept
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
        static constexpr std::array<uint8_t, 7> numericCtrls{ 0, 27, 28, 29, 30, 31, 127 };
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
    std::wstring str;
    _stringPushCodepoint(str, cp);
    return str;
}

TerminalInput::StringType TerminalInput::_makeNoOutput() noexcept
{
    return {};
}

// Turns an KEY_EVENT_RECORD into a win32-input-mode VT sequence.
// It allows us to send KEY_EVENT_RECORD data losslessly to conhost.
TerminalInput::OutputType TerminalInput::_makeWin32Output(const KEY_EVENT_RECORD& key)
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

bool TerminalInput::_encodeKitty(KeyboardHelper& kbd, EncodingHelper& enc, const SanitizedKeyEvent& key) noexcept
{
    static constexpr auto isKittyFunctionalKey = [](uint32_t cp) noexcept {
        return
            // C0 control code (0x01 is not assigned - we use it as a sentinel value)
            (cp > KittyKeyCodeLegacySentinel && cp <= 0x1f) ||
            // C1 control code (although only 0x7f = Esc is currently assigned)
            (cp >= 0x7f && cp <= 0x9f) ||
            // And any other regular Kitty PUA code
            (cp >= 0xE000 && cp <= 0xF8FF);
    };
    static constexpr auto isKittyValidText = [](uint32_t cp) noexcept {
        return (cp > 0x1f && cp < 0x7f) || (cp > 0x9f && cp < InvalidCodepoint);
    };
    static constexpr auto isTextKey = [](uint32_t fk) noexcept {
        return fk == KittyKeyCodeTextSentinel;
    };
    static constexpr auto isAnyFunctionalKey = [](uint32_t fk) noexcept {
        return fk > KittyKeyCodeTextSentinel;
    };
    static constexpr auto isNonLegacyFunctionalKey = [](uint32_t fk) noexcept {
        return fk > KittyKeyCodeLegacySentinel;
    };

    const auto functionalKeyCode = _getKittyFunctionalKeyCode(key.virtualKey, key.scanCode, WI_IsFlagSet(key.controlKeyState, ENHANCED_KEY));

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::DisambiguateEscapeCodes))
    {
        // KKP> Turning on [DisambiguateEscapeCodes] will cause the terminal to
        // KKP> report the Esc, alt+key, ctrl+key, ctrl+alt+key, shift+alt+key
        // KKP> keys using CSI u sequences instead of legacy ones.
        // KKP> Here key is any ASCII key as described in Legacy text keys. [...]
        //
        // NOTE: The specification is wrong. It's actually either Esc, or
        //       *any modifier with any key* (!).
        //
        // KKP> Legacy text keys:
        // KKP> For legacy compatibility, the keys a-z 0-9 ` - = [ ] \ ; ' , . /
        //
        // NOTE: Again, the list of keys is flat out wrong. It's actually all text-keys,
        //       plus classic C0 keys Escape, Return, Tab, and Backspace. Since it involves
        //       text keys, we'll all of them below, together with ReportAllKeysAsEscapeCodes.
        //
        // KKP> Additionally, all non text keypad keys will be reported [...] with CSI u encoding, [...].

        constexpr uint32_t ESCAPE = 27;
        constexpr uint32_t KP_0 = 57399; // The lowest keypad key
        constexpr uint32_t KP_BEGIN = 57427; // and the highest one

        if (functionalKeyCode == ESCAPE || (functionalKeyCode >= KP_0 && functionalKeyCode <= KP_BEGIN))
        {
            enc.csiUnicodeKeyCode = functionalKeyCode;
            enc.csiFinal = L'u';
        }
    }

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportEventTypes))
    {
        // KKP> This [...] causes the terminal to report key repeat and key release events.
        if (!key.keyDown)
        {
            enc.csiEventType = 3; // release event
        }
        else if (key.keyRepeat)
        {
            enc.csiEventType = 2; // repeat event
        }
    }

    if (
        // KKP> This [...] turns on key reporting even for key events that generate text.
        // KKP> [...] text will not be sent, instead only key events are sent.
        // KKP> [...] with this mode, events for pressing modifier keys are reported.
        //
        // In other words: Get the functional key code if any; otherwise use the codepoint.
        WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes) ||
        // A continuation of DisambiguateEscapeCodes above: modifier + key = CSI u.
        (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::DisambiguateEscapeCodes) &&
         functionalKeyCode <= 127 && // As documented above: All text keys (=0) and control keys (1..127)
         enc.csiModifier != 0) ||
        // Enabling ReportEventTypes implies that `CSI u` is used for all key up events.
        enc.csiEventType == 3)
    {
        if (isKittyFunctionalKey(functionalKeyCode))
        {
            enc.csiUnicodeKeyCode = functionalKeyCode;
            enc.csiFinal = L'u';
        }
        else if (isTextKey(functionalKeyCode))
        {
            const auto cp = kbd.getKittyBaseKey(key);

            if (cp < InvalidCodepoint)
            {
                enc.csiUnicodeKeyCode = cp;
                enc.csiFinal = L'u';
            }
        }

        if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAssociatedText))
        {
            // KKP> This [...] causes key events that generate text to be reported
            // KKP> as CSI u escape codes with the text embedded in the escape code.
            //
            // KKP> Note that this flag is an enhancement to Report all keys as escape codes [...].
            //
            // KKP> The associated text must not contain control codes [...].
            //
            // NOTE: What the specification fails to mention is that it's not reported for key-up events.
            if (enc.csiEventType != 3 && isKittyValidText(key.codepoint))
            {
                enc.csiTextAsCodepoint = key.codepoint;
            }
        }
    }

    if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAlternateKeys))
    {
        // KKP> This [...] causes the terminal to report alternate key values [...]
        //
        // KKP> Note that [...] only key events represented as escape codes due to the
        // KKP> other enhancements in effect will be affected by this enhancement. [...]

        if (enc.csiUnicodeKeyCode)
        {
            // KKP> [...] the terminal can [...] [additionally send] the shifted key and base layout key [...].
            // KKP> The shifted key is [...] the upper-case version of unicode-codepoint,
            // KKP> or more technically, the shifted version, in the currently active keyboard layout.
            //
            // NOTE that the 2nd line is wrong. On a US layout, Shift+3 is not an "uppercase 3",
            // it's "#", which is exactly what kitty sends as the "shifted key" parameter.
            // (This is in addition to "unicode-codepoint" never being defined throughout the spec...)

            // KKP> Note that the shifted key must be present only if shift is also present in the modifiers.

            if (isTextKey(functionalKeyCode) && enc.shiftPressed())
            {
                // This is almost identical to our computation of the "base key" for
                // ReportAllKeysAsEscapeCodes above, but this time with SHIFT_PRESSED.
                const auto cp = kbd.getKittyShiftedKey(key);

                if (cp < InvalidCodepoint)
                {
                    enc.csiAltKeyCodeShifted = cp;
                }
            }

            if (key.scanCode)
            {
                // > The base layout key is the key corresponding to the physical key in the standard PC-101 key layout.
                //
                // NOTE: The specification doesn't mention that the value is
                // not reported if it's identical to the regular key-code.
                const auto cp = kbd.getKittyUSBaseKey(key);
                if (cp < InvalidCodepoint && cp != enc.csiUnicodeKeyCode)
                {
                    enc.csiAltKeyCodeBase = cp;
                }
            }
        }
    }

    // As per KKP: shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
    // KKP> Lock modifiers are not reported for text producing keys, [...].
    // KKP> To get lock modifiers for all keys use the Report all keys as escape codes enhancement.
    if (isKittyFunctionalKey(enc.csiUnicodeKeyCode) || WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes))
    {
        if (WI_IsFlagSet(key.controlKeyState, CAPSLOCK_ON))
        {
            enc.csiModifier |= 64;
        }
        if (WI_IsFlagSet(key.controlKeyState, NUMLOCK_ON))
        {
            enc.csiModifier |= 128;
        }
    }

    // The code above should either:
    // * Emit a proper CSI <num> u sequence, or
    // * Set whatever kitty fields we want and leave csiFinal at zero.
    //   (This then triggers the standard key encoding logic below.)
    assert(enc.csiFinal == 0 || enc.csiUnicodeKeyCode > KittyKeyCodeLegacySentinel);

    return enc.csiFinal != 0;
}

uint32_t TerminalInput::_getKittyFunctionalKeyCode(UINT vkey, WORD scanCode, bool enhanced) noexcept
{
    // Most KKP functional keys are rather predictable. This LUT helps cover most of them.
    // Some keys however depend on the key state (specifically the enhanced bit).
    static constexpr auto lut = [] {
        std::array<uint8_t, 256> data{};

#define SET(key, value) data.at(key) = static_cast<uint8_t>(value);

        SET(VK_CAPITAL, 57358); // CAPS_LOCK
        SET(VK_SCROLL, 57359); // SCROLL_LOCK
        SET(VK_NUMLOCK, 57360); // NUM_LOCK
        SET(VK_SNAPSHOT, 57361); // PRINT_SCREEN
        SET(VK_PAUSE, 57362); // PAUSE
        SET(VK_APPS, 57363); // MENU

        SET(VK_F1, KittyKeyCodeLegacySentinel);
        SET(VK_F2, KittyKeyCodeLegacySentinel);
        SET(VK_F3, KittyKeyCodeLegacySentinel);
        SET(VK_F4, KittyKeyCodeLegacySentinel);
        SET(VK_F5, KittyKeyCodeLegacySentinel);
        SET(VK_F6, KittyKeyCodeLegacySentinel);
        SET(VK_F7, KittyKeyCodeLegacySentinel);
        SET(VK_F8, KittyKeyCodeLegacySentinel);
        SET(VK_F9, KittyKeyCodeLegacySentinel);
        SET(VK_F10, KittyKeyCodeLegacySentinel);
        SET(VK_F11, KittyKeyCodeLegacySentinel);
        SET(VK_F12, KittyKeyCodeLegacySentinel);
        SET(VK_F13, 57376); // F13
        SET(VK_F14, 57377); // F14
        SET(VK_F15, 57378); // F15
        SET(VK_F16, 57379); // F16
        SET(VK_F17, 57380); // F17
        SET(VK_F18, 57381); // F18
        SET(VK_F19, 57382); // F19
        SET(VK_F20, 57383); // F20
        SET(VK_F21, 57384); // F21
        SET(VK_F22, 57385); // F22
        SET(VK_F23, 57386); // F23
        SET(VK_F24, 57387); // F24

        SET(VK_NUMPAD0, 57399); // KP_0
        SET(VK_NUMPAD1, 57400); // KP_1
        SET(VK_NUMPAD2, 57401); // KP_2
        SET(VK_NUMPAD3, 57402); // KP_3
        SET(VK_NUMPAD4, 57403); // KP_4
        SET(VK_NUMPAD5, 57404); // KP_5
        SET(VK_NUMPAD6, 57405); // KP_6
        SET(VK_NUMPAD7, 57406); // KP_7
        SET(VK_NUMPAD8, 57407); // KP_8
        SET(VK_NUMPAD9, 57408); // KP_9
        SET(VK_DECIMAL, 57409); // KP_DECIMAL
        SET(VK_DIVIDE, 57410); // KP_DIVIDE
        SET(VK_MULTIPLY, 57411); // KP_MULTIPLY
        SET(VK_SUBTRACT, 57412); // KP_SUBTRACT
        SET(VK_ADD, 57413); // KP_ADD
        SET(VK_RETURN, 57414); // KP_ENTER
        SET(VK_SEPARATOR, 57416); // KP_SEPARATOR

        // All the following keys only apply if enhanced=false. This is because
        // enhanced=true implies non-keypad keys, which as per KKP means legacy encoding.
        SET(VK_LEFT, 57417); // KP_LEFT
        SET(VK_RIGHT, 57418); // KP_RIGHT
        SET(VK_UP, 57419); // KP_UP
        SET(VK_DOWN, 57420); // KP_DOWN
        SET(VK_PRIOR, 57421); // KP_PAGE_UP
        SET(VK_NEXT, 57422); // KP_PAGE_DOWN
        SET(VK_HOME, 57423); // KP_HOME
        SET(VK_END, 57424); // KP_END
        SET(VK_INSERT, 57425); // KP_INSERT
        SET(VK_DELETE, 57426); // KP_DELETE
        SET(VK_CLEAR, KittyKeyCodeLegacySentinel); // KP_BEGIN

        SET(VK_MEDIA_PLAY_PAUSE, 57430); // MEDIA_PLAY_PAUSE
        SET(VK_MEDIA_STOP, 57432); // MEDIA_STOP
        SET(VK_MEDIA_NEXT_TRACK, 57435); // MEDIA_TRACK_NEXT
        SET(VK_MEDIA_PREV_TRACK, 57436); // MEDIA_TRACK_PREVIOUS
        SET(VK_VOLUME_DOWN, 57438); // LOWER_VOLUME
        SET(VK_VOLUME_UP, 57439); // RAISE_VOLUME
        SET(VK_VOLUME_MUTE, 57440); // MUTE_VOLUME

        SET(VK_LSHIFT, 57441); // LEFT_SHIFT
        SET(VK_LCONTROL, 57442); // LEFT_CONTROL
        SET(VK_LMENU, 57443); // LEFT_ALT
        SET(VK_LWIN, 57444); // LEFT_SUPER
        SET(VK_RSHIFT, 57447); // RIGHT_SHIFT
        SET(VK_RCONTROL, 57448); // RIGHT_CONTROL
        SET(VK_RMENU, 57449); // RIGHT_ALT
        SET(VK_RWIN, 57450); // RIGHT_SUPER

#undef SET

        return data;
    }();

    switch (vkey)
    {
        // These keys don't fall into the Private Use Area (ugh).
    case VK_ESCAPE:
        return 27; // ESCAPE
    case VK_RETURN:
        return enhanced ? 57414 : 13; // KP_ENTER : ENTER
    case VK_TAB:
        return 9; // TAB
    case VK_BACK:
        return 127; // BACKSPACE

        // These keys depend on the enhanced bit.
        // enhanced=true means they're not keypad keys.
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_HOME:
    case VK_END:
    case VK_INSERT:
    case VK_DELETE:
        if (enhanced)
        {
            return KittyKeyCodeLegacySentinel;
        }
        break;

        // These non-left/right vkeys need to be mapped to left/right variants first.
    case VK_SHIFT:
        // I've extracted the scan codes from all keyboard layouts that ship with Windows,
        // and I've found that all of them use scan code 0x2A for VK_LSHIFT and 0x36 for VK_RSHIFT.
        vkey = scanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
        break;
    case VK_CONTROL:
        vkey = enhanced ? VK_RCONTROL : VK_LCONTROL;
        break;
    case VK_MENU:
        vkey = enhanced ? VK_RMENU : VK_LMENU;
        break;

    default:
        break;
    }

    const auto v = til::at(lut, vkey & 0xff);
    return v <= KittyKeyCodeLegacySentinel ? v : 0xE000 + v;
}

void TerminalInput::_encodeRegular(EncodingHelper& enc, const SanitizedKeyEvent& key) const noexcept
{
    const auto modified = enc.csiModifier != 0;
    const auto enhanced = WI_IsFlagSet(key.controlKeyState, ENHANCED_KEY);
    const auto kitty = WI_IsAnyFlagSet(
        _kittyFlags,
        KittyKeyboardProtocolFlags::DisambiguateEscapeCodes |
            KittyKeyboardProtocolFlags::ReportEventTypes |
            KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes);

    switch (key.virtualKey)
    {
    case VK_BACK:
    {
        // BACKSPACE maps to either DEL or BS, depending on the Backarrow Key mode.
        // The Ctrl modifier inverts the active mode, swapping BS and DEL (this is
        // not standard, but a modern terminal convention). The Alt modifier adds
        // an ESC prefix (also not standard).
        enc.altPrefix = true;
        const auto ctrl = (enc.csiModifier & CSI_CTRL) == 0;
        const auto back = _inputMode.test(Mode::BackarrowKey);
        enc.plain = ctrl != back ? L"\x7f"sv : L"\b"sv;
        break;
    }
    case VK_TAB:
        // TAB maps to HT, and Shift+TAB to CBT. The Ctrl modifier has no effect.
        // The Alt modifier adds an ESC prefix, although in practice all the Alt
        // mappings are likely to be system hotkeys.
        enc.altPrefix = true;
        if ((enc.csiModifier & CSI_SHIFT) == 0)
        {
            enc.plain = L"\t"sv;
        }
        else
        {
            // While CBT is a CSI sequence, it doesn't take any parameters or modifiers.
            // To prevent any being added accidentally, we use enc.plain instead.
            enc.plain = _inputMode.test(Mode::SendC1) ? L"\x9BZ"sv : L"\x1B[Z"sv;
        }
        break;
    case VK_RETURN:
        // RETURN maps to either CR or CR LF, depending on the Line Feed mode. With
        // a Ctrl modifier it maps to LF, because that's the expected behavior for
        // most PC keyboard layouts. The Alt modifier adds an ESC prefix.
        // The keypad RETURN key works different.
        enc.altPrefix = true;
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad) && enhanced)
        {
            if (_inputMode.test(Mode::Ansi))
            {
                enc.ss3Final = L'M';
            }
            else
            {
                enc.plain = L"\033?M"sv;
            }
        }
        else
        {
            if ((enc.csiModifier & CSI_CTRL) == 0)
            {
                enc.plain = _inputMode.test(Mode::LineFeed) ? L"\r\n"sv : L"\r"sv;
            }
            else
            {
                enc.plain = L"\n"sv;
            }
        }
        break;

    case VK_PAUSE:
        // PAUSE doesn't have a VT mapping, but traditionally we've mapped it to ^Z,
        // regardless of modifiers.
        enc.plain = L"\x1A"sv;
        break;

    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
        // F1 to F4 map to the VT keypad function keys, which are SS3 sequences.
        // When combined with a modifier, we use CSI sequences with the modifier
        // embedded as a parameter (not standard - a modern terminal extension).
        // VT52 only supports PF1 through PF4 function keys with ESC prefix.
        if (_inputMode.test(Mode::Ansi))
        {
            // KKP> The original version of this specification allowed F3 to
            // KKP> be encoded as both CSI R and CSI ~ [and now it doesn't].
            if (kitty && key.virtualKey == VK_F3)
            {
                enc.csiFinal = L'~';
                enc.csiUnicodeKeyCode = 13;
            }
            else
            {
                auto& dst = kitty || modified ? enc.csiFinal : enc.ss3Final;
                dst = L'P' + (key.virtualKey - VK_F1);
            }
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033P", L"\033Q", L"\033R", L"\033S" };
            enc.plain = til::at(lut, key.virtualKey - VK_F1);
        }
        break;

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
        // F5 through F20 map to the top row VT function keys. They use standard
        // DECFNK sequences with the modifier embedded as a parameter. The first
        // five function keys on a VT terminal are typically local functions, so
        // there's not much need to support mappings for them.
        // In VT52 mode, only F11-F13 are defined (as control keys).
        if (_inputMode.test(Mode::Ansi))
        {
            static constexpr uint8_t lut[] = { 15, 17, 18, 19, 20, 21, 23, 24, 25, 26, 28, 29, 31, 32, 33, 34 };
            enc.csiFinal = L'~';
            enc.csiUnicodeKeyCode = til::at(lut, key.virtualKey - VK_F5);
        }
        else
        {
            switch (key.virtualKey)
            {
            case VK_F11:
                enc.plain = L"\033"sv;
                break;
            case VK_F12:
                enc.plain = L"\b"sv;
                break;
            case VK_F13:
                enc.plain = L"\n"sv;
                break;
            default:
                break;
            }
        }
        break;

    case VK_LEFT: // 0x25 = D
    case VK_UP: // 0x26 = A
    case VK_RIGHT: // 0x27 = C
    case VK_DOWN: // 0x28 = B
        // Cursor keys follow a similar pattern to the VT keypad function keys,
        // although they only use an SS3 prefix when the Cursor Key mode is set.
        // When combined with a modifier, they'll use CSI sequences with the
        // modifier embedded as a parameter (again not standard).
        // In VT52 mode, cursor keys use ESC prefix.
        if (_inputMode.test(Mode::Ansi))
        {
            static constexpr uint8_t lut[] = { 'D', 'A', 'C', 'B' };
            const auto csi = kitty || modified || !_inputMode.test(Mode::CursorKey);
            auto& dst = csi ? enc.csiFinal : enc.ss3Final;
            dst = til::at(lut, key.virtualKey - VK_LEFT);
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033D", L"\033A", L"\033C", L"\033B" };
            enc.plain = til::at(lut, key.virtualKey - VK_LEFT);
        }
        break;
    case VK_CLEAR:
        if (_inputMode.test(Mode::Ansi))
        {
            const auto csi = kitty || modified || !_inputMode.test(Mode::CursorKey);
            auto& dst = csi ? enc.csiFinal : enc.ss3Final;
            dst = L'E';
        }
        else
        {
            enc.plain = L"\033E"sv;
        }
        break;
    case VK_HOME:
        if (_inputMode.test(Mode::Ansi))
        {
            const auto csi = kitty || modified || !_inputMode.test(Mode::CursorKey);
            auto& dst = csi ? enc.csiFinal : enc.ss3Final;
            dst = L'H';
        }
        else
        {
            enc.plain = L"\033H"sv;
        }
        break;
    case VK_END:
        if (_inputMode.test(Mode::Ansi))
        {
            const auto csi = kitty || modified || !_inputMode.test(Mode::CursorKey);
            auto& dst = csi ? enc.csiFinal : enc.ss3Final;
            dst = L'F';
        }
        else
        {
            enc.plain = L"\033F"sv;
        }
        break;

    case VK_INSERT: // 0x2D = 2
    case VK_DELETE: // 0x2E = 3
        // Editing keys follow the same pattern as the top row VT function
        // keys, using standard DECFNK sequences with the modifier embedded.
        // These are not defined in VT52 mode.
        if (_inputMode.test(Mode::Ansi))
        {
            enc.csiFinal = L'~';
            enc.csiUnicodeKeyCode = 2 + (key.virtualKey - VK_INSERT);
        }
        break;
    case VK_PRIOR: // 0x21 = 5
    case VK_NEXT: // 0x22 = 6
        if (_inputMode.test(Mode::Ansi))
        {
            enc.csiFinal = L'~';
            enc.csiUnicodeKeyCode = 5 + (key.virtualKey - VK_PRIOR);
        }
        break;

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
        // Keypad keys depend on the Keypad mode. When reset, they transmit
        // the ASCII character assigned by the keyboard layout, but when set
        // they transmit SS3 escape sequences. When used with a modifier, the
        // modifier is embedded as a parameter value (not standard).
        // In VT52 mode, keypad keys use ESC ? prefix instead of SS3.
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            if (_inputMode.test(Mode::Ansi))
            {
                enc.ss3Final = L'p' + (key.virtualKey - VK_NUMPAD0);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?p", L"\033?q", L"\033?r", L"\033?s", L"\033?t", L"\033?u", L"\033?v", L"\033?w", L"\033?x", L"\033?y" };
                enc.plain = til::at(lut, key.virtualKey - VK_NUMPAD0);
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
                enc.ss3Final = L'j' + (key.virtualKey - VK_MULTIPLY);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?j", L"\033?k", L"\033?l", L"\033?m", L"\033?n", L"\033?o" };
                enc.plain = til::at(lut, key.virtualKey - VK_MULTIPLY);
            }
        }
        break;

    default:
        break;
    }
}

bool TerminalInput::_formatEncodingHelper(EncodingHelper& enc, std::wstring& seq) const
{
    // NOTE: altPrefix is only ever true for `_fillRegularKeyEncodingInfo` calls,
    // and only if one of the 3 conditions below applies.
    // In other words, we return with an unmodified `str` if `enc` is unmodified.
    if (enc.altPrefix && enc.altPressed() && _inputMode.test(Mode::Ansi))
    {
        seq.push_back(L'\x1b');
    }

    if (enc.csiFinal)
    {
        // If we have an event-type, we need a modifier parameter as well.
        if (enc.csiModifier || enc.csiEventType)
        {
            // The CSI modifier value is 1 based.
            enc.csiModifier += 1;
            // If the modifier is non-default, the first parameter must not be missing.
            enc.csiUnicodeKeyCode = std::max<uint32_t>(enc.csiUnicodeKeyCode, 1);
        }

        seq.append(_csi);

        if (enc.csiUnicodeKeyCode)
        {
            fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), enc.csiUnicodeKeyCode);
        }
        if (enc.csiAltKeyCodeShifted || enc.csiAltKeyCodeBase)
        {
            seq.push_back(L':');
            if (enc.csiAltKeyCodeShifted)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), enc.csiAltKeyCodeShifted);
            }
            if (enc.csiAltKeyCodeBase)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L":{}"), enc.csiAltKeyCodeBase);
            }
        }
        if (enc.csiModifier || enc.csiTextAsCodepoint)
        {
            seq.push_back(L';');
            if (enc.csiModifier)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), enc.csiModifier);
                if (enc.csiEventType)
                {
                    fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L":{}"), enc.csiEventType);
                }
            }
            if (enc.csiTextAsCodepoint)
            {
                fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L";{}"), enc.csiTextAsCodepoint);
            }
        }

        seq.push_back(enc.csiFinal);
        return true;
    }

    if (enc.ss3Final)
    {
        seq.append(_ss3);
        seq.push_back(enc.ss3Final);
        return true;
    }

    if (!enc.plain.empty())
    {
        seq.append(enc.plain);
        return true;
    }

    return false;
}

void TerminalInput::_formatFallback(KeyboardHelper& kbd, const EncodingHelper& enc, const SanitizedKeyEvent& key, std::wstring& seq) const
{
    // If this is a modifier, it won't produce output, so we can return early.
    if (key.virtualKey >= VK_SHIFT && key.virtualKey <= VK_MENU)
    {
        return;
    }

    const auto anyAltPressed = key.anyAltPressed();
    auto codepoint = key.codepoint;

    // If it's not in the key map, we'll use the UnicodeChar, if provided,
    // except in the case of Ctrl+Space, which is often mapped incorrectly as
    // a space character when it's expected to be mapped to NUL. We need to
    // let that fall through to the standard mapping algorithm below.
    const auto ctrlSpaceKey = enc.ctrlPressed() && key.virtualKey == VK_SPACE;
    if (codepoint != 0 && !ctrlSpaceKey)
    {
        // In the case of an AltGr key, we may still need to apply a Ctrl
        // modifier to the char, either because both Ctrl keys were pressed,
        // or we got a LeftCtrl that was distinctly separate from the RightAlt.
        const auto altGrPressed = key.altGrPressed();
        const auto bothAltPressed = key.bothAltPressed();
        const auto bothCtrlPressed = key.bothCtrlPressed();
        const auto rightAltPressed = key.rightAltPressed();

        if (altGrPressed && (bothCtrlPressed || (rightAltPressed && key.leftCtrlIsReallyPressed)))
        {
            codepoint = _makeCtrlChar(codepoint);
        }

        // We may also need to apply an Alt prefix to the char sequence, but
        // if this is an AltGr key, we only do so if both Alts are pressed.
        const auto wantsEscPrefix = altGrPressed ? bothAltPressed : anyAltPressed;
        if (wantsEscPrefix && _inputMode.test(Mode::Ansi))
        {
            seq.push_back(L'\x1b');
        }
    }
    // If we don't have a UnicodeChar, we'll try and determine what the key
    // would have transmitted without any Ctrl or Alt modifiers applied. But
    // this only makes sense if there were actually modifiers pressed.
    else if (anyAltPressed || WI_IsAnyFlagSet(key.controlKeyState, CTRL_PRESSED))
    {
        codepoint = kbd.getUnmodifiedKeyboardKey(key);
        if (codepoint >= InvalidCodepoint)
        {
            return;
        }

        // If Alt is pressed, that also needs to be applied to the sequence.
        if (anyAltPressed && _inputMode.test(Mode::Ansi))
        {
            seq.push_back(L'\x1b');
        }

        // Once we've got the base character, we can apply the Ctrl modifier.
        if (enc.ctrlPressed())
        {
            codepoint = _makeCtrlChar(codepoint);
            // If we haven't found a Ctrl mapping for the key, and it's one of
            // the alphanumeric keys, we try again using the virtual key code.
            // On keyboard layouts where the alphanumeric keys are not mapped to
            // their typical ASCII values, this provides a simple fallback.
            if (codepoint >= L' ' && key.virtualKey >= '2' && key.virtualKey <= 'Z')
            {
                codepoint = _makeCtrlChar(key.virtualKey);
            }
        }
    }
    else
    {
        return;
    }

    _stringPushCodepoint(seq, codepoint);
}

void TerminalInput::_stringPushCodepoint(std::wstring& str, uint32_t cp)
{
    const CodepointBuffer cb{ cp };
    str.append(&cb.buf[0], cb.len);
}

uint32_t TerminalInput::_codepointToLower(uint32_t cp) noexcept
{
    if (cp >= InvalidCodepoint)
    {
        return cp;
    }
    else if (cp < 0x80)
    {
        return til::tolower_ascii(cp);
    }
    else
    {
        CodepointBuffer cb{ cp };
        cb.convertLowercase();
        return cb.asSingleCodepoint();
    }
}

TerminalInput::CodepointBuffer::CodepointBuffer(uint32_t cp) noexcept
{
    if (cp <= 0xFFFF)
    {
        buf[0] = gsl::narrow_cast<wchar_t>(cp);
        buf[1] = 0;
        len = 1;
    }
    else
    {
        cp -= 0x10000;
        buf[0] = gsl::narrow_cast<wchar_t>((cp >> 10) + 0xD800);
        buf[1] = gsl::narrow_cast<wchar_t>((cp & 0x3FF) + 0xDC00);
        buf[2] = 0;
        len = 2;
    }
}

void TerminalInput::CodepointBuffer::convertLowercase() noexcept
{
    // NOTE: MSDN states that `lpSrcStr == lpDestStr` is valid for LCMAP_LOWERCASE.
    len = LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE, &buf[0], len, &buf[0], ARRAYSIZE(buf));
    // NOTE: LCMapStringW returns the length including the null terminator.
    len -= 1;
}

uint32_t TerminalInput::CodepointBuffer::asSingleCodepoint() const noexcept
{
    if (len == 1)
    {
        if (!til::is_surrogate(buf[0]))
        {
            return buf[0];
        }
    }
    else if (len == 2)
    {
        if (til::is_leading_surrogate(buf[0]) && til::is_trailing_surrogate(buf[1]))
        {
            return til::combine_surrogates(buf[0], buf[1]);
        }
    }
    return InvalidCodepoint;
}

bool TerminalInput::SanitizedKeyEvent::anyAltPressed() const noexcept
{
    return WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED);
}

bool TerminalInput::SanitizedKeyEvent::bothAltPressed() const noexcept
{
    return WI_AreAllFlagsSet(controlKeyState, ALT_PRESSED);
}

bool TerminalInput::SanitizedKeyEvent::rightAltPressed() const noexcept
{
    return WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED);
}

bool TerminalInput::SanitizedKeyEvent::bothCtrlPressed() const noexcept
{
    return WI_AreAllFlagsSet(controlKeyState, CTRL_PRESSED);
}

bool TerminalInput::SanitizedKeyEvent::altGrPressed() const noexcept
{
    return WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED) && WI_IsAnyFlagSet(controlKeyState, CTRL_PRESSED);
}

uint32_t TerminalInput::KeyboardHelper::getUnmodifiedKeyboardKey(const SanitizedKeyEvent& key) noexcept
{
    const auto virtualKey = key.virtualKey;
    const auto controlKeyState = key.controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED);
    return getKeyboardKey(virtualKey, controlKeyState, nullptr);
}

uint32_t TerminalInput::KeyboardHelper::getKittyBaseKey(const SanitizedKeyEvent& key) noexcept
{
    const auto virtualKey = key.virtualKey;
    const auto controlKeyState = key.controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | SHIFT_PRESSED | CAPSLOCK_ON);
    return _codepointToLower(getKeyboardKey(virtualKey, controlKeyState, nullptr));
}

uint32_t TerminalInput::KeyboardHelper::getKittyShiftedKey(const SanitizedKeyEvent& key) noexcept
{
    const auto virtualKey = key.virtualKey;
    const auto controlKeyState = key.controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | CAPSLOCK_ON) | SHIFT_PRESSED;
    return getKeyboardKey(virtualKey, controlKeyState, nullptr);
}

uint32_t TerminalInput::KeyboardHelper::getKittyUSBaseKey(const SanitizedKeyEvent& key) noexcept
{
    // > The base layout key is the key corresponding to the physical key in the standard PC-101 key layout.
    static const auto usLayout = LoadKeyboardLayoutW(L"00000409", 0);
    if (!usLayout)
    {
        return InvalidCodepoint;
    }

    const auto vkey = MapVirtualKeyExW(key.scanCode, MAPVK_VSC_TO_VK_EX, usLayout);
    if (!vkey)
    {
        return InvalidCodepoint;
    }

    // KKP doesn't document whether the "base layout key" should also
    // properly map function keys using the >0xE000 Private Use Area codes.
    // I'm just going to do it.
    auto keyCode = _getKittyFunctionalKeyCode(vkey, key.scanCode, WI_IsFlagSet(key.controlKeyState, ENHANCED_KEY));

    // By extension, KKP also doesn't document what to do with function keys
    // that only have a canonical legacy encoding (no assigned PUA code).
    // Here I'll just treat them as unmapped.
    if (keyCode == KittyKeyCodeLegacySentinel)
    {
        return InvalidCodepoint;
    }

    // Otherwise, map any text key to their text code.
    if (keyCode == 0)
    {
        const auto controlKeyState = key.controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | SHIFT_PRESSED | CAPSLOCK_ON);
        keyCode = getKeyboardKey(vkey, controlKeyState, usLayout);
        keyCode = _codepointToLower(keyCode);
    }

    return keyCode;
}

uint32_t TerminalInput::KeyboardHelper::getKeyboardKey(UINT vkey, DWORD controlKeyState, HKL hkl) noexcept
{
    init();

    if (!hkl)
    {
        hkl = _keyboardLayout;
    }

    vkey &= 0xff;

    const uint8_t rightAlt = WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED) ? 0x80 : 0;
    const uint8_t leftAlt = WI_IsFlagSet(controlKeyState, LEFT_ALT_PRESSED) ? 0x80 : 0;
    const uint8_t rightCtrl = WI_IsFlagSet(controlKeyState, RIGHT_CTRL_PRESSED) ? 0x80 : 0;
    const uint8_t leftCtrl = WI_IsFlagSet(controlKeyState, LEFT_CTRL_PRESSED) ? 0x80 : 0;
    const uint8_t shift = WI_IsFlagSet(controlKeyState, SHIFT_PRESSED) ? 0x80 : 0;
    const uint8_t capsLock = WI_IsFlagSet(controlKeyState, CAPSLOCK_ON) ? 0x01 : 0;

    _keyboardState[VK_SHIFT] = shift;
    _keyboardState[VK_CONTROL] = leftCtrl | rightCtrl;
    _keyboardState[VK_MENU] = leftAlt | rightAlt;
    _keyboardState[VK_CAPITAL] = capsLock;
    _keyboardState[VK_LSHIFT] = shift;
    _keyboardState[VK_LCONTROL] = leftCtrl;
    _keyboardState[VK_RCONTROL] = rightCtrl;
    _keyboardState[VK_LMENU] = leftAlt;
    _keyboardState[VK_RMENU] = rightAlt;
    til::at(_keyboardState, vkey) = 0x80; // Momentarily pretend as if the key is set

    CodepointBuffer cb;
    cb.len = ToUnicodeEx(vkey, 0, &_keyboardState[0], &cb.buf[0], ARRAYSIZE(cb.buf), 0b101, hkl);

    til::at(_keyboardState, vkey) = 0;

    return cb.asSingleCodepoint();
}

void TerminalInput::KeyboardHelper::init() noexcept
{
    if (!_initialized)
    {
        initSlow();
    }
}

void TerminalInput::KeyboardHelper::initSlow() noexcept
{
    _keyboardLayout = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
    memset(&_keyboardState[0], 0, sizeof(_keyboardState));
    _initialized = true;
}

TerminalInput::EncodingHelper::EncodingHelper() noexcept
{
    memset(this, 0, sizeof(*this));
}

bool TerminalInput::EncodingHelper::shiftPressed() const noexcept
{
    return csiModifier & CSI_SHIFT;
}

bool TerminalInput::EncodingHelper::altPressed() const noexcept
{
    return csiModifier & CSI_ALT;
}

bool TerminalInput::EncodingHelper::ctrlPressed() const noexcept
{
    return csiModifier & CSI_CTRL;
}
