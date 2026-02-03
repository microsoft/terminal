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

static constexpr uint8_t KittyKeyCodeLegacySentinel = 1;

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

    const auto functionalKeyCode = _getKittyFunctionalKeyCode(keyEvent.wVirtualKeyCode, keyEvent.wVirtualScanCode, simpleKeyState);
    EncodingHelper enc;

    // However, we first need to query the key with the original state, to check
    // whether it's a dead key. If that is the case, ToUnicodeEx should return a
    // negative number, although in practice it's more likely to return a string
    // of length two, with two identical characters. This is because the system
    // sees this as a second press of the dead key, which would typically result
    // in the combining character representation being transmitted twice.
    //
    // _getKittyFunctionalKeyCode() only returns non-zero for non-text keys.
    // So, using !functionalKeyCode we filter down to (possible) text keys.
    // We can also further filter down to only key-combinations with modifier,
    // since no modifiers means that we can just use the codepoint as is.
    if (!functionalKeyCode && (simpleKeyState & (SKS_CTRL | SKS_ALT)) != 0)
    {
        const auto hkl = enc.getKeyboardLayoutCached();
        const auto cb = enc.getKeyboardKey(virtualKeyCode, controlKeyState, hkl);
        if (cb.len < 0 || (cb.len == 2 && cb.buf[0] == cb.buf[1]))
        {
            return _makeNoOutput();
        }
    }

    uint32_t kittyKeyCode = 0;
    uint32_t kittyEventType = 0;
    uint32_t kittyAltKeyCodeShifted = 0;
    uint32_t kittyAltKeyCodeBase = 0;
    uint32_t kittyTextAsCodepoint = 0;

    if (_kittyFlags)
    {
        if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::DisambiguateEscapeCodes))
        {
            // KKP> Turning on [DisambiguateEscapeCodes] will cause the terminal to
            // KKP> report the Esc, alt+key, ctrl+key, ctrl+alt+key, shift+alt+key
            // KKP> keys using CSI u sequences instead of legacy ones.
            // KKP> Here key is any ASCII key as described in Legacy text keys. [...]
            // KKP> Additionally, all non text keypad keys will be reported [...] with CSI u encoding, [...].
            //
            // NOTE: The specification fails to mention ctrl+shift+key, but Kitty does handle it.
            // So really, it's actually "any modifiers, except for shift+key".
            //
            // NOTE: The list of legacy keys doesn't really make any sense.
            // My interpretation is that the author meant all printable keys,
            // because as before, that's also how Kitty handles it.
            //
            // NOTE: We'll handle modifier+key combinations below, together with ReportAllKeysAsEscapeCodes.

            if (virtualKeyCode == VK_ESCAPE || (virtualKeyCode >= VK_NUMPAD0 && virtualKeyCode <= VK_DIVIDE))
            {
                kittyKeyCode = functionalKeyCode <= KittyKeyCodeLegacySentinel ? 0 : functionalKeyCode;
            }
        }

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

        if (
            // KKP> This [...] turns on key reporting even for key events that generate text.
            // KKP> [...] text will not be sent, instead only key events are sent.
            // KKP> [...] with this mode, events for pressing modifier keys are reported.
            //
            // In other words: Get the functional key code if any, otherwise use the codepoint.
            WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes) ||
            // A continuation of DisambiguateEscapeCodes above: modifier + text-key = CSI u.
            (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::DisambiguateEscapeCodes) &&
             functionalKeyCode <= KittyKeyCodeLegacySentinel && // Implies that it's a possibly a text-key
             (simpleKeyState & ~SKS_ENHANCED) > SKS_SHIFT) ||
            // Enabling ReportEventTypes implies that `CSI u` is used for all key up events.
            kittyEventType == 3)
        {
            kittyKeyCode = functionalKeyCode <= KittyKeyCodeLegacySentinel ? 0 : functionalKeyCode;

            if (kittyKeyCode == 0)
            {
                // KKP> Note that the codepoint used is always the lower-case [...] version of the key.
                //
                // In other words, we want the "base key" of the current layout,
                // effectively the key without Ctrl/Alt/Shift modifiers.
                const auto hkl = enc.getKeyboardLayoutCached();
                const auto cb = enc.getKeyboardKey(virtualKeyCode, controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | SHIFT_PRESSED | CAPSLOCK_ON), hkl);
                const auto cp = _bufferToCodepoint(cb.buf);

                if (_codepointIsText(cp))
                {
                    kittyKeyCode = cp;
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
                if (_codepointIsText(codepoint))
                {
                    kittyTextAsCodepoint = codepoint;
                }
            }
        }

        if (WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAlternateKeys))
        {
            // KKP> This [...] causes the terminal to report alternate key values [...]
            //
            // KKP> Note that [...] only key events represented as escape codes due to the
            // KKP> other enhancements in effect will be affected by this enhancement. [...]

            if (kittyKeyCode)
            {
                // KKP> [...] the terminal can [...] [additionally send] the shifted key and base layout key [...].
                // KKP> The shifted key is [...] the upper-case version of unicode-codepoint,
                // KKP> or more technically, the shifted version, in the currently active keyboard layout.
                //
                // NOTE that the 2nd line is wrong. On a US layout, Shift+3 is not an "uppercase 3",
                // it's "#", which is exactly what kitty sends as the "shifted key" parameter.
                // (This is in addition to "unicode-codepoint" never being defined throughout the spec...)

                // KKP> Note that the shifted key must be present only if shift is also present in the modifiers.

                if ((simpleKeyState & SKS_SHIFT) != 0)
                {
                    // This is almost identical to our computation of the "base key" for
                    // ReportAllKeysAsEscapeCodes above, but this time with SHIFT_PRESSED.
                    const auto hkl = enc.getKeyboardLayoutCached();
                    const auto cb = enc.getKeyboardKey(virtualKeyCode, controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | CAPSLOCK_ON) | SHIFT_PRESSED, hkl);
                    const auto cp = _bufferToCodepoint(cb.buf);

                    // The specification doesn't state whether the shifted key should be reported if it's identical
                    // to the unshifted version, so I don't (this matches the behavior for base layout key below).
                    if (_codepointIsText(cp) && cp != kittyKeyCode)
                    {
                        kittyAltKeyCodeShifted = cp;
                    }
                }

                if (keyEvent.wVirtualScanCode)
                {
                    // > The base layout key is the key corresponding to the physical key in the standard PC-101 key layout.
                    static const auto usLayout = LoadKeyboardLayoutW(L"00000409", 0);
                    if (usLayout)
                    {
                        const auto vkey = MapVirtualKeyExW(keyEvent.wVirtualScanCode, MAPVK_VSC_TO_VK_EX, usLayout);
                        if (vkey)
                        {
                            kittyAltKeyCodeBase = _getKittyFunctionalKeyCode(vkey, keyEvent.wVirtualScanCode, simpleKeyState);

                            if (kittyAltKeyCodeBase <= KittyKeyCodeLegacySentinel)
                            {
                                const auto cb = enc.getKeyboardKey(virtualKeyCode, controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED | SHIFT_PRESSED | CAPSLOCK_ON), usLayout);
                                kittyAltKeyCodeBase = _bufferToCodepoint(cb.buf);
                                if (!_codepointIsText(kittyAltKeyCodeBase))
                                {
                                    kittyAltKeyCodeShifted = 0;
                                }
                            }

                            if (kittyAltKeyCodeBase == kittyKeyCode)
                            {
                                kittyAltKeyCodeBase = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    if (kittyKeyCode)
    {
        enc.csiFinal = L'u';
        enc.csiParam[0][0] = kittyKeyCode;
        enc.csiParam[0][1] = kittyAltKeyCodeShifted;
        enc.csiParam[0][2] = kittyAltKeyCodeBase;
        // enc.csiParam[1][0] contains the CSI modifier value, which gets calculated below.
        enc.csiParam[1][1] = kittyEventType;
        enc.csiParam[2][0] = kittyTextAsCodepoint;
    }
    else
    {
        _fillRegularKeyEncodingInfo(enc, keyEvent, simpleKeyState);
    }

    std::wstring seq;

    if (enc.csiFinal)
    {
        {
            // As per KKP: shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
            enc.csiParam[1][0] = simpleKeyState & (SKS_SHIFT | SKS_ALT | SKS_CTRL);

            // KKP> Lock modifiers are not reported for text producing keys, [...].
            // KKP> To get lock modifiers for all keys use the Report all keys as escape codes enhancement.
            if ((_kittyFlags != 0 && !_codepointIsText(codepoint)) ||
                WI_IsFlagSet(_kittyFlags, KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes))
            {
                if (WI_IsFlagSet(controlKeyState, CAPSLOCK_ON))
                {
                    enc.csiParam[1][0] |= 64;
                }
                if (WI_IsFlagSet(controlKeyState, NUMLOCK_ON))
                {
                    enc.csiParam[1][0] |= 128;
                }
            }

            if (enc.csiParam[1][0] != 0)
            {
                // NOTE: The CSI modifier value is 1 based.
                enc.csiParam[1][0] += 1;
            }
        }

        constexpr size_t maxParamCount = std::size(enc.csiParam);
        constexpr size_t maxSubParamCount = std::size(enc.csiParam[0]);

        // If any sub-parameter of a parameter is set,
        // the first sub-parameter must be at least 1.
        for (size_t p = 0; p < maxParamCount; p++)
        {
            uint32_t bits = 0;
            for (size_t s = 1; s < maxSubParamCount; s++)
            {
                bits |= enc.csiParam[p][s];
            }
            if (bits != 0)
            {
                auto& dst = enc.csiParam[p][0];
                dst = std::max<uint32_t>(dst, 1);
            }
        }

        size_t paramMaxIdx = 0;
        for (size_t p = maxParamCount - 1; p != 0; p--)
        {
            if (enc.csiParam[p][0] != 0)
            {
                paramMaxIdx = p;
                break;
            }
        }

        seq.append(_csi);

        // Format the parameters, skipping any trailing empty parameters entirely.
        // Empty sub-parameters are omitted, while the colons are kept.
        for (size_t p = 0; p <= paramMaxIdx; p++)
        {
            if (p > 0)
            {
                seq.push_back(L';');
            }

            // Find the last non-zero sub-parameter in this parameter.
            size_t subMaxIdx = 0;
            for (size_t s = maxSubParamCount - 1; s != 0; s--)
            {
                if (enc.csiParam[p][s] != 0)
                {
                    subMaxIdx = s;
                    break;
                }
            }

            for (size_t s = 0; s <= subMaxIdx; s++)
            {
                if (s > 0)
                {
                    seq.push_back(L':');
                }
                if (const auto v = enc.csiParam[p][s])
                {
                    fmt::format_to(std::back_inserter(seq), FMT_COMPILE(L"{}"), v);
                }
            }
        }

        seq.push_back(enc.csiFinal);
    }
    else if (enc.ss3Final)
    {
        seq.append(L"\033O");
        seq.push_back(enc.ss3Final);
    }
    else if (!enc.plain.empty())
    {
        if (enc.plainAltPrefix && (simpleKeyState & SKS_ALT) && _inputMode.test(Mode::Ansi))
        {
            seq.push_back(L'\x1b');
        }
        seq.append(enc.plain);
    }
    // If this is a modifier, it won't produce output.
    else if (virtualKeyCode < VK_SHIFT || virtualKeyCode > VK_MENU)
    {
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
            auto cp = codepoint;

            if (altGrIsPressed && (bothCtrlsArePressed || (rightAltIsPressed && leftCtrlIsReallyPressed)))
            {
                cp = _makeCtrlChar(cp);
            }

            // We may also need to apply an Alt prefix to the char sequence, but
            // if this is an AltGr key, we only do so if both Alts are pressed.
            const auto bothAltsArePressed = WI_AreAllFlagsSet(controlKeyState, ALT_PRESSED);

            if ((altGrIsPressed ? bothAltsArePressed : altIsPressed) && _inputMode.test(Mode::Ansi))
            {
                seq.push_back(L'\x1b');
            }

            _stringPushCodepoint(seq, cp);
        }
        // If we don't have a UnicodeChar, we'll try and determine what the key
        // would have transmitted without any Ctrl or Alt modifiers applied. But
        // this only makes sense if there were actually modifiers pressed.
        else if (altIsPressed || ctrlIsPressed)
        {
            // If Alt is pressed, that also needs to be applied to the sequence.
            if (altIsPressed && _inputMode.test(Mode::Ansi))
            {
                seq.push_back(L'\x1b');
            }

            const auto hkl = enc.getKeyboardLayoutCached();
            const auto cb = enc.getKeyboardKey(virtualKeyCode, controlKeyState & ~(ALT_PRESSED | CTRL_PRESSED), hkl);
            auto cp = _bufferToCodepoint(cb.buf);

            // Once we've got the base character, we can apply the Ctrl modifier.
            if (ctrlIsReallyPressed)
            {
                cp = _makeCtrlChar(cp);
                // If we haven't found a Ctrl mapping for the key, and it's one of
                // the alphanumeric keys, we try again using the virtual key code.
                // On keyboard layouts where the alphanumeric keys are not mapped to
                // their typical ASCII values, this provides a simple fallback.
                if (cp >= L' ' && virtualKeyCode >= '2' && virtualKeyCode <= 'Z')
                {
                    cp = _makeCtrlChar(virtualKeyCode);
                }
            }

            _stringPushCodepoint(seq, cp);
        }
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
try
{
    _focusInSequence = fmt::format(FMT_COMPILE(L"{}I"), _csi);
    _focusOutSequence = fmt::format(FMT_COMPILE(L"{}O"), _csi);
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
std::array<byte, 256> TerminalInput::_getKeyboardState(const size_t virtualKeyCode, const DWORD controlKeyState)
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
    const auto buf = _codepointToBuffer(cp);
    return { buf.buf, gsl::narrow_cast<size_t>(buf.len) };
}

TerminalInput::StringType TerminalInput::_makeNoOutput() noexcept
{
    return {};
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

void TerminalInput::_fillRegularKeyEncodingInfo(EncodingHelper& enc, const KEY_EVENT_RECORD& key, DWORD simpleKeyState) const noexcept
{
    const auto virtualKeyCode = key.wVirtualKeyCode;
    const auto modified = (simpleKeyState & ~SKS_ENHANCED) != 0;
    const auto enhanced = (simpleKeyState & SKS_ENHANCED) != 0;
    const auto kitty = WI_IsAnyFlagSet(
        _kittyFlags,
        KittyKeyboardProtocolFlags::DisambiguateEscapeCodes |
            KittyKeyboardProtocolFlags::ReportEventTypes |
            KittyKeyboardProtocolFlags::ReportAllKeysAsEscapeCodes);

    switch (virtualKeyCode)
    {
    // BACKSPACE maps to either DEL or BS, depending on the Backarrow Key mode.
    // The Ctrl modifier inverts the active mode, swapping BS and DEL (this is
    // not standard, but a modern terminal convention). The Alt modifier adds
    // an ESC prefix (also not standard).
    case VK_BACK:
        enc.plainAltPrefix = true;
        switch (simpleKeyState & ~(SKS_ALT | SKS_SHIFT))
        {
        default:
            enc.plain = _inputMode.test(Mode::BackarrowKey) ? L"\b"sv : L"\x7F"sv;
            break;
        case SKS_CTRL:
            enc.plain = _inputMode.test(Mode::BackarrowKey) ? L"\x7f"sv : L"\b"sv;
            break;
        }
        break;
    // TAB maps to HT, and Shift+TAB to CBT. The Ctrl modifier has no effect.
    // The Alt modifier adds an ESC prefix, although in practice all the Alt
    // mappings are likely to be system hotkeys.
    case VK_TAB:
        enc.plainAltPrefix = true;
        switch (simpleKeyState & ~(SKS_ALT | SKS_CTRL))
        {
        default:
            enc.plain = L"\t"sv;
            break;
        case SKS_SHIFT:
            enc.csiFinal = L'Z';
            break;
        }
        break;
    // RETURN maps to either CR or CR LF, depending on the Line Feed mode. With
    // a Ctrl modifier it maps to LF, because that's the expected behavior for
    // most PC keyboard layouts. The Alt modifier adds an ESC prefix.
    case VK_RETURN:
        // The keypad RETURN key works different.
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
            enc.plainAltPrefix = true;
            switch (simpleKeyState & ~(SKS_ALT | SKS_SHIFT | SKS_ENHANCED))
            {
            default:
                enc.plain = _inputMode.test(Mode::LineFeed) ? L"\r\n"sv : L"\r"sv;
                break;
            case SKS_CTRL:
                enc.plain = L"\n"sv;
                break;
            }
        }
        break;

    // PAUSE doesn't have a VT mapping, but traditionally we've mapped it to ^Z,
    // regardless of modifiers.
    case VK_PAUSE:
        enc.plain = L"\x1A"sv;
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
            if (kitty && virtualKeyCode == VK_F3)
            {
                enc.csiFinal = L'~';
                enc.csiParam[0][0] = 13;
            }
            else
            {
                auto& dst = kitty || modified ? enc.csiFinal : enc.ss3Final;
                dst = L'P' + (virtualKeyCode - VK_F1);
            }
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033P", L"\033Q", L"\033R", L"\033S" };
            enc.plain = lut[virtualKeyCode - VK_F1];
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
        if (_inputMode.test(Mode::Ansi))
        {
            static constexpr uint8_t lut[] = { 15, 17, 18, 19, 20, 21, 23, 24, 25, 26, 28, 29, 31, 32, 33, 34 };
            enc.csiFinal = L'~';
            enc.csiParam[0][0] = lut[virtualKeyCode - VK_F5];
        }
        else
        {
            switch (virtualKeyCode)
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

    // Cursor keys follow a similar pattern to the VT keypad function keys,
    // although they only use an SS3 prefix when the Cursor Key mode is set.
    // When combined with a modifier, they'll use CSI sequences with the
    // modifier embedded as a parameter (again not standard).
    // In VT52 mode, cursor keys use ESC prefix.
    case VK_LEFT: // 0x25 = D
    case VK_UP: // 0x26 = A
    case VK_RIGHT: // 0x27 = C
    case VK_DOWN: // 0x28 = B
        if (_inputMode.test(Mode::Ansi))
        {
            static constexpr uint8_t lut[] = { 'D', 'A', 'C', 'B' };
            const auto csi = kitty || modified || !_inputMode.test(Mode::CursorKey);
            auto& dst = csi ? enc.csiFinal : enc.ss3Final;
            dst = lut[virtualKeyCode - VK_LEFT];
        }
        else
        {
            static constexpr std::wstring_view lut[] = { L"\033D", L"\033A", L"\033C", L"\033B" };
            enc.plain = lut[virtualKeyCode - VK_LEFT];
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

    // Editing keys follow the same pattern as the top row VT function
    // keys, using standard DECFNK sequences with the modifier embedded.
    // These are not defined in VT52 mode.
    case VK_INSERT: // 0x2D = 2
    case VK_DELETE: // 0x2E = 3
        if (_inputMode.test(Mode::Ansi))
        {
            enc.csiFinal = L'~';
            enc.csiParam[0][0] = 2 + (virtualKeyCode - VK_INSERT);
        }
        break;
    case VK_PRIOR: // 0x21 = 5
    case VK_NEXT: // 0x22 = 6
        if (_inputMode.test(Mode::Ansi))
        {
            enc.csiFinal = L'~';
            enc.csiParam[0][0] = 5 + (virtualKeyCode - VK_PRIOR);
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
                enc.ss3Final = L'p' + (virtualKeyCode - VK_NUMPAD0);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?p", L"\033?q", L"\033?r", L"\033?s", L"\033?t", L"\033?u", L"\033?v", L"\033?w", L"\033?x", L"\033?y" };
                enc.plain = lut[virtualKeyCode - VK_NUMPAD0];
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
                enc.ss3Final = L'j' + (virtualKeyCode - VK_MULTIPLY);
            }
            else
            {
                static constexpr std::wstring_view lut[] = { L"\033?j", L"\033?k", L"\033?l", L"\033?m", L"\033?n", L"\033?o" };
                enc.plain = lut[virtualKeyCode - VK_MULTIPLY];
            }
        }
        break;

    default:
        break;
    }
}

uint32_t TerminalInput::_getKittyFunctionalKeyCode(UINT vkey, WORD scanCode, DWORD simpleKeyState) noexcept
{
    // Most KKP functional keys are rather predictable. This LUT helps cover most of them.
    // Some keys however depend on the key state (specifically the enhanced bit).
    static constexpr auto lut = []() {
        std::array<uint8_t, 256> data{};

#define SET(key, value) data[key] = static_cast<uint8_t>(value);

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

    const auto enhanced = (simpleKeyState & SKS_ENHANCED) != 0;

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

    const auto v = lut[vkey & 0xff];
    return v <= KittyKeyCodeLegacySentinel ? v : 0xE000 + v;
}

std::vector<uint8_t>& TerminalInput::_getKittyStack() noexcept
{
    return _inAlternateBuffer ? _kittyAltStack : _kittyMainStack;
}

bool TerminalInput::_codepointIsText(uint32_t cp) noexcept
{
    return cp > 0x1f && (cp < 0x7f || cp > 0x9f) && (cp < 57344 || cp > 63743);
}

void TerminalInput::_stringPushCodepoint(std::wstring& str, uint32_t cp)
{
    const auto cb = _codepointToBuffer(cp);
    str.append(cb.buf, cb.len);
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

uint32_t TerminalInput::_getBaseLayoutCodepoint(const WORD scanCode) noexcept
{
    if (!scanCode)
    {
        return 0;
    }

    // > The base layout key is the key corresponding to the physical key in the standard PC-101 key layout.
    static const auto usLayout = LoadKeyboardLayoutW(L"00000409", 0);
    if (!usLayout)
    {
        return 0;
    }

    const auto vkey = MapVirtualKeyExW(scanCode, MAPVK_VSC_TO_VK, usLayout);
    if (!vkey)
    {
        return 0;
    }

    wchar_t baseChar[4];
    const auto keyState = _getKeyboardState(vkey, 0);
    const auto result = ToUnicodeEx(vkey, scanCode, keyState.data(), baseChar, 4, 4, usLayout);

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
