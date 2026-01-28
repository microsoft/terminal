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

void TerminalInput::UseAlternateScreenBuffer() noexcept
{
    _inAlternateBuffer = true;
}

void TerminalInput::UseMainScreenBuffer() noexcept
{
    _inAlternateBuffer = false;
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

// Kitty keyboard protocol methods

void TerminalInput::SetKittyKeyboardProtocol(const uint8_t flags, const KittyKeyboardProtocolMode mode) noexcept
{
    switch (mode)
    {
    case KittyKeyboardProtocolMode::Replace:
        _kittyFlags = flags & KittyKeyboardProtocolFlags::All;
        break;
    case KittyKeyboardProtocolMode::Set:
        _kittyFlags |= (flags & KittyKeyboardProtocolFlags::All);
        break;
    case KittyKeyboardProtocolMode::Reset:
        _kittyFlags &= ~(flags & KittyKeyboardProtocolFlags::All);
        break;
    }
}

uint8_t TerminalInput::GetKittyFlags() const noexcept
{
    return _kittyFlags;
}

void TerminalInput::PushKittyFlags(const uint8_t flags) noexcept
{
    auto& stack = _getKittyStack();
    // Evict oldest entry if stack is full (DoS prevention)
    if (stack.size() >= KittyStackMaxSize)
    {
        stack.erase(stack.begin());
    }
    stack.push_back(_kittyFlags);
    _kittyFlags = flags & KittyKeyboardProtocolFlags::All;
}

void TerminalInput::PopKittyFlags(const size_t count) noexcept
{
    auto& stack = _getKittyStack();
    const auto popCount = std::min(count, stack.size());
    for (size_t i = 0; i < popCount; ++i)
    {
        if (!stack.empty())
        {
            _kittyFlags = stack.back();
            stack.pop_back();
        }
    }
    // If we've emptied the stack, reset all flags
    if (stack.empty() && popCount > 0)
    {
        _kittyFlags = 0;
    }
}

void TerminalInput::ResetKittyKeyboardProtocols() noexcept
{
    _kittyFlags = 0;
    _kittyMainStack.clear();
    _kittyAltStack.clear();
}

std::vector<uint8_t>& TerminalInput::_getKittyStack() noexcept
{
    return _inAlternateBuffer ? _kittyAltStack : _kittyMainStack;
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

    // If kitty keyboard mode is active, use kitty keyboard protocol.
    // This handles release events when ReportEventTypes flag is set.
    if (_kittyFlags != 0)
    {
        return _makeKittyOutput(keyEvent, controlKeyState);
    }

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
        // the event; otherwise, the key press can still end up being submitted.
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

// Maps Windows virtual key codes to kitty keyboard protocol functional key codes.
// Returns 0 if the key is a text-generating key (should use unicode codepoint instead).
// https://sw.kovidgoyal.net/kitty/keyboard-protocol/#functional-key-definitions
uint32_t TerminalInput::_getKittyFunctionalKeyCode(const WORD virtualKeyCode, const DWORD controlKeyState) noexcept
{
    // Unicode Private Use Area codes for functional keys (57344+)
    constexpr uint32_t CAPS_LOCK = 57358;
    constexpr uint32_t SCROLL_LOCK = 57359;
    constexpr uint32_t NUM_LOCK = 57360;
    constexpr uint32_t PRINT_SCREEN = 57361;
    constexpr uint32_t PAUSE = 57362;
    constexpr uint32_t MENU = 57363;

    // F13-F35
    constexpr uint32_t F13 = 57376;

    // Keypad keys
    constexpr uint32_t KP_0 = 57399;
    constexpr uint32_t KP_DECIMAL = 57409;
    constexpr uint32_t KP_DIVIDE = 57410;
    constexpr uint32_t KP_MULTIPLY = 57411;
    constexpr uint32_t KP_SUBTRACT = 57412;
    constexpr uint32_t KP_ADD = 57413;
    constexpr uint32_t KP_ENTER = 57414;
    // Note: KP_EQUAL (57415) has no standard Windows VK code
    constexpr uint32_t KP_SEPARATOR = 57416;
    constexpr uint32_t KP_LEFT = 57417;
    constexpr uint32_t KP_RIGHT = 57418;
    constexpr uint32_t KP_UP = 57419;
    constexpr uint32_t KP_DOWN = 57420;
    constexpr uint32_t KP_PAGE_UP = 57421;
    constexpr uint32_t KP_PAGE_DOWN = 57422;
    constexpr uint32_t KP_HOME = 57423;
    constexpr uint32_t KP_END = 57424;
    constexpr uint32_t KP_INSERT = 57425;
    constexpr uint32_t KP_DELETE = 57426;

    // Media keys
    // Note: Windows only has VK_MEDIA_PLAY_PAUSE, not separate play/pause keys
    // MEDIA_FAST_FORWARD (57433) and MEDIA_REWIND (57434) have no standard Windows VK codes
    constexpr uint32_t MEDIA_PLAY_PAUSE = 57430;
    constexpr uint32_t MEDIA_STOP = 57432;
    constexpr uint32_t MEDIA_TRACK_NEXT = 57435;
    constexpr uint32_t MEDIA_TRACK_PREVIOUS = 57436;
    constexpr uint32_t LOWER_VOLUME = 57438;
    constexpr uint32_t RAISE_VOLUME = 57439;
    constexpr uint32_t MUTE_VOLUME = 57440;

    // Modifier keys
    constexpr uint32_t LEFT_SHIFT = 57441;
    constexpr uint32_t LEFT_CONTROL = 57442;
    constexpr uint32_t LEFT_ALT = 57443;
    constexpr uint32_t LEFT_SUPER = 57444;
    constexpr uint32_t RIGHT_SHIFT = 57447;
    constexpr uint32_t RIGHT_CONTROL = 57448;
    constexpr uint32_t RIGHT_ALT = 57449;
    constexpr uint32_t RIGHT_SUPER = 57450;

    const auto isEnhanced = WI_IsFlagSet(controlKeyState, ENHANCED_KEY);

    switch (virtualKeyCode)
    {
    // Special keys with C0 control codes
    case VK_ESCAPE:
        return 27;
    case VK_RETURN:
        // Keypad Enter has ENHANCED_KEY flag
        return isEnhanced ? KP_ENTER : 13;
    case VK_TAB:
        return 9;
    case VK_BACK:
        return 127;

    // Navigation keys - when ENHANCED_KEY is not set, these are keypad keys
    case VK_INSERT:
        return isEnhanced ? 0 : KP_INSERT; // Use CSI 2~ for enhanced, kitty code for keypad
    case VK_DELETE:
        return isEnhanced ? 0 : KP_DELETE; // Use CSI 3~ for enhanced
    case VK_HOME:
        return isEnhanced ? 0 : KP_HOME; // Use CSI H for enhanced
    case VK_END:
        return isEnhanced ? 0 : KP_END; // Use CSI F for enhanced
    case VK_PRIOR: // Page Up
        return isEnhanced ? 0 : KP_PAGE_UP;
    case VK_NEXT: // Page Down
        return isEnhanced ? 0 : KP_PAGE_DOWN;
    case VK_UP:
        return isEnhanced ? 0 : KP_UP; // Use CSI A for enhanced
    case VK_DOWN:
        return isEnhanced ? 0 : KP_DOWN;
    case VK_LEFT:
        return isEnhanced ? 0 : KP_LEFT;
    case VK_RIGHT:
        return isEnhanced ? 0 : KP_RIGHT;

    // Function keys F1-F12 - use legacy sequences
    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
        return 0; // Use legacy sequences

    // Function keys F13-F24
    case VK_F13:
        return F13;
    case VK_F14:
        return F13 + 1;
    case VK_F15:
        return F13 + 2;
    case VK_F16:
        return F13 + 3;
    case VK_F17:
        return F13 + 4;
    case VK_F18:
        return F13 + 5;
    case VK_F19:
        return F13 + 6;
    case VK_F20:
        return F13 + 7;
    case VK_F21:
        return F13 + 8;
    case VK_F22:
        return F13 + 9;
    case VK_F23:
        return F13 + 10;
    case VK_F24:
        return F13 + 11;

    // Lock keys
    case VK_CAPITAL:
        return CAPS_LOCK;
    case VK_SCROLL:
        return SCROLL_LOCK;
    case VK_NUMLOCK:
        return NUM_LOCK;

    // Other special keys
    case VK_SNAPSHOT:
        return PRINT_SCREEN;
    case VK_PAUSE:
        return PAUSE;
    case VK_APPS:
        return MENU;

    // Keypad keys
    case VK_NUMPAD0:
        return KP_0;
    case VK_NUMPAD1:
        return KP_0 + 1;
    case VK_NUMPAD2:
        return KP_0 + 2;
    case VK_NUMPAD3:
        return KP_0 + 3;
    case VK_NUMPAD4:
        return KP_0 + 4;
    case VK_NUMPAD5:
        return KP_0 + 5;
    case VK_NUMPAD6:
        return KP_0 + 6;
    case VK_NUMPAD7:
        return KP_0 + 7;
    case VK_NUMPAD8:
        return KP_0 + 8;
    case VK_NUMPAD9:
        return KP_0 + 9;
    case VK_DECIMAL:
        return KP_DECIMAL;
    case VK_DIVIDE:
        return KP_DIVIDE;
    case VK_MULTIPLY:
        return KP_MULTIPLY;
    case VK_SUBTRACT:
        return KP_SUBTRACT;
    case VK_ADD:
        return KP_ADD;
    case VK_SEPARATOR:
        return KP_SEPARATOR;
    case VK_CLEAR:
        return KP_0 + 5; // Keypad 5 when NumLock is off

    // Media keys
    case VK_MEDIA_PLAY_PAUSE:
        return MEDIA_PLAY_PAUSE;
    case VK_MEDIA_STOP:
        return MEDIA_STOP;
    case VK_MEDIA_NEXT_TRACK:
        return MEDIA_TRACK_NEXT;
    case VK_MEDIA_PREV_TRACK:
        return MEDIA_TRACK_PREVIOUS;
    case VK_VOLUME_DOWN:
        return LOWER_VOLUME;
    case VK_VOLUME_UP:
        return RAISE_VOLUME;
    case VK_VOLUME_MUTE:
        return MUTE_VOLUME;

    // Modifier keys
    case VK_SHIFT:
        // Distinguish left/right shift by scan code
        return (MapVirtualKeyW(virtualKeyCode, MAPVK_VK_TO_VSC) == 0x2A) ? LEFT_SHIFT : RIGHT_SHIFT;
    case VK_LSHIFT:
        return LEFT_SHIFT;
    case VK_RSHIFT:
        return RIGHT_SHIFT;
    case VK_CONTROL:
        return isEnhanced ? RIGHT_CONTROL : LEFT_CONTROL;
    case VK_LCONTROL:
        return LEFT_CONTROL;
    case VK_RCONTROL:
        return RIGHT_CONTROL;
    case VK_MENU:
        return isEnhanced ? RIGHT_ALT : LEFT_ALT;
    case VK_LMENU:
        return LEFT_ALT;
    case VK_RMENU:
        return RIGHT_ALT;
    case VK_LWIN:
        return LEFT_SUPER;
    case VK_RWIN:
        return RIGHT_SUPER;

    default:
        return 0; // Text-generating key, use unicode codepoint
    }
}

// Generates kitty keyboard protocol output for a key event.
// https://sw.kovidgoyal.net/kitty/keyboard-protocol/
TerminalInput::OutputType TerminalInput::_makeKittyOutput(const KEY_EVENT_RECORD& key, const DWORD controlKeyState) const
{
    const auto virtualKeyCode = key.wVirtualKeyCode;
    const auto unicodeChar = key.uChar.UnicodeChar;
    const auto isKeyDown = key.bKeyDown;
    const auto reportEventTypes = (_kittyFlags & KittyKeyboardProtocolFlags::ReportEventTypes) != 0;
    const auto reportAllKeys = (_kittyFlags & KittyKeyboardProtocolFlags::ReportAllKeys) != 0;
    const auto reportAlternateKeys = (_kittyFlags & KittyKeyboardProtocolFlags::ReportAlternateKeys) != 0;
    const auto reportText = (_kittyFlags & KittyKeyboardProtocolFlags::ReportText) != 0;
    // Note: Disambiguate flag (0x01) is implicitly handled - if we're in this function
    // at all (_kittyFlags != 0), then Ctrl+key and Alt+key combos get CSI u encoding.

    // Without ReportEventTypes or ReportAllKeys, we only handle key down events
    // For Enter, Tab, Backspace - keep legacy behavior unless ReportAllKeys is set
    if (!isKeyDown)
    {
        if (!reportEventTypes)
        {
            return _makeNoOutput();
        }
        // For release events, we need ReportEventTypes
    }

    // Get the functional key code, or 0 if this is a text key
    const auto functionalKeyCode = _getKittyFunctionalKeyCode(virtualKeyCode, controlKeyState);

    // Determine if we should use CSI u encoding
    const auto isModifierKey = (virtualKeyCode >= VK_SHIFT && virtualKeyCode <= VK_MENU) ||
                               virtualKeyCode == VK_LSHIFT || virtualKeyCode == VK_RSHIFT ||
                               virtualKeyCode == VK_LCONTROL || virtualKeyCode == VK_RCONTROL ||
                               virtualKeyCode == VK_LMENU || virtualKeyCode == VK_RMENU ||
                               virtualKeyCode == VK_LWIN || virtualKeyCode == VK_RWIN ||
                               virtualKeyCode == VK_CAPITAL || virtualKeyCode == VK_NUMLOCK ||
                               virtualKeyCode == VK_SCROLL;

    // Special handling for Enter, Tab, Backspace in legacy compatibility
    // These should only be encoded as CSI u when ReportAllKeys is set
    // This allows users to still type "reset" at a shell prompt if an app crashes
    const auto isLegacySpecialKey = (virtualKeyCode == VK_RETURN || virtualKeyCode == VK_TAB || virtualKeyCode == VK_BACK);
    if (isLegacySpecialKey && !reportAllKeys && isKeyDown)
    {
        // Send legacy sequence for compatibility
        switch (virtualKeyCode)
        {
        case VK_RETURN:
            return MakeOutput(_inputMode.test(Mode::LineFeed) ? L"\r\n" : L"\r");
        case VK_TAB:
            if (WI_IsFlagSet(controlKeyState, SHIFT_PRESSED))
            {
                return MakeOutput(fmt::format(FMT_COMPILE(L"{}Z"), _csi));
            }
            return MakeOutput(L"\t");
        case VK_BACK:
            return MakeOutput(_inputMode.test(Mode::BackarrowKey) ? L"\x08" : L"\x7f");
        }
    }

    // With only disambiguate flag (no reportAllKeys), we still send plain text for
    // unmodified text-producing keys, but Escape and modified keys get CSI u encoding.
    // This is covered by the logic below.

    // For text-producing keys without modifiers, send plain UTF-8 unless ReportAllKeys is set.
    // With disambiguate flag, Ctrl+key and Alt+key combos get CSI u encoding, but plain
    // keys (with or without shift) still send text directly.
    if (functionalKeyCode == 0 && !isModifierKey && isKeyDown)
    {
        const auto ctrlIsPressed = WI_IsAnyFlagSet(controlKeyState, CTRL_PRESSED);
        const auto altIsPressed = WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED);

        // If there are no significant modifiers (only shift is allowed for text), send plain text.
        // ReportAllKeys forces everything through CSI u encoding.
        if (!ctrlIsPressed && !altIsPressed && !reportAllKeys)
        {
            if (unicodeChar != 0 && unicodeChar >= 0x20)
            {
                return MakeOutput({ &unicodeChar, 1 });
            }
        }
    }

    // Calculate kitty modifiers early - needed for legacy sequences too
    // kitty: shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
    uint32_t modifiers = 0;
    if (WI_IsFlagSet(controlKeyState, SHIFT_PRESSED))
        modifiers |= 1;
    if (WI_IsAnyFlagSet(controlKeyState, ALT_PRESSED))
        modifiers |= 2;
    if (WI_IsAnyFlagSet(controlKeyState, CTRL_PRESSED))
        modifiers |= 4;
    // Note: Windows key state is not reliably available in controlKeyState
    // super would be modifiers |= 8
    if (WI_IsFlagSet(controlKeyState, CAPSLOCK_ON))
        modifiers |= 64;
    if (WI_IsFlagSet(controlKeyState, NUMLOCK_ON))
        modifiers |= 128;

    const auto encodedModifiers = 1 + modifiers;

    // Determine event type: 1=press, 2=repeat, 3=release
    uint32_t eventType = 1; // press (default)
    if (!isKeyDown)
    {
        eventType = 3; // release
    }
    else if (key.wRepeatCount > 1)
    {
        eventType = 2; // repeat
    }

    // Keys that use legacy CSI sequences (not CSI u):
    // Arrow keys: CSI [1;modifiers] A/B/C/D
    // Home/End: CSI [1;modifiers] H/F
    // F1-F4: CSI [1;modifiers] P/Q/R/S (or SS3 P/Q/R/S without modifiers)
    // Insert/Delete/PageUp/PageDown: CSI [param;modifiers] ~
    // F5-F12: CSI [param;modifiers] ~
    wchar_t legacyFinalChar = 0;
    int legacyParam = 0;
    const auto isEnhanced = WI_IsFlagSet(controlKeyState, ENHANCED_KEY);

    switch (virtualKeyCode)
    {
    case VK_UP:
        legacyFinalChar = L'A';
        legacyParam = 1;
        break;
    case VK_DOWN:
        legacyFinalChar = L'B';
        legacyParam = 1;
        break;
    case VK_RIGHT:
        legacyFinalChar = L'C';
        legacyParam = 1;
        break;
    case VK_LEFT:
        legacyFinalChar = L'D';
        legacyParam = 1;
        break;
    case VK_HOME:
        if (isEnhanced)
        {
            legacyFinalChar = L'H';
            legacyParam = 1;
        }
        break;
    case VK_END:
        if (isEnhanced)
        {
            legacyFinalChar = L'F';
            legacyParam = 1;
        }
        break;
    case VK_INSERT:
        if (isEnhanced)
        {
            legacyFinalChar = L'~';
            legacyParam = 2;
        }
        break;
    case VK_DELETE:
        if (isEnhanced)
        {
            legacyFinalChar = L'~';
            legacyParam = 3;
        }
        break;
    case VK_PRIOR: // Page Up
        if (isEnhanced)
        {
            legacyFinalChar = L'~';
            legacyParam = 5;
        }
        break;
    case VK_NEXT: // Page Down
        if (isEnhanced)
        {
            legacyFinalChar = L'~';
            legacyParam = 6;
        }
        break;
    case VK_F1:
        legacyFinalChar = L'P';
        legacyParam = 1;
        break;
    case VK_F2:
        legacyFinalChar = L'Q';
        legacyParam = 1;
        break;
    case VK_F3:
        legacyFinalChar = L'R';
        legacyParam = 1;
        break;
    case VK_F4:
        legacyFinalChar = L'S';
        legacyParam = 1;
        break;
    case VK_F5:
        legacyFinalChar = L'~';
        legacyParam = 15;
        break;
    case VK_F6:
        legacyFinalChar = L'~';
        legacyParam = 17;
        break;
    case VK_F7:
        legacyFinalChar = L'~';
        legacyParam = 18;
        break;
    case VK_F8:
        legacyFinalChar = L'~';
        legacyParam = 19;
        break;
    case VK_F9:
        legacyFinalChar = L'~';
        legacyParam = 20;
        break;
    case VK_F10:
        legacyFinalChar = L'~';
        legacyParam = 21;
        break;
    case VK_F11:
        legacyFinalChar = L'~';
        legacyParam = 23;
        break;
    case VK_F12:
        legacyFinalChar = L'~';
        legacyParam = 24;
        break;
    }

    // If this is a key that uses legacy CSI sequences, generate it
    if (legacyFinalChar != 0)
    {
        std::wstring seq;
        seq.reserve(16);
        seq.append(_csi);

        if (legacyFinalChar == L'~')
        {
            // Format: CSI param ; modifiers ~ or CSI param ; modifiers : event-type ~
            seq.append(std::to_wstring(legacyParam));
            if (encodedModifiers != 1 || (reportEventTypes && eventType != 1))
            {
                seq.push_back(L';');
                seq.append(std::to_wstring(encodedModifiers));
                if (reportEventTypes && eventType != 1)
                {
                    seq.push_back(L':');
                    seq.append(std::to_wstring(eventType));
                }
            }
        }
        else
        {
            // Format: CSI 1 ; modifiers [letter] or CSI [letter]
            // For kitty, we always use CSI 1 ; modifiers format when modifiers or event type present
            if (encodedModifiers != 1 || (reportEventTypes && eventType != 1))
            {
                seq.append(L"1;");
                seq.append(std::to_wstring(encodedModifiers));
                if (reportEventTypes && eventType != 1)
                {
                    seq.push_back(L':');
                    seq.append(std::to_wstring(eventType));
                }
            }
        }

        seq.push_back(legacyFinalChar);
        return seq;
    }

    // Build kitty CSI u sequence
    // Format: CSI unicode-key-code:shifted-key:base-layout-key ; modifiers:event-type ; text-as-codepoints u

    // Determine the key code to send
    // According to kitty protocol: "the codepoint used is always the lower-case
    // (or more technically, un-shifted) version of the key"
    uint32_t keyCode = functionalKeyCode;
    if (keyCode == 0)
    {
        // For alphabetic keys, use the virtual key code converted to lowercase.
        // We can't use unicodeChar here because when Ctrl is pressed, unicodeChar
        // becomes the control character (e.g., Ctrl+C gives unicodeChar=0x03).
        if (virtualKeyCode >= 'A' && virtualKeyCode <= 'Z')
        {
            keyCode = virtualKeyCode + 32; // Convert to lowercase ('A'->'a')
        }
        // For uppercase unicodeChar (shift+letter), convert to lowercase
        else if (unicodeChar >= L'A' && unicodeChar <= L'Z')
        {
            keyCode = unicodeChar + 32; // Convert to lowercase
        }
        // For printable characters, use the unicode char directly
        else if (unicodeChar >= 0x20)
        {
            keyCode = unicodeChar;
        }
        // For control characters (unicodeChar < 0x20), try to derive from VK code
        else if (virtualKeyCode >= '0' && virtualKeyCode <= '9')
        {
            keyCode = virtualKeyCode; // Use the digit
        }
        else if (virtualKeyCode >= 0xBA && virtualKeyCode <= 0xC0)
        {
            // OEM keys like semicolon, equals, comma, minus, period, slash, backtick
            // These need the actual character, but we'd need ToUnicodeEx to get it.
            // For now, if we have a unicodeChar, use it; otherwise fall through.
            if (unicodeChar != 0)
            {
                keyCode = unicodeChar;
            }
            else
            {
                return _makeNoOutput();
            }
        }
        else if (unicodeChar != 0)
        {
            keyCode = unicodeChar;
        }
        else
        {
            // Unknown key without unicode - shouldn't happen for valid keys
            return _makeNoOutput();
        }
    }

    // Build the sequence
    std::wstring seq;
    seq.reserve(32);
    seq.append(_csi);
    seq.append(std::to_wstring(keyCode));

    // Add alternate keys if requested (shifted key and base layout key)
    if (reportAlternateKeys && modifiers != 0)
    {
        // Add shifted key if shift is pressed
        if ((modifiers & 1) && unicodeChar != 0 && unicodeChar != keyCode)
        {
            seq.push_back(L':');
            seq.append(std::to_wstring(static_cast<uint32_t>(unicodeChar)));
        }

        // TODO: Add base layout key for non-Latin keyboards
        // For now, we just set it to a placeholder value
        // seq.push_back(L':');
        // seq.append(std::to_wstring(baseLayoutKey));
    }

    // Determine if we need to output text-as-codepoints (third field)
    const auto needsText = reportText && reportAllKeys && functionalKeyCode == 0 && unicodeChar >= 0x20 && isKeyDown;

    // We need to include modifiers field if:
    // - modifiers are non-default (encodedModifiers != 1), OR
    // - we need to report non-press event type, OR
    // - we need to output text (text is the 3rd field, so we must have 2nd field too)
    const auto needsModifiers = encodedModifiers != 1 || (reportEventTypes && eventType != 1) || needsText;
    if (needsModifiers)
    {
        seq.push_back(L';');
        seq.append(std::to_wstring(encodedModifiers));

        if (reportEventTypes && eventType != 1)
        {
            seq.push_back(L':');
            seq.append(std::to_wstring(eventType));
        }
    }

    // Add text-as-codepoints if requested and this is a text-generating key
    if (needsText)
    {
        seq.push_back(L';');
        seq.append(std::to_wstring(static_cast<uint32_t>(unicodeChar)));
    }

    seq.push_back(L'u');

    return seq;
}
