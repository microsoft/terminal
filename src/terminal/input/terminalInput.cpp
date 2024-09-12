// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "terminalInput.hpp"

#include <til/unicode.h>

#include "../../inc/unicode.hpp"
#include "../../interactivity/inc/VtApiRedirection.hpp"
#include "../types/inc/IInputEvent.hpp"

using namespace std::string_literals;
using namespace Microsoft::Console::VirtualTerminal;

namespace
{
    // These modifier constants are added to the virtual key code
    // to produce a lookup value for determining the appropriate
    // VT sequence for a particular modifier + key combination.
    constexpr int VTModifier(const int m) { return m << 8; }
    constexpr auto Unmodified = VTModifier(0);
    constexpr auto Shift = VTModifier(1);
    constexpr auto Alt = VTModifier(2);
    constexpr auto Ctrl = VTModifier(4);
    constexpr auto Enhanced = VTModifier(8);
}

TerminalInput::TerminalInput() noexcept
{
    _initKeyboardMap();
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
    _initKeyboardMap();
}

void TerminalInput::ForceDisableWin32InputMode(const bool win32InputMode) noexcept
{
    _forceDisableWin32InputMode = win32InputMode;
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
    // Only do this if win32-input-mode support isn't manually disabled.
    if (_inputMode.test(Mode::Win32) && !_forceDisableWin32InputMode)
    {
        return _makeWin32Output(keyEvent);
    }

    const auto controlKeyState = _trackControlKeyState(keyEvent);
    const auto virtualKeyCode = keyEvent.wVirtualKeyCode;
    auto unicodeChar = keyEvent.uChar.UnicodeChar;

    // Check if this key matches the last recorded key code.
    const auto matchingLastKeyPress = _lastVirtualKeyCode == virtualKeyCode;

    // Only need to handle key down. See raw key handler (see RawReadWaitRoutine in stream.cpp)
    if (!keyEvent.bKeyDown)
    {
        // If this is a release of the last recorded key press, we can reset that.
        if (matchingLastKeyPress)
        {
            _lastVirtualKeyCode = std::nullopt;
        }
        // If NumLock is on, and this is an Alt release with a unicode char,
        // it must be the generated character from an Alt-Numpad composition.
        if (WI_IsFlagSet(controlKeyState, NUMLOCK_ON) && virtualKeyCode == VK_MENU && unicodeChar != 0)
        {
            return MakeOutput({ &unicodeChar, 1 });
        }
        // Otherwise we should return an empty string here to prevent unwanted
        // characters being transmitted by the release event.
        return _makeNoOutput();
    }

    // Unpaired surrogates are no good -> early return.
    if (til::is_leading_surrogate(unicodeChar))
    {
        _leadingSurrogate = unicodeChar;
        return _makeNoOutput();
    }
    // Using a scope_exit ensures that a previous leading surrogate is forgotten
    // even if the KEY_EVENT that followed didn't end up calling _makeCharOutput.
    const auto leadingSurrogateReset = wil::scope_exit([&]() {
        _leadingSurrogate = 0;
    });

    // If this is a VK_PACKET or 0 virtual key, it's likely a synthesized
    // keyboard event, so the UnicodeChar is transmitted as is. This must be
    // handled before the Auto Repeat test, other we'll end up dropping chars.
    if (virtualKeyCode == VK_PACKET || virtualKeyCode == 0)
    {
        return _makeCharOutput(unicodeChar);
    }

    // If this is a repeat of the last recorded key press, and Auto Repeat Mode
    // is disabled, then we should suppress this event.
    if (matchingLastKeyPress && !_inputMode.test(Mode::AutoRepeat))
    {
        // Note that we must return an empty string here to imply that we've handled
        // the event, otherwise the key press can still end up being submitted.
        return _makeNoOutput();
    }
    _lastVirtualKeyCode = virtualKeyCode;

    // If this is a modifier, it won't produce output, so we can return early.
    if (virtualKeyCode >= VK_SHIFT && virtualKeyCode <= VK_MENU)
    {
        return _makeNoOutput();
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
        const auto timeBetweenCtrlAlt = _lastRightAltTime > _lastLeftCtrlTime ?
                                            _lastRightAltTime - _lastLeftCtrlTime :
                                            _lastLeftCtrlTime - _lastRightAltTime;
        leftCtrlIsReallyPressed = timeBetweenCtrlAlt > 50;
    }

    const auto ctrlIsPressed = WI_IsAnyFlagSet(controlKeyState, CTRL_PRESSED);
    const auto ctrlIsReallyPressed = leftCtrlIsReallyPressed || WI_IsFlagSet(controlKeyState, RIGHT_CTRL_PRESSED);
    const auto shiftIsPressed = WI_IsFlagSet(controlKeyState, SHIFT_PRESSED);
    const auto altIsPressed = WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED);
    const auto altGrIsPressed = altIsPressed && ctrlIsPressed;

    // If it's a numeric keypad key, and Alt is pressed (but not Ctrl), then
    // this is an Alt-Numpad composition and we should ignore these keys. The
    // generated character will be transmitted when the Alt is released.
    if (virtualKeyCode >= VK_NUMPAD0 && virtualKeyCode <= VK_NUMPAD9 && altIsPressed && !ctrlIsPressed)
    {
        return _makeNoOutput();
    }

    // The only enhanced key we care about is the Return key, because that
    // indicates that it's the key on the numeric keypad, which will transmit
    // different escape sequences when the Keypad mode is enabled.
    const auto enhancedReturnKey = WI_IsFlagSet(controlKeyState, ENHANCED_KEY) && virtualKeyCode == VK_RETURN;

    // Using the control key state that we calculated above, combined with the
    // virtual key code, we've got a unique identifier for the key combination
    // that we can lookup in our map of predefined key sequences.
    auto keyCombo = virtualKeyCode;
    WI_SetFlagIf(keyCombo, Ctrl, ctrlIsReallyPressed);
    WI_SetFlagIf(keyCombo, Alt, altIsPressed);
    WI_SetFlagIf(keyCombo, Shift, shiftIsPressed);
    WI_SetFlagIf(keyCombo, Enhanced, enhancedReturnKey);
    const auto keyMatch = _keyMap.find(keyCombo);
    if (keyMatch != _keyMap.end())
    {
        return keyMatch->second;
    }

    // If it's not in the key map, we'll use the UnicodeChar, if provided,
    // except in the case of Ctrl+Space, which is often mapped incorrectly as
    // a space character when it's expected to be mapped to NUL. We need to
    // let that fall through to the standard mapping algorithm below.
    const auto ctrlSpaceKey = ctrlIsReallyPressed && virtualKeyCode == VK_SPACE;
    if (unicodeChar != 0 && !ctrlSpaceKey)
    {
        // In the case of an AltGr key, we may still need to apply a Ctrl
        // modifier to the char, either because both Ctrl keys were pressed,
        // or we got a LeftCtrl that was distinctly separate from the RightAlt.
        const auto bothCtrlsArePressed = WI_AreAllFlagsSet(controlKeyState, CTRL_PRESSED);
        const auto rightAltIsPressed = WI_IsFlagSet(controlKeyState, RIGHT_ALT_PRESSED);
        if (altGrIsPressed && (bothCtrlsArePressed || (rightAltIsPressed && leftCtrlIsReallyPressed)))
        {
            unicodeChar = _makeCtrlChar(unicodeChar);
        }
        auto charSequence = _makeCharOutput(unicodeChar);
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

    // We need the current keyboard layout and state to lookup the character
    // that would be transmitted in that state (via the ToUnicodeEx API).
    const auto hkl = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), nullptr));
    auto keyState = _getKeyboardState(virtualKeyCode, controlKeyState);
    const auto flags = 4u; // Don't modify the state in the ToUnicodeEx call.
    const auto bufferSize = 16;
    auto buffer = std::array<wchar_t, bufferSize>{};

    // However, we first need to query the key with the original state, to check
    // whether it's a dead key. If that is the case, ToUnicodeEx should return a
    // negative number, although in practice it's more likely to return a string
    // of length two, with two identical characters. This is because the system
    // sees this as a second press of the dead key, which would typically result
    // in the combining character representation being transmit twice.
    auto length = ToUnicodeEx(virtualKeyCode, 0, keyState.data(), buffer.data(), bufferSize, flags, hkl);
    if (length < 0 || (length == 2 && buffer.at(0) == buffer.at(1)))
    {
        return _makeNoOutput();
    }

    // Once we know it's not a dead key, we run the query again, but with the
    // Ctrl and Alt modifiers disabled to obtain the base character mapping.
    keyState.at(VK_CONTROL) = keyState.at(VK_LCONTROL) = keyState.at(VK_RCONTROL) = 0;
    keyState.at(VK_MENU) = keyState.at(VK_LMENU) = keyState.at(VK_RMENU) = 0;
    length = ToUnicodeEx(virtualKeyCode, 0, keyState.data(), buffer.data(), bufferSize, flags, hkl);
    if (length <= 0)
    {
        // If we've got nothing usable, we'll just return an empty string. The event
        // has technically still been handled, even if it's an unmapped key.
        return _makeNoOutput();
    }

    auto charSequence = StringType{ buffer.data(), gsl::narrow_cast<size_t>(length) };
    // Once we've got the base character, we can apply the Ctrl modifier.
    if (ctrlIsReallyPressed && charSequence.length() == 1)
    {
        auto ch = _makeCtrlChar(charSequence.at(0));
        // If we haven't found a Ctrl mapping for the key, and it's one of
        // the alphanumeric keys, we try again using the virtual key code.
        // On keyboard layouts where the alphanumeric keys are not mapped to
        // their typical ASCII values, this provides a simple fallback.
        if (ch >= L' ' && virtualKeyCode >= '2' && virtualKeyCode <= 'Z')
        {
            ch = _makeCtrlChar(virtualKeyCode);
        }
        charSequence.at(0) = ch;
    }
    // If Alt is pressed, that also needs to be applied to the sequence.
    _escapeOutput(charSequence, altIsPressed);
    return charSequence;
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
    auto defineKeyWithUnusedModifiers = [this](const int keyCode, const std::wstring& sequence) {
        for (auto m = 0; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = sequence;
    };
    auto defineKeyWithAltModifier = [this](const int keyCode, const std::wstring& sequence) {
        _keyMap[keyCode] = sequence;
        _keyMap[Alt + keyCode] = L"\x1B" + sequence;
    };
    auto defineKeypadKey = [this](const int keyCode, const wchar_t* prefix, const wchar_t finalChar) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}"), prefix, finalChar);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}1;{}{}"), _csi, m + 1, finalChar);
    };
    auto defineEditingKey = [this](const int keyCode, const int parm) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}~"), _csi, parm);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}{};{}~"), _csi, parm, m + 1);
    };
    auto defineNumericKey = [this](const int keyCode, const wchar_t finalChar) {
        _keyMap[keyCode] = fmt::format(FMT_COMPILE(L"{}{}"), _ss3, finalChar);
        for (auto m = 1; m < 8; m++)
            _keyMap[VTModifier(m) + keyCode] = fmt::format(FMT_COMPILE(L"{}{}{}"), _ss3, m + 1, finalChar);
    };

    _keyMap.clear();

    // The CSI and SS3 introducers are C1 control codes, which can either be
    // sent as a single codepoint, or as a two character escape sequence.
    if (_inputMode.test(Mode::SendC1))
    {
        _csi = L"\x9B";
        _ss3 = L"\x8F";
    }
    else
    {
        _csi = L"\x1B[";
        _ss3 = L"\x1BO";
    }

    // PAUSE doesn't have a VT mapping, but traditionally we've mapped it to ^Z,
    // regardless of modifiers.
    defineKeyWithUnusedModifiers(VK_PAUSE, L"\x1A"s);

    // BACKSPACE maps to either DEL or BS, depending on the Backarrow Key mode.
    // The Ctrl modifier inverts the active mode, swapping BS and DEL (this is
    // not standard, but a modern terminal convention). The Alt modifier adds
    // an ESC prefix (also not standard).
    const auto backSequence = _inputMode.test(Mode::BackarrowKey) ? L"\b"s : L"\x7F"s;
    const auto ctrlBackSequence = _inputMode.test(Mode::BackarrowKey) ? L"\x7F"s : L"\b"s;
    defineKeyWithAltModifier(VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + VK_BACK, ctrlBackSequence);
    defineKeyWithAltModifier(Shift + VK_BACK, backSequence);
    defineKeyWithAltModifier(Ctrl + Shift + VK_BACK, ctrlBackSequence);

    // TAB maps to HT, and Shift+TAB to CBT. The Ctrl modifier has no effect.
    // The Alt modifier adds an ESC prefix, although in practice all the Alt
    // mappings are likely to be system hotkeys.
    const auto shiftTabSequence = fmt::format(FMT_COMPILE(L"{}Z"), _csi);
    defineKeyWithAltModifier(VK_TAB, L"\t"s);
    defineKeyWithAltModifier(Ctrl + VK_TAB, L"\t"s);
    defineKeyWithAltModifier(Shift + VK_TAB, shiftTabSequence);
    defineKeyWithAltModifier(Ctrl + Shift + VK_TAB, shiftTabSequence);

    // RETURN maps to either CR or CR LF, depending on the Line Feed mode. With
    // a Ctrl modifier it maps to LF, because that's the expected behavior for
    // most PC keyboard layouts. The Alt modifier adds an ESC prefix.
    const auto returnSequence = _inputMode.test(Mode::LineFeed) ? L"\r\n"s : L"\r"s;
    defineKeyWithAltModifier(VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Shift + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Ctrl + VK_RETURN, L"\n"s);
    defineKeyWithAltModifier(Ctrl + Shift + VK_RETURN, L"\n"s);

    // The keypad RETURN key works the same way, except when Keypad mode is
    // enabled, but that's handled below with the other keypad keys.
    defineKeyWithAltModifier(Enhanced + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Shift + Enhanced + VK_RETURN, returnSequence);
    defineKeyWithAltModifier(Ctrl + Enhanced + VK_RETURN, L"\n"s);
    defineKeyWithAltModifier(Ctrl + Shift + Enhanced + VK_RETURN, L"\n"s);

    if (_inputMode.test(Mode::Ansi))
    {
        // F1 to F4 map to the VT keypad function keys, which are SS3 sequences.
        // When combined with a modifier, we use CSI sequences with the modifier
        // embedded as a parameter (not standard - a modern terminal extension).
        defineKeypadKey(VK_F1, _ss3, L'P');
        defineKeypadKey(VK_F2, _ss3, L'Q');
        defineKeypadKey(VK_F3, _ss3, L'R');
        defineKeypadKey(VK_F4, _ss3, L'S');

        // F5 through F20 map to the top row VT function keys. They use standard
        // DECFNK sequences with the modifier embedded as a parameter. The first
        // five function keys on a VT terminal are typically local functions, so
        // there's not much need to support mappings for them.
        for (auto vk = VK_F5; vk <= VK_F20; vk++)
        {
            static constexpr std::array<uint8_t, 16> parameters = { 15, 17, 18, 19, 20, 21, 23, 24, 25, 26, 28, 29, 31, 32, 33, 34 };
            const auto parm = parameters.at(static_cast<size_t>(vk) - VK_F5);
            defineEditingKey(vk, parm);
        }

        // Cursor keys follow a similar pattern to the VT keypad function keys,
        // although they only use an SS3 prefix when the Cursor Key mode is set.
        // When combined with a modifier, they'll use CSI sequences with the
        // modifier embedded as a parameter (again not standard).
        const auto ckIntroducer = _inputMode.test(Mode::CursorKey) ? _ss3 : _csi;
        defineKeypadKey(VK_UP, ckIntroducer, L'A');
        defineKeypadKey(VK_DOWN, ckIntroducer, L'B');
        defineKeypadKey(VK_RIGHT, ckIntroducer, L'C');
        defineKeypadKey(VK_LEFT, ckIntroducer, L'D');
        defineKeypadKey(VK_CLEAR, ckIntroducer, L'E');
        defineKeypadKey(VK_HOME, ckIntroducer, L'H');
        defineKeypadKey(VK_END, ckIntroducer, L'F');

        // Editing keys follow the same pattern as the top row VT function
        // keys, using standard DECFNK sequences with the modifier embedded.
        defineEditingKey(VK_INSERT, 2);
        defineEditingKey(VK_DELETE, 3);
        defineEditingKey(VK_PRIOR, 5);
        defineEditingKey(VK_NEXT, 6);

        // Keypad keys depend on the Keypad mode. When reset, they transmit
        // the ASCII character assigned by the keyboard layout, but when set
        // they transmit SS3 escape sequences. When used with a modifier, the
        // modifier is embedded as a parameter value (not standard).
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            defineNumericKey(VK_MULTIPLY, L'j');
            defineNumericKey(VK_ADD, L'k');
            defineNumericKey(VK_SEPARATOR, L'l');
            defineNumericKey(VK_SUBTRACT, L'm');
            defineNumericKey(VK_DECIMAL, L'n');
            defineNumericKey(VK_DIVIDE, L'o');

            defineNumericKey(VK_NUMPAD0, L'p');
            defineNumericKey(VK_NUMPAD1, L'q');
            defineNumericKey(VK_NUMPAD2, L'r');
            defineNumericKey(VK_NUMPAD3, L's');
            defineNumericKey(VK_NUMPAD4, L't');
            defineNumericKey(VK_NUMPAD5, L'u');
            defineNumericKey(VK_NUMPAD6, L'v');
            defineNumericKey(VK_NUMPAD7, L'w');
            defineNumericKey(VK_NUMPAD8, L'x');
            defineNumericKey(VK_NUMPAD9, L'y');

            defineNumericKey(Enhanced + VK_RETURN, L'M');
        }
    }
    else
    {
        // In VT52 mode, the sequences tend to use the same final character as
        // their ANSI counterparts, but with a simple ESC prefix. The modifier
        // keys have no effect.

        // VT52 only support PF1 through PF4 function keys.
        defineKeyWithUnusedModifiers(VK_F1, L"\033P"s);
        defineKeyWithUnusedModifiers(VK_F2, L"\033Q"s);
        defineKeyWithUnusedModifiers(VK_F3, L"\033R"s);
        defineKeyWithUnusedModifiers(VK_F4, L"\033S"s);

        // But terminals with application functions keys would
        // map some of them as controls keys in VT52 mode.
        defineKeyWithUnusedModifiers(VK_F11, L"\033"s);
        defineKeyWithUnusedModifiers(VK_F12, L"\b"s);
        defineKeyWithUnusedModifiers(VK_F13, L"\n"s);

        // Cursor keys use the same finals as the ANSI sequences.
        defineKeyWithUnusedModifiers(VK_UP, L"\033A"s);
        defineKeyWithUnusedModifiers(VK_DOWN, L"\033B"s);
        defineKeyWithUnusedModifiers(VK_RIGHT, L"\033C"s);
        defineKeyWithUnusedModifiers(VK_LEFT, L"\033D"s);
        defineKeyWithUnusedModifiers(VK_CLEAR, L"\033E"s);
        defineKeyWithUnusedModifiers(VK_HOME, L"\033H"s);
        defineKeyWithUnusedModifiers(VK_END, L"\033F"s);

        // Keypad keys also depend on Keypad mode, the same as ANSI mappings,
        // but the sequences use an ESC ? prefix instead of SS3.
        if (Feature_KeypadModeEnabled::IsEnabled() && _inputMode.test(Mode::Keypad))
        {
            defineKeyWithUnusedModifiers(VK_MULTIPLY, L"\033?j"s);
            defineKeyWithUnusedModifiers(VK_ADD, L"\033?k"s);
            defineKeyWithUnusedModifiers(VK_SEPARATOR, L"\033?l"s);
            defineKeyWithUnusedModifiers(VK_SUBTRACT, L"\033?m"s);
            defineKeyWithUnusedModifiers(VK_DECIMAL, L"\033?n"s);
            defineKeyWithUnusedModifiers(VK_DIVIDE, L"\033?o"s);

            defineKeyWithUnusedModifiers(VK_NUMPAD0, L"\033?p"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD1, L"\033?q"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD2, L"\033?r"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD3, L"\033?s"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD4, L"\033?t"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD5, L"\033?u"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD6, L"\033?v"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD7, L"\033?w"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD8, L"\033?x"s);
            defineKeyWithUnusedModifiers(VK_NUMPAD9, L"\033?y"s);

            defineKeyWithUnusedModifiers(Enhanced + VK_RETURN, L"\033?M"s);
        }
    }

    _focusInSequence = _csi + L"I"s;
    _focusOutSequence = _csi + L"O"s;
}
CATCH_LOG()

DWORD TerminalInput::_trackControlKeyState(const KEY_EVENT_RECORD& key)
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
std::array<byte, 256> TerminalInput::_getKeyboardState(const WORD virtualKeyCode, const DWORD controlKeyState) const
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
TerminalInput::StringType TerminalInput::_makeCharOutput(const wchar_t ch)
{
    StringType str;

    if (_leadingSurrogate && til::is_trailing_surrogate(ch))
    {
        str.push_back(_leadingSurrogate);
    }

    str.push_back(ch);
    return str;
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
