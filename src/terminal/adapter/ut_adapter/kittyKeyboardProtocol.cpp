// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <WexTestClass.h>
#include <consoletaeftemplates.hpp>

#include "../../input/terminalInput.hpp"

using namespace WEX::TestExecution;
using namespace WEX::Logging;
using namespace WEX::Common;
using namespace Microsoft::Console::VirtualTerminal;

// Kitty keyboard protocol enhancement flag abbreviations
using KF = TerminalInput::KittyKeyboardProtocolFlags;
constexpr auto D = KF::DisambiguateEscapeCodes;
constexpr auto E = KF::ReportEventTypes;
constexpr auto A = KF::ReportAlternateKeys;
constexpr auto K = KF::ReportAllKeysAsEscapeCodes;
constexpr auto T = KF::ReportAssociatedText;

namespace
{
    TerminalInput::OutputType process(TerminalInput& input, bool keyDown, uint16_t vk, uint16_t sc, wchar_t ch, uint32_t state)
    {
        INPUT_RECORD record{};
        record.EventType = KEY_EVENT;
        record.Event.KeyEvent.bKeyDown = keyDown ? TRUE : FALSE;
        record.Event.KeyEvent.wRepeatCount = 1;
        record.Event.KeyEvent.wVirtualKeyCode = vk;
        record.Event.KeyEvent.wVirtualScanCode = sc;
        record.Event.KeyEvent.uChar.UnicodeChar = ch;
        record.Event.KeyEvent.dwControlKeyState = state;
        return input.HandleKey(record);
    }

    TerminalInput createInput(uint8_t flags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(flags, TerminalInput::KittyKeyboardProtocolMode::Replace);
        return input;
    }

    // Kitty modifier bits: shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
    // Event types: press=1, repeat=2, release=3

    constexpr auto Alt = LEFT_ALT_PRESSED;
    constexpr auto Ctrl = LEFT_CTRL_PRESSED;

    struct TestCase
    {
        std::wstring_view name;
        std::wstring_view expected;
        uint8_t flags;
        bool keyDown;
        uint16_t vk;
        uint16_t sc;
        wchar_t ch;
        uint32_t state;
    };

    constexpr TestCase testCases[] = {
        // Core behavior: DisambiguateEscapeCodes (D)
        { L"D Esc", L"\x1b[27u", D, true, VK_ESCAPE, 1, 0, 0 },
        { L"D Ctrl+a", L"\x1b[97;5u", D, true, 'A', 0x1E, L'\x01', Ctrl },
        { L"D Ctrl+Alt+a", L"\x1b[97;7u", D, true, 'A', 0x1E, L'\x01', Ctrl | Alt },
        { L"D Shift+Alt+a", L"\x1b[97;4u", D, true, 'A', 0x1E, L'A', SHIFT_PRESSED | Alt },

        // Modifiers with AllKeys (K): all keys use CSI u
        { L"K a", L"\x1b[97u", K, true, 'A', 0x1E, L'a', 0 },
        { L"K Shift+a", L"\x1b[97;2u", K, true, 'A', 0x1E, L'A', SHIFT_PRESSED },
        { L"K Alt+a", L"\x1b[97;3u", K, true, 'A', 0x1E, L'a', Alt },
        { L"K Ctrl+a", L"\x1b[97;5u", K, true, 'A', 0x1E, L'\x01', Ctrl },
        { L"K Shift+Alt+a", L"\x1b[97;4u", K, true, 'A', 0x1E, L'A', SHIFT_PRESSED | Alt },
        { L"K Shift+Ctrl+a", L"\x1b[97;6u", K, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | Ctrl },
        { L"K Alt+Ctrl+a", L"\x1b[97;7u", K, true, 'A', 0x1E, L'\x01', Alt | Ctrl },
        { L"K Shift+Alt+Ctrl+a", L"\x1b[97;8u", K, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | Alt | Ctrl },
        { L"K CapsLock+a", L"\x1b[97;65u", K, true, 'A', 0x1E, L'A', CAPSLOCK_ON },
        { L"K NumLock+a", L"\x1b[97;129u", K, true, 'A', 0x1E, L'a', NUMLOCK_ON },
        { L"K CapsLock+NumLock+a", L"\x1b[97;193u", K, true, 'A', 0x1E, L'A', CAPSLOCK_ON | NUMLOCK_ON },
        { L"K all mods", L"\x1b[97;200u", K, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | Alt | Ctrl | CAPSLOCK_ON | NUMLOCK_ON },

        // Enter/Tab/Backspace: legacy with D, CSI u with K
        { L"K Enter", L"\x1b[13u", K, true, VK_RETURN, 0x1C, L'\r', 0 },
        { L"K Tab", L"\x1b[9u", K, true, VK_TAB, 0x0F, L'\t', 0 },
        { L"K Backspace", L"\x1b[127u", K, true, VK_BACK, 0x0E, L'\b', 0 },
        { L"K Shift+Enter", L"\x1b[13;2u", K, true, VK_RETURN, 0x1C, L'\r', SHIFT_PRESSED },
        { L"K Ctrl+Tab", L"\x1b[9;5u", K, true, VK_TAB, 0x0F, L'\t', Ctrl },
        { L"K Alt+Backspace", L"\x1b[127;3u", K, true, VK_BACK, 0x0E, L'\b', Alt },
        { L"K Shift+Tab", L"\x1b[9;2u", K, true, VK_TAB, 0x0F, 0, SHIFT_PRESSED },

        // Event types (D|E, E|K): release sends ;1:3
        { L"D|E Esc press", L"\x1b[27u", D | E, true, VK_ESCAPE, 1, 0, 0 },
        { L"D|E Esc release", L"\x1b[27;1:3u", D | E, false, VK_ESCAPE, 1, 0, 0 },
        { L"E|K a press", L"\x1b[97u", E | K, true, 'A', 0x1E, L'a', 0 },
        { L"E|K a release", L"\x1b[97;1:3u", E | K, false, 'A', 0x1E, L'a', 0 },
        { L"E|K Enter release", L"\x1b[13;1:3u", E | K, false, VK_RETURN, 0x1C, L'\r', 0 },
        { L"E|K Tab release", L"\x1b[9;1:3u", E | K, false, VK_TAB, 0x0F, L'\t', 0 },
        { L"E|K Backspace release", L"\x1b[127;1:3u", E | K, false, VK_BACK, 0x0E, L'\b', 0 },
        { L"D|E Shift+Esc press", L"\x1b[27;2u", D | E, true, VK_ESCAPE, 1, 0, SHIFT_PRESSED },
        { L"D|E Shift+Esc release", L"\x1b[27;2:3u", D | E, false, VK_ESCAPE, 1, 0, SHIFT_PRESSED },

        // Keypad keys (D disambiguates, getting CSI u with PUA codes)
        { L"D Numpad0", L"\x1b[57399u", D, true, VK_NUMPAD0, 0x52, L'0', 0 },
        { L"D Numpad5", L"\x1b[57404u", D, true, VK_NUMPAD5, 0x4C, L'5', 0 },
        { L"D Numpad9", L"\x1b[57408u", D, true, VK_NUMPAD9, 0x49, L'9', 0 },
        { L"D Numpad Decimal", L"\x1b[57409u", D, true, VK_DECIMAL, 0x53, L'.', 0 },
        { L"D Numpad Divide", L"\x1b[57410u", D, true, VK_DIVIDE, 0x35, L'/', ENHANCED_KEY },
        { L"D Numpad Multiply", L"\x1b[57411u", D, true, VK_MULTIPLY, 0x37, L'*', 0 },
        { L"D Numpad Subtract", L"\x1b[57412u", D, true, VK_SUBTRACT, 0x4A, L'-', 0 },
        { L"D Numpad Add", L"\x1b[57413u", D, true, VK_ADD, 0x4E, L'+', 0 },
        { L"D Shift+Numpad5", L"\x1b[57404;2u", D, true, VK_NUMPAD5, 0x4C, L'5', SHIFT_PRESSED },

        // Lock keys and modifier keys (K reports them)
        { L"K CapsLock key", L"\x1b[57358u", K, true, VK_CAPITAL, 0x3A, 0, 0 },
        { L"K NumLock key", L"\x1b[57360u", K, true, VK_NUMLOCK, 0x45, 0, ENHANCED_KEY },
        { L"K ScrollLock key", L"\x1b[57359u", K, true, VK_SCROLL, 0x46, 0, 0 },
        { L"K Left Shift", L"\x1b[57441;2u", K, true, VK_SHIFT, 0x2A, 0, SHIFT_PRESSED },
        { L"K Right Shift", L"\x1b[57447;2u", K, true, VK_SHIFT, 0x36, 0, SHIFT_PRESSED },
        { L"K Left Ctrl", L"\x1b[57442;5u", K, true, VK_CONTROL, 0x1D, 0, Ctrl },
        { L"K Right Ctrl", L"\x1b[57448;5u", K, true, VK_CONTROL, 0x1D, 0, Ctrl | ENHANCED_KEY },
        { L"K Left Alt", L"\x1b[57443;3u", K, true, VK_MENU, 0x38, 0, Alt },
        { L"K Right Alt", L"\x1b[57449;3u", K, true, VK_MENU, 0x38, 0, RIGHT_ALT_PRESSED | ENHANCED_KEY },
        { L"K Left Win", L"\x1b[57444u", K, true, VK_LWIN, 0x5B, 0, ENHANCED_KEY },
        { L"K Right Win", L"\x1b[57450u", K, true, VK_RWIN, 0x5C, 0, ENHANCED_KEY },

        // Special keys
        { L"K Pause", L"\x1b[57362u", K, true, VK_PAUSE, 0x45, 0, 0 },
        { L"K PrintScreen", L"\x1b[57361u", K, true, VK_SNAPSHOT, 0x37, 0, ENHANCED_KEY },
        { L"K Menu", L"\x1b[57363u", K, true, VK_APPS, 0x5D, 0, ENHANCED_KEY },

        // Navigation keys: enhanced=keypad keys with PUA codes
        { L"K Keypad Home", L"\x1b[57423u", K, true, VK_HOME, 0x47, 0, 0 },
        { L"K Keypad End", L"\x1b[57424u", K, true, VK_END, 0x4F, 0, 0 },
        { L"K Keypad Insert", L"\x1b[57425u", K, true, VK_INSERT, 0x52, 0, 0 },
        { L"K Keypad Delete", L"\x1b[57426u", K, true, VK_DELETE, 0x53, 0, 0 },
        { L"K Keypad PageUp", L"\x1b[57421u", K, true, VK_PRIOR, 0x49, 0, 0 },
        { L"K Keypad PageDown", L"\x1b[57422u", K, true, VK_NEXT, 0x51, 0, 0 },
        { L"K Keypad Up", L"\x1b[57419u", K, true, VK_UP, 0x48, 0, 0 },
        { L"K Keypad Down", L"\x1b[57420u", K, true, VK_DOWN, 0x50, 0, 0 },
        { L"K Keypad Left", L"\x1b[57417u", K, true, VK_LEFT, 0x4B, 0, 0 },
        { L"K Keypad Right", L"\x1b[57418u", K, true, VK_RIGHT, 0x4D, 0, 0 },

        // Media keys
        { L"K Media Play/Pause", L"\x1b[57430u", K, true, VK_MEDIA_PLAY_PAUSE, 0, 0, 0 },
        { L"K Media Stop", L"\x1b[57432u", K, true, VK_MEDIA_STOP, 0, 0, 0 },
        { L"K Media Next", L"\x1b[57435u", K, true, VK_MEDIA_NEXT_TRACK, 0, 0, 0 },
        { L"K Media Prev", L"\x1b[57436u", K, true, VK_MEDIA_PREV_TRACK, 0, 0, 0 },
        { L"K Volume Down", L"\x1b[57438u", K, true, VK_VOLUME_DOWN, 0, 0, 0 },
        { L"K Volume Up", L"\x1b[57439u", K, true, VK_VOLUME_UP, 0, 0, 0 },
        { L"K Volume Mute", L"\x1b[57440u", K, true, VK_VOLUME_MUTE, 0, 0, 0 },

        // Function keys F13-F24 (PUA codes)
        { L"K F13", L"\x1b[57376u", K, true, VK_F13, 0x64, 0, 0 },
        { L"K F20", L"\x1b[57383u", K, true, VK_F20, 0x6B, 0, 0 },
        { L"K F24", L"\x1b[57387u", K, true, VK_F24, 0x76, 0, 0 },
        { L"K Shift+F13", L"\x1b[57376;2u", K, true, VK_F13, 0x64, 0, SHIFT_PRESSED },

        // Alternate keys (A|K): shifted key and base layout key
        { L"A|K Shift+a", L"\x1b[97:65;2u", A | K, true, 'A', 0x1E, L'A', SHIFT_PRESSED },
        { L"A|K Shift+1", L"\x1b[49:33;2u", A | K, true, '1', 0x02, L'!', SHIFT_PRESSED },
        { L"A|K a (no shift)", L"\x1b[97u", A | K, true, 'A', 0x1E, L'a', 0 },

        // Associated text (K|T): text codepoint in 3rd param
        { L"K|T Shift+a", L"\x1b[97;2;65u", K | T, true, 'A', 0x1E, L'A', SHIFT_PRESSED },
        { L"K|T Shift+1", L"\x1b[49;2;33u", K | T, true, '1', 0x02, L'!', SHIFT_PRESSED },
        { L"K|T Ctrl+a", L"\x1b[97;5u", K | T, true, 'A', 0x1E, L'\x01', Ctrl }, // control char omitted

        // Edge cases
        { L"K Keypad Enter", L"\x1b[57414u", K, true, VK_RETURN, 0x1C, L'\r', ENHANCED_KEY },
        { L"K Regular Enter", L"\x1b[13u", K, true, VK_RETURN, 0x1C, L'\r', 0 },
        { L"K Shift+Alt+Ctrl+Esc", L"\x1b[27;8u", K, true, VK_ESCAPE, 1, 0, SHIFT_PRESSED | Alt | Ctrl },
        { L"E|K CapsLock+a", L"\x1b[97;65u", E | K, true, 'A', 0x1E, L'A', CAPSLOCK_ON },
        { L"E|K all mods release", L"\x1b[97;200:3u", E | K, false, 'A', 0x1E, L'\x01', SHIFT_PRESSED | Alt | Ctrl | CAPSLOCK_ON | NUMLOCK_ON },

        // F1-F4 with kitty flags (CSI instead of SS3, F3 special case)
        { L"D F1", L"\x1b[P", D, true, VK_F1, 0x3B, 0, 0 },
        { L"D F2", L"\x1b[Q", D, true, VK_F2, 0x3C, 0, 0 },
        { L"D F3", L"\x1b[13~", D, true, VK_F3, 0x3D, 0, 0 }, // F3 uses ~ per updated spec
        { L"D F4", L"\x1b[S", D, true, VK_F4, 0x3E, 0, 0 },
        { L"D Shift+F1", L"\x1b[1;2P", D, true, VK_F1, 0x3B, 0, SHIFT_PRESSED },
        { L"K F5", L"\x1b[15~", K, true, VK_F5, 0x3F, 0, 0 },
        { L"K F12", L"\x1b[24~", K, true, VK_F12, 0x58, 0, 0 },
        { L"K Shift+F5", L"\x1b[15;2~", K, true, VK_F5, 0x3F, 0, SHIFT_PRESSED },

        // Navigation with ENHANCED_KEY (regular arrows, not keypad)
        { L"K Up", L"\x1b[A", K, true, VK_UP, 0x48, 0, ENHANCED_KEY },
        { L"K Down", L"\x1b[B", K, true, VK_DOWN, 0x50, 0, ENHANCED_KEY },
        { L"K Right", L"\x1b[C", K, true, VK_RIGHT, 0x4D, 0, ENHANCED_KEY },
        { L"K Left", L"\x1b[D", K, true, VK_LEFT, 0x4B, 0, ENHANCED_KEY },
        { L"K Home", L"\x1b[H", K, true, VK_HOME, 0x47, 0, ENHANCED_KEY },
        { L"K End", L"\x1b[F", K, true, VK_END, 0x4F, 0, ENHANCED_KEY },
        { L"K Insert", L"\x1b[2~", K, true, VK_INSERT, 0x52, 0, ENHANCED_KEY },
        { L"K Delete", L"\x1b[3~", K, true, VK_DELETE, 0x53, 0, ENHANCED_KEY },
        { L"K PageUp", L"\x1b[5~", K, true, VK_PRIOR, 0x49, 0, ENHANCED_KEY },
        { L"K PageDown", L"\x1b[6~", K, true, VK_NEXT, 0x51, 0, ENHANCED_KEY },
        { L"K Shift+Up", L"\x1b[1;2A", K, true, VK_UP, 0x48, 0, SHIFT_PRESSED | ENHANCED_KEY },
        { L"K Ctrl+Home", L"\x1b[1;5H", K, true, VK_HOME, 0x47, 0, Ctrl | ENHANCED_KEY },
        { L"K Clear", L"\x1b[E", K, true, VK_CLEAR, 0x4C, 0, ENHANCED_KEY },

        // Additional edge cases
        // F-key with event type
        { L"E|K F1 release", L"\x1b[1;1:3P", E | K, false, VK_F1, 0x3B, 0, 0 },
        { L"E|K F5 release", L"\x1b[15;1:3~", E | K, false, VK_F5, 0x3F, 0, 0 },
        // Navigation release
        { L"E|K Up release", L"\x1b[1;1:3A", E | K, false, VK_UP, 0x48, 0, ENHANCED_KEY },
        { L"E|K Insert release", L"\x1b[2;1:3~", E | K, false, VK_INSERT, 0x52, 0, ENHANCED_KEY },
        // Alternate keys with modifiers
        { L"A|K Shift+Ctrl+a", L"\x1b[97:65;6u", A | K, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | Ctrl },
        // Associated text with plain key
        { L"K|T a", L"\x1b[97;;97u", K | T, true, 'A', 0x1E, L'a', 0 },
        // Text not reported on release
        { L"E|K|T a release", L"\x1b[97;1:3u", E | K | T, false, 'A', 0x1E, L'a', 0 },
        // Escape has no associated text
        { L"K|T Esc", L"\x1b[27u", K | T, true, VK_ESCAPE, 1, 0, 0 },
        // Combined flags: alternate keys with locks
        { L"A|K CapsLock+Shift+a", L"\x1b[97:65;66u", A | K, true, 'A', 0x1E, L'a', CAPSLOCK_ON | SHIFT_PRESSED },
        // All flags combined
        { L"A|K|T Shift+a", L"\x1b[97:65;2;65u", A | K | T, true, 'A', 0x1E, L'A', SHIFT_PRESSED },

        // Release without EventTypes flag: no output
        { L"K a release (no EventTypes)", L"", K, false, 'A', 0x1E, L'a', 0 },

        // Enter/Tab/Backspace release without AllKeys: no output
        { L"D|E Enter press", L"\r", D | E, true, VK_RETURN, 0x1C, L'\r', 0 },
        { L"D|E Enter release (no AllKeys)", L"", D | E, false, VK_RETURN, 0x1C, L'\r', 0 },

        // Modifier key press/release
        { L"E|K Left Shift press", L"\x1b[57441;2u", E | K, true, VK_SHIFT, 0x2A, 0, SHIFT_PRESSED },
        { L"E|K Left Shift release", L"\x1b[57441;1:3u", E | K, false, VK_SHIFT, 0x2A, 0, 0 },

        // Lock key toggle (CapsLock)
        { L"E|K CapsLock press", L"\x1b[57358u", E | K, true, VK_CAPITAL, 0x3A, 0, 0 },
        { L"E|K CapsLock release (now on)", L"\x1b[57358;65:3u", E | K, false, VK_CAPITAL, 0x3A, 0, CAPSLOCK_ON },

        // Associated text filtering
        { L"K|T Shift+a (text)", L"\x1b[97;2;65u", K | T, true, 'A', 0x1E, L'A', SHIFT_PRESSED },
        { L"K|T Ctrl+a (control char filtered)", L"\x1b[97;5u", K | T, true, 'A', 0x1E, L'\x01', Ctrl },
        { L"K|T Esc (no text)", L"\x1b[27u", K | T, true, VK_ESCAPE, 1, 0, 0 },
    };
}

extern "C" HRESULT __declspec(dllexport) __cdecl KittyKeyTestDataSource(IDataSource** ppDataSource, void*)
{
    *ppDataSource = new ArrayIndexTaefAdapterSource(std::size(testCases));
    return S_OK;
}

class KittyKeyboardProtocolTests
{
    TEST_CLASS(KittyKeyboardProtocolTests);

    TEST_METHOD(KeyPressTests)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"DataSource", L"Export:KittyKeyTestDataSource")
        END_TEST_METHOD_PROPERTIES()

        DisableVerifyExceptions disableVerifyExceptions{};
        SetVerifyOutput verifyOutputScope{ VerifyOutputSettings::LogOnlyFailures };

        size_t i{};
        TestData::TryGetValue(L"index", i);
        const auto& tc = testCases[i];

        Log::Comment(NoThrowString().Format(L"[%zu] %.*s", i, static_cast<int>(tc.name.size()), tc.name.data()));

        auto input = createInput(tc.flags);
        const auto expected = TerminalInput::MakeOutput(tc.expected);
        const auto actual = process(input, tc.keyDown, tc.vk, tc.sc, tc.ch, tc.state);
        const auto msg = fmt::format(L"{} != {}", til::visualize_control_codes(expected.value_or({})), til::visualize_control_codes(actual.value_or({})));
        VERIFY_ARE_EQUAL(expected, actual, msg.c_str());
    }

    TEST_METHOD(KeyRepeatEvents)
    {
        auto input = createInput(E | K);
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97u"), process(input, true, 'A', 0x1E, L'a', 0));
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97;1:2u"), process(input, true, 'A', 0x1E, L'a', 0)); // repeat
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97;1:2u"), process(input, true, 'A', 0x1E, L'a', 0)); // repeat
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97;1:3u"), process(input, false, 'A', 0x1E, L'a', 0)); // release
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97u"), process(input, true, 'A', 0x1E, L'a', 0)); // new press
    }

    TEST_METHOD(KeyRepeatWithModifiers)
    {
        auto input = createInput(E | K);
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97;2u"), process(input, true, 'A', 0x1E, L'A', SHIFT_PRESSED));
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97;2:2u"), process(input, true, 'A', 0x1E, L'A', SHIFT_PRESSED));
    }

    TEST_METHOD(KeyRepeatResetOnDifferentKey)
    {
        auto input = createInput(E | K);
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97u"), process(input, true, 'A', 0x1E, L'a', 0));
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[98u"), process(input, true, 'B', 0x30, L'b', 0)); // different key
        VERIFY_ARE_EQUAL(TerminalInput::MakeOutput(L"\x1b[97u"), process(input, true, 'A', 0x1E, L'a', 0)); // not repeat
    }
};
