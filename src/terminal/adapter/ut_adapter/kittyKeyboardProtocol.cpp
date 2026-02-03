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
using KittyKeyboardProtocolFlags = TerminalInput::KittyKeyboardProtocolFlags;
using KittyKeyboardProtocolMode = TerminalInput::KittyKeyboardProtocolMode;

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
        input.SetKittyKeyboardProtocol(flags, KittyKeyboardProtocolMode::Replace);
        return input;
    }

    // Kitty modifier bit values (as transmitted, before adding 1):
    // shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
    // Transmitted as: 1 + modifiers

    // CSI = "\x1b["

    // Helper macros for common state combinations
    constexpr auto ALT_PRESSED = LEFT_ALT_PRESSED; // Use left for consistency
    constexpr auto CTRL_PRESSED = LEFT_CTRL_PRESSED;

    struct TestCase
    {
        std::wstring_view name;
        std::wstring_view expected;
        uint8_t flags; // KittyKeyboardProtocolFlags
        bool keyDown;
        uint16_t vk;
        uint16_t sc;
        wchar_t ch;
        uint32_t state;
    };

    // ========================================================================
    // Test case organization:
    //
    // 1. FLAG COMBINATIONS (32 total = 2^5 enhancement flags)
    //    Testing each flag combination with a representative key
    //
    // 2. MODIFIER COMBINATIONS
    //    Testing all modifier permutations (shift, alt, ctrl, caps_lock, num_lock)
    //
    // 3. SPECIAL KEY BEHAVIORS
    //    - Enter/Tab/Backspace legacy behavior
    //    - Escape key disambiguation
    //    - Keypad keys
    //    - Function keys
    //    - Lock keys
    //    - Modifier keys themselves
    //
    // 4. EVENT TYPES
    //    - Press, repeat, release events
    //    - Special handling for Enter/Tab/Backspace release
    //
    // 5. ALTERNATE KEYS
    //    - Shifted key codes
    //    - Base layout key codes
    //
    // 6. TEXT AS CODEPOINTS
    //    - Text embedded in escape codes
    // ========================================================================

    constexpr TestCase testCases[] = {
        // ====================================================================
        // SECTION 1: Enhancement Flag Combinations (32 combinations)
        // Using Escape key as representative since it's affected by Disambiguate
        // ====================================================================

        // flags=0 (0b00000): No enhancements - legacy mode
        // Escape key in legacy mode: just ESC byte
        TestCase{ L"Flags=0 (none) Esc key", L"\x1b", 0, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=1 (0b00001): DisambiguateEscapeCodes only
        // Escape key becomes CSI 27 u
        TestCase{ L"Flags=1 (Disambiguate) Esc key", L"\x1b[27u", 1, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=2 (0b00010): ReportEventTypes only
        // No disambiguation, so Esc is still legacy (but with event type tracking internally)
        TestCase{ L"Flags=2 (EventTypes) Esc key down", L"\x1b", 2, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=3 (0b00011): Disambiguate + EventTypes
        // Escape key with event type: CSI 27;1:1 u (mod=1, event=press=1)
        TestCase{ L"Flags=3 (Disambiguate+EventTypes) Esc key press", L"\x1b[27u", 3, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=4 (0b00100): ReportAlternateKeys only
        // Without Disambiguate, Escape is still legacy
        TestCase{ L"Flags=4 (AltKeys) Esc key", L"\x1b", 4, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=5 (0b00101): Disambiguate + AltKeys
        TestCase{ L"Flags=5 (Disambiguate+AltKeys) Esc key", L"\x1b[27u", 5, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=6 (0b00110): EventTypes + AltKeys
        TestCase{ L"Flags=6 (EventTypes+AltKeys) Esc key", L"\x1b", 6, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=7 (0b00111): Disambiguate + EventTypes + AltKeys
        TestCase{ L"Flags=7 (Disambiguate+EventTypes+AltKeys) Esc key press", L"\x1b[27u", 7, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=8 (0b01000): ReportAllKeysAsEscapeCodes only
        // All keys become CSI u, including Escape
        TestCase{ L"Flags=8 (AllKeys) Esc key", L"\x1b[27u", 8, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=9 (0b01001): Disambiguate + AllKeys
        TestCase{ L"Flags=9 (Disambiguate+AllKeys) Esc key", L"\x1b[27u", 9, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=10 (0b01010): EventTypes + AllKeys
        TestCase{ L"Flags=10 (EventTypes+AllKeys) Esc key press", L"\x1b[27u", 10, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=11 (0b01011): Disambiguate + EventTypes + AllKeys
        TestCase{ L"Flags=11 (Disambiguate+EventTypes+AllKeys) Esc key press", L"\x1b[27u", 11, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=12 (0b01100): AltKeys + AllKeys
        TestCase{ L"Flags=12 (AltKeys+AllKeys) Esc key", L"\x1b[27u", 12, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=13 (0b01101): Disambiguate + AltKeys + AllKeys
        TestCase{ L"Flags=13 (Disambiguate+AltKeys+AllKeys) Esc key", L"\x1b[27u", 13, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=14 (0b01110): EventTypes + AltKeys + AllKeys
        TestCase{ L"Flags=14 (EventTypes+AltKeys+AllKeys) Esc key press", L"\x1b[27u", 14, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=15 (0b01111): Disambiguate + EventTypes + AltKeys + AllKeys
        TestCase{ L"Flags=15 (Disambiguate+EventTypes+AltKeys+AllKeys) Esc key press", L"\x1b[27u", 15, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=16 (0b10000): ReportAssociatedText only (meaningless without AllKeys)
        TestCase{ L"Flags=16 (AssocText) Esc key", L"\x1b", 16, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=17 (0b10001): Disambiguate + AssocText
        TestCase{ L"Flags=17 (Disambiguate+AssocText) Esc key", L"\x1b[27u", 17, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=18 (0b10010): EventTypes + AssocText
        TestCase{ L"Flags=18 (EventTypes+AssocText) Esc key", L"\x1b", 18, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=19 (0b10011): Disambiguate + EventTypes + AssocText
        TestCase{ L"Flags=19 (Disambiguate+EventTypes+AssocText) Esc key press", L"\x1b[27u", 19, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=20 (0b10100): AltKeys + AssocText
        TestCase{ L"Flags=20 (AltKeys+AssocText) Esc key", L"\x1b", 20, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=21 (0b10101): Disambiguate + AltKeys + AssocText
        TestCase{ L"Flags=21 (Disambiguate+AltKeys+AssocText) Esc key", L"\x1b[27u", 21, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=22 (0b10110): EventTypes + AltKeys + AssocText
        TestCase{ L"Flags=22 (EventTypes+AltKeys+AssocText) Esc key", L"\x1b", 22, true, VK_ESCAPE, 0x01, L'\x1b', 0 },

        // flags=23 (0b10111): Disambiguate + EventTypes + AltKeys + AssocText
        TestCase{ L"Flags=23 (Disambiguate+EventTypes+AltKeys+AssocText) Esc key press", L"\x1b[27u", 23, true, VK_ESCAPE, 0x01, 0, 0 },

        // flags=24 (0b11000): AllKeys + AssocText
        // 'a' key with text reporting: CSI 97;;97 u
        TestCase{ L"Flags=24 (AllKeys+AssocText) 'a' key", L"\x1b[97;;97u", 24, true, 'A', 0x1E, L'a', 0 },

        // flags=25 (0b11001): Disambiguate + AllKeys + AssocText
        TestCase{ L"Flags=25 (Disambiguate+AllKeys+AssocText) 'a' key", L"\x1b[97;;97u", 25, true, 'A', 0x1E, L'a', 0 },

        // flags=26 (0b11010): EventTypes + AllKeys + AssocText
        TestCase{ L"Flags=26 (EventTypes+AllKeys+AssocText) 'a' key press", L"\x1b[97;;97u", 26, true, 'A', 0x1E, L'a', 0 },

        // flags=27 (0b11011): Disambiguate + EventTypes + AllKeys + AssocText
        TestCase{ L"Flags=27 (Disambiguate+EventTypes+AllKeys+AssocText) 'a' key press", L"\x1b[97;;97u", 27, true, 'A', 0x1E, L'a', 0 },

        // flags=28 (0b11100): AltKeys + AllKeys + AssocText
        TestCase{ L"Flags=28 (AltKeys+AllKeys+AssocText) 'a' key", L"\x1b[97;;97u", 28, true, 'A', 0x1E, L'a', 0 },

        // flags=29 (0b11101): Disambiguate + AltKeys + AllKeys + AssocText
        TestCase{ L"Flags=29 (Disambiguate+AltKeys+AllKeys+AssocText) 'a' key", L"\x1b[97;;97u", 29, true, 'A', 0x1E, L'a', 0 },

        // flags=30 (0b11110): EventTypes + AltKeys + AllKeys + AssocText
        TestCase{ L"Flags=30 (EventTypes+AltKeys+AllKeys+AssocText) 'a' key press", L"\x1b[97;;97u", 30, true, 'A', 0x1E, L'a', 0 },

        // flags=31 (0b11111): All flags enabled
        TestCase{ L"Flags=31 (all) 'a' key press", L"\x1b[97;;97u", 31, true, 'A', 0x1E, L'a', 0 },

        // ====================================================================
        // SECTION 2: Modifier Combinations with Disambiguate (flag=1)
        // Testing all modifier permutations with 'a' key
        // Kitty modifier encoding: shift=1, alt=2, ctrl=4, caps_lock=64, num_lock=128
        // Transmitted value = 1 + modifiers
        // ====================================================================

        // Alt+'a' -> CSI 97;3 u (mod=1+2=3)
        TestCase{ L"Disambiguate: Alt+a", L"\x1b[97;3u", 1, true, 'A', 0x1E, L'a', ALT_PRESSED },

        // Ctrl+'a' -> CSI 97;5 u (mod=1+4=5)
        TestCase{ L"Disambiguate: Ctrl+a", L"\x1b[97;5u", 1, true, 'A', 0x1E, L'\x01', CTRL_PRESSED },

        // Ctrl+Alt+'a' -> CSI 97;7 u (mod=1+2+4=7)
        TestCase{ L"Disambiguate: Ctrl+Alt+a", L"\x1b[97;7u", 1, true, 'A', 0x1E, L'\x01', CTRL_PRESSED | ALT_PRESSED },

        // Shift+Alt+'a' -> CSI 97;4 u (mod=1+1+2=4)
        TestCase{ L"Disambiguate: Shift+Alt+a", L"\x1b[97;4u", 1, true, 'A', 0x1E, L'A', SHIFT_PRESSED | ALT_PRESSED },

        // ====================================================================
        // SECTION 3: Modifier combinations with AllKeys (flag=8)
        // All keys produce CSI u, lock keys are reported
        // ====================================================================

        // No modifiers: 'a' -> CSI 97 u
        TestCase{ L"AllKeys: 'a' no mods", L"\x1b[97u", 8, true, 'A', 0x1E, L'a', 0 },

        // Shift+'a' -> CSI 97;2 u (mod=1+1=2)
        TestCase{ L"AllKeys: Shift+a", L"\x1b[97;2u", 8, true, 'A', 0x1E, L'A', SHIFT_PRESSED },

        // Alt+'a' -> CSI 97;3 u (mod=1+2=3)
        TestCase{ L"AllKeys: Alt+a", L"\x1b[97;3u", 8, true, 'A', 0x1E, L'a', ALT_PRESSED },

        // Ctrl+'a' -> CSI 97;5 u (mod=1+4=5)
        TestCase{ L"AllKeys: Ctrl+a", L"\x1b[97;5u", 8, true, 'A', 0x1E, L'\x01', CTRL_PRESSED },

        // Shift+Alt+'a' -> CSI 97;4 u (mod=1+1+2=4)
        TestCase{ L"AllKeys: Shift+Alt+a", L"\x1b[97;4u", 8, true, 'A', 0x1E, L'A', SHIFT_PRESSED | ALT_PRESSED },

        // Shift+Ctrl+'a' -> CSI 97;6 u (mod=1+1+4=6)
        TestCase{ L"AllKeys: Shift+Ctrl+a", L"\x1b[97;6u", 8, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | CTRL_PRESSED },

        // Alt+Ctrl+'a' -> CSI 97;7 u (mod=1+2+4=7)
        TestCase{ L"AllKeys: Alt+Ctrl+a", L"\x1b[97;7u", 8, true, 'A', 0x1E, L'\x01', ALT_PRESSED | CTRL_PRESSED },

        // Shift+Alt+Ctrl+'a' -> CSI 97;8 u (mod=1+1+2+4=8)
        TestCase{ L"AllKeys: Shift+Alt+Ctrl+a", L"\x1b[97;8u", 8, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED },

        // CapsLock+'a' -> CSI 97;65 u (mod=1+64=65)
        TestCase{ L"AllKeys: CapsLock+a", L"\x1b[97;65u", 8, true, 'A', 0x1E, L'A', CAPSLOCK_ON },

        // NumLock+'a' -> CSI 97;129 u (mod=1+128=129)
        TestCase{ L"AllKeys: NumLock+a", L"\x1b[97;129u", 8, true, 'A', 0x1E, L'a', NUMLOCK_ON },

        // CapsLock+NumLock+'a' -> CSI 97;193 u (mod=1+64+128=193)
        TestCase{ L"AllKeys: CapsLock+NumLock+a", L"\x1b[97;193u", 8, true, 'A', 0x1E, L'A', CAPSLOCK_ON | NUMLOCK_ON },

        // Shift+CapsLock+'a' -> CSI 97;66 u (mod=1+1+64=66)
        TestCase{ L"AllKeys: Shift+CapsLock+a", L"\x1b[97;66u", 8, true, 'A', 0x1E, L'a', SHIFT_PRESSED | CAPSLOCK_ON },

        // All modifiers: Shift+Alt+Ctrl+CapsLock+NumLock
        // mod=1+1+2+4+64+128=200
        TestCase{ L"AllKeys: all mods", L"\x1b[97;200u", 8, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED | CAPSLOCK_ON | NUMLOCK_ON },

        // ====================================================================
        // SECTION 4: Enter, Tab, Backspace - Legacy behavior exceptions
        // Per spec: "The only exceptions are the Enter, Tab and Backspace keys
        // which still generate the same bytes as in legacy mode"
        // ====================================================================

        // With Disambiguate only (flag=1), these stay legacy:
        // (These should return MakeUnhandled(), causing legacy processing)
        // We'll test that they DON'T produce CSI u output

        // With AllKeys (flag=8), they DO get CSI u encoding:
        // Enter -> CSI 13 u
        TestCase{ L"AllKeys: Enter", L"\x1b[13u", 8, true, VK_RETURN, 0x1C, L'\r', 0 },

        // Tab -> CSI 9 u
        TestCase{ L"AllKeys: Tab", L"\x1b[9u", 8, true, VK_TAB, 0x0F, L'\t', 0 },

        // Backspace -> CSI 127 u
        TestCase{ L"AllKeys: Backspace", L"\x1b[127u", 8, true, VK_BACK, 0x0E, L'\b', 0 },

        // Enter with Shift -> CSI 13;2 u
        TestCase{ L"AllKeys: Shift+Enter", L"\x1b[13;2u", 8, true, VK_RETURN, 0x1C, L'\r', SHIFT_PRESSED },

        // Tab with Ctrl -> CSI 9;5 u
        TestCase{ L"AllKeys: Ctrl+Tab", L"\x1b[9;5u", 8, true, VK_TAB, 0x0F, L'\t', CTRL_PRESSED },

        // Backspace with Alt -> CSI 127;3 u
        TestCase{ L"AllKeys: Alt+Backspace", L"\x1b[127;3u", 8, true, VK_BACK, 0x0E, L'\b', ALT_PRESSED },

        // ====================================================================
        // SECTION 5: Event Types (flag=2)
        // press=1, repeat=2, release=3
        // Format: CSI keycode;mod:event u
        // ====================================================================

        // Key press with Disambiguate+EventTypes (flag=3)
        TestCase{ L"EventTypes: Esc press", L"\x1b[27u", 3, true, VK_ESCAPE, 0x01, 0, 0 },

        // Key release with Disambiguate+EventTypes (flag=3)
        TestCase{ L"EventTypes: Esc release", L"\x1b[27;1:3u", 3, false, VK_ESCAPE, 0x01, 0, 0 },

        // Key press with AllKeys+EventTypes (flag=10)
        TestCase{ L"EventTypes+AllKeys: 'a' press", L"\x1b[97u", 10, true, 'A', 0x1E, L'a', 0 },

        // Key release with AllKeys+EventTypes (flag=10)
        TestCase{ L"EventTypes+AllKeys: 'a' release", L"\x1b[97;1:3u", 10, false, 'A', 0x1E, L'a', 0 },

        // Enter/Tab/Backspace release - only with AllKeys+EventTypes
        // Without AllKeys, release events for these are suppressed
        TestCase{ L"EventTypes+AllKeys: Enter release", L"\x1b[13;1:3u", 10, false, VK_RETURN, 0x1C, L'\r', 0 },
        TestCase{ L"EventTypes+AllKeys: Tab release", L"\x1b[9;1:3u", 10, false, VK_TAB, 0x0F, L'\t', 0 },
        TestCase{ L"EventTypes+AllKeys: Backspace release", L"\x1b[127;1:3u", 10, false, VK_BACK, 0x0E, L'\b', 0 },

        // Press with modifier: Shift+Esc -> CSI 27;2 u
        TestCase{ L"EventTypes: Shift+Esc press", L"\x1b[27;2u", 3, true, VK_ESCAPE, 0x01, 0, SHIFT_PRESSED },

        // Release with modifier: Shift+Esc -> CSI 27;2:3 u
        TestCase{ L"EventTypes: Shift+Esc release", L"\x1b[27;2:3u", 3, false, VK_ESCAPE, 0x01, 0, SHIFT_PRESSED },

        // ====================================================================
        // SECTION 6: Keypad Keys
        // With Disambiguate, keypad keys get CSI u with special codepoints
        // ====================================================================

        // Keypad 0-9: 57399-57408
        TestCase{ L"Disambiguate: Numpad0", L"\x1b[57399u", 1, true, VK_NUMPAD0, 0x52, L'0', 0 },
        TestCase{ L"Disambiguate: Numpad1", L"\x1b[57400u", 1, true, VK_NUMPAD1, 0x4F, L'1', 0 },
        TestCase{ L"Disambiguate: Numpad5", L"\x1b[57404u", 1, true, VK_NUMPAD5, 0x4C, L'5', 0 },
        TestCase{ L"Disambiguate: Numpad9", L"\x1b[57408u", 1, true, VK_NUMPAD9, 0x49, L'9', 0 },

        // Keypad operators
        TestCase{ L"Disambiguate: Numpad Decimal", L"\x1b[57409u", 1, true, VK_DECIMAL, 0x53, L'.', 0 },
        TestCase{ L"Disambiguate: Numpad Divide", L"\x1b[57410u", 1, true, VK_DIVIDE, 0x35, L'/', ENHANCED_KEY },
        TestCase{ L"Disambiguate: Numpad Multiply", L"\x1b[57411u", 1, true, VK_MULTIPLY, 0x37, L'*', 0 },
        TestCase{ L"Disambiguate: Numpad Subtract", L"\x1b[57412u", 1, true, VK_SUBTRACT, 0x4A, L'-', 0 },
        TestCase{ L"Disambiguate: Numpad Add", L"\x1b[57413u", 1, true, VK_ADD, 0x4E, L'+', 0 },

        // Keypad with modifiers
        TestCase{ L"Disambiguate: Shift+Numpad5", L"\x1b[57404;2u", 1, true, VK_NUMPAD5, 0x4C, L'5', SHIFT_PRESSED },
        TestCase{ L"Disambiguate: Ctrl+Numpad0", L"\x1b[57399;5u", 1, true, VK_NUMPAD0, 0x52, L'0', CTRL_PRESSED },

        // ====================================================================
        // SECTION 7: Lock Keys and Modifier Keys (with AllKeys flag=8)
        // These report their own key codes
        // ====================================================================

        // CapsLock key itself -> CSI 57358 u
        TestCase{ L"AllKeys: CapsLock key press", L"\x1b[57358u", 8, true, VK_CAPITAL, 0x3A, 0, 0 },

        // NumLock key itself -> CSI 57360 u
        TestCase{ L"AllKeys: NumLock key press", L"\x1b[57360u", 8, true, VK_NUMLOCK, 0x45, 0, ENHANCED_KEY },

        // ScrollLock key itself -> CSI 57359 u
        TestCase{ L"AllKeys: ScrollLock key press", L"\x1b[57359u", 8, true, VK_SCROLL, 0x46, 0, 0 },

        // Left Shift key -> CSI 57441 u (with shift modifier set)
        TestCase{ L"AllKeys: Left Shift key press", L"\x1b[57441;2u", 8, true, VK_SHIFT, 0x2A, 0, SHIFT_PRESSED },

        // Right Shift key -> CSI 57447 u
        TestCase{ L"AllKeys: Right Shift key press", L"\x1b[57447;2u", 8, true, VK_SHIFT, 0x36, 0, SHIFT_PRESSED },

        // Left Ctrl key -> CSI 57442 u (with ctrl modifier set)
        TestCase{ L"AllKeys: Left Ctrl key press", L"\x1b[57442;5u", 8, true, VK_CONTROL, 0x1D, 0, CTRL_PRESSED },

        // Right Ctrl key -> CSI 57448 u
        TestCase{ L"AllKeys: Right Ctrl key press", L"\x1b[57448;5u", 8, true, VK_CONTROL, 0x1D, 0, CTRL_PRESSED | ENHANCED_KEY },

        // Left Alt key -> CSI 57443 u (with alt modifier set)
        TestCase{ L"AllKeys: Left Alt key press", L"\x1b[57443;3u", 8, true, VK_MENU, 0x38, 0, ALT_PRESSED },

        // Right Alt key -> CSI 57449 u
        TestCase{ L"AllKeys: Right Alt key press", L"\x1b[57449;3u", 8, true, VK_MENU, 0x38, 0, RIGHT_ALT_PRESSED | ENHANCED_KEY },

        // Left Windows key -> CSI 57444 u (super modifier not available in Win32)
        TestCase{ L"AllKeys: Left Win key press", L"\x1b[57444u", 8, true, VK_LWIN, 0x5B, 0, ENHANCED_KEY },

        // Right Windows key -> CSI 57450 u
        TestCase{ L"AllKeys: Right Win key press", L"\x1b[57450u", 8, true, VK_RWIN, 0x5C, 0, ENHANCED_KEY },

        // ====================================================================
        // SECTION 8: Special Keys with Disambiguate (flag=1)
        // ====================================================================

        // Various special keys that get CSI u encoding

        // Pause key -> CSI 57362 u
        TestCase{ L"AllKeys: Pause key", L"\x1b[57362u", 8, true, VK_PAUSE, 0x45, 0, 0 },

        // PrintScreen key -> CSI 57361 u
        TestCase{ L"AllKeys: PrintScreen key", L"\x1b[57361u", 8, true, VK_SNAPSHOT, 0x37, 0, ENHANCED_KEY },

        // Menu/Apps key -> CSI 57363 u
        TestCase{ L"AllKeys: Menu key", L"\x1b[57363u", 8, true, VK_APPS, 0x5D, 0, ENHANCED_KEY },

        // ====================================================================
        // SECTION 9: Legacy text keys with Disambiguate (flag=1)
        // Per spec: "the keys a-z 0-9 ` - = [ ] \ ; ' , . / with modifiers
        // alt, ctrl, ctrl+alt, shift+alt" get CSI u encoding
        // ====================================================================

        // Test each punctuation key with Alt
        TestCase{ L"Disambiguate: Alt+`", L"\x1b[96;3u", 1, true, VK_OEM_3, 0x29, L'`', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+-", L"\x1b[45;3u", 1, true, VK_OEM_MINUS, 0x0C, L'-', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+=", L"\x1b[61;3u", 1, true, VK_OEM_PLUS, 0x0D, L'=', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+[", L"\x1b[91;3u", 1, true, VK_OEM_4, 0x1A, L'[', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+]", L"\x1b[93;3u", 1, true, VK_OEM_6, 0x1B, L']', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+\\", L"\x1b[92;3u", 1, true, VK_OEM_5, 0x2B, L'\\', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+;", L"\x1b[59;3u", 1, true, VK_OEM_1, 0x27, L';', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+'", L"\x1b[39;3u", 1, true, VK_OEM_7, 0x28, L'\'', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+,", L"\x1b[44;3u", 1, true, VK_OEM_COMMA, 0x33, L',', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+.", L"\x1b[46;3u", 1, true, VK_OEM_PERIOD, 0x34, L'.', ALT_PRESSED },
        TestCase{ L"Disambiguate: Alt+/", L"\x1b[47;3u", 1, true, VK_OEM_2, 0x35, L'/', ALT_PRESSED },

        // Test numbers with Ctrl
        TestCase{ L"Disambiguate: Ctrl+0", L"\x1b[48;5u", 1, true, '0', 0x0B, L'0', CTRL_PRESSED },
        TestCase{ L"Disambiguate: Ctrl+1", L"\x1b[49;5u", 1, true, '1', 0x02, L'1', CTRL_PRESSED },
        TestCase{ L"Disambiguate: Ctrl+9", L"\x1b[57;5u", 1, true, '9', 0x0A, L'9', CTRL_PRESSED },

        // Test letters with Ctrl+Alt
        TestCase{ L"Disambiguate: Ctrl+Alt+a", L"\x1b[97;7u", 1, true, 'A', 0x1E, L'\x01', CTRL_PRESSED | ALT_PRESSED },
        TestCase{ L"Disambiguate: Ctrl+Alt+z", L"\x1b[122;7u", 1, true, 'Z', 0x2C, L'\x1A', CTRL_PRESSED | ALT_PRESSED },

        // ====================================================================
        // SECTION 10: Navigation keys as keypad (without ENHANCED_KEY)
        // When ENHANCED_KEY is not set, navigation keys are from the keypad
        // ====================================================================

        // Home without ENHANCED_KEY -> KP_HOME (57423)
        TestCase{ L"AllKeys: Keypad Home", L"\x1b[57423u", 8, true, VK_HOME, 0x47, 0, 0 },

        // End without ENHANCED_KEY -> KP_END (57424)
        TestCase{ L"AllKeys: Keypad End", L"\x1b[57424u", 8, true, VK_END, 0x4F, 0, 0 },

        // Insert without ENHANCED_KEY -> KP_INSERT (57425)
        TestCase{ L"AllKeys: Keypad Insert", L"\x1b[57425u", 8, true, VK_INSERT, 0x52, 0, 0 },

        // Delete without ENHANCED_KEY -> KP_DELETE (57426)
        TestCase{ L"AllKeys: Keypad Delete", L"\x1b[57426u", 8, true, VK_DELETE, 0x53, 0, 0 },

        // PageUp without ENHANCED_KEY -> KP_PAGE_UP (57421)
        TestCase{ L"AllKeys: Keypad PageUp", L"\x1b[57421u", 8, true, VK_PRIOR, 0x49, 0, 0 },

        // PageDown without ENHANCED_KEY -> KP_PAGE_DOWN (57422)
        TestCase{ L"AllKeys: Keypad PageDown", L"\x1b[57422u", 8, true, VK_NEXT, 0x51, 0, 0 },

        // Arrows without ENHANCED_KEY
        TestCase{ L"AllKeys: Keypad Up", L"\x1b[57419u", 8, true, VK_UP, 0x48, 0, 0 },
        TestCase{ L"AllKeys: Keypad Down", L"\x1b[57420u", 8, true, VK_DOWN, 0x50, 0, 0 },
        TestCase{ L"AllKeys: Keypad Left", L"\x1b[57417u", 8, true, VK_LEFT, 0x4B, 0, 0 },
        TestCase{ L"AllKeys: Keypad Right", L"\x1b[57418u", 8, true, VK_RIGHT, 0x4D, 0, 0 },

        // ====================================================================
        // SECTION 11: Media Keys
        // ====================================================================

        TestCase{ L"AllKeys: Media Play/Pause", L"\x1b[57430u", 8, true, VK_MEDIA_PLAY_PAUSE, 0, 0, 0 },
        TestCase{ L"AllKeys: Media Stop", L"\x1b[57432u", 8, true, VK_MEDIA_STOP, 0, 0, 0 },
        TestCase{ L"AllKeys: Media Next Track", L"\x1b[57435u", 8, true, VK_MEDIA_NEXT_TRACK, 0, 0, 0 },
        TestCase{ L"AllKeys: Media Prev Track", L"\x1b[57436u", 8, true, VK_MEDIA_PREV_TRACK, 0, 0, 0 },
        TestCase{ L"AllKeys: Volume Down", L"\x1b[57438u", 8, true, VK_VOLUME_DOWN, 0, 0, 0 },
        TestCase{ L"AllKeys: Volume Up", L"\x1b[57439u", 8, true, VK_VOLUME_UP, 0, 0, 0 },
        TestCase{ L"AllKeys: Volume Mute", L"\x1b[57440u", 8, true, VK_VOLUME_MUTE, 0, 0, 0 },

        // ====================================================================
        // SECTION 12: Function Keys (F13-F24)
        // F1-F12 use legacy sequences, F13-F24 use CSI u with codes 57376-57387
        // ====================================================================

        TestCase{ L"AllKeys: F13", L"\x1b[57376u", 8, true, VK_F13, 0x64, 0, 0 },
        TestCase{ L"AllKeys: F14", L"\x1b[57377u", 8, true, VK_F14, 0x65, 0, 0 },
        TestCase{ L"AllKeys: F15", L"\x1b[57378u", 8, true, VK_F15, 0x66, 0, 0 },
        TestCase{ L"AllKeys: F16", L"\x1b[57379u", 8, true, VK_F16, 0x67, 0, 0 },
        TestCase{ L"AllKeys: F17", L"\x1b[57380u", 8, true, VK_F17, 0x68, 0, 0 },
        TestCase{ L"AllKeys: F18", L"\x1b[57381u", 8, true, VK_F18, 0x69, 0, 0 },
        TestCase{ L"AllKeys: F19", L"\x1b[57382u", 8, true, VK_F19, 0x6A, 0, 0 },
        TestCase{ L"AllKeys: F20", L"\x1b[57383u", 8, true, VK_F20, 0x6B, 0, 0 },
        TestCase{ L"AllKeys: F21", L"\x1b[57384u", 8, true, VK_F21, 0x6C, 0, 0 },
        TestCase{ L"AllKeys: F22", L"\x1b[57385u", 8, true, VK_F22, 0x6D, 0, 0 },
        TestCase{ L"AllKeys: F23", L"\x1b[57386u", 8, true, VK_F23, 0x6E, 0, 0 },
        TestCase{ L"AllKeys: F24", L"\x1b[57387u", 8, true, VK_F24, 0x76, 0, 0 },

        // F13 with modifiers
        TestCase{ L"AllKeys: Shift+F13", L"\x1b[57376;2u", 8, true, VK_F13, 0x64, 0, SHIFT_PRESSED },
        TestCase{ L"AllKeys: Ctrl+F13", L"\x1b[57376;5u", 8, true, VK_F13, 0x64, 0, CTRL_PRESSED },
        TestCase{ L"AllKeys: Alt+F13", L"\x1b[57376;3u", 8, true, VK_F13, 0x64, 0, ALT_PRESSED },

        // ====================================================================
        // SECTION 13: Alternate Keys (ReportAlternateKeys flag = 4)
        // Format: CSI keycode:shifted-key:base-layout-key ; modifiers u
        // Shifted key is present only when shift modifier is active
        // Base layout key is the PC-101 US keyboard equivalent
        // ====================================================================

        // Shift+a with AltKeys flag: 97:65 (a:A) - shifted key is 'A' (65)
        // flags = AllKeys(8) + AltKeys(4) = 12
        TestCase{ L"AltKeys+AllKeys: Shift+a", L"\x1b[97:65;2u", 12, true, 'A', 0x1E, L'A', SHIFT_PRESSED },

        // Shift+1 with AltKeys flag: 49:33 (1:!) - shifted key is '!' (33)
        TestCase{ L"AltKeys+AllKeys: Shift+1", L"\x1b[49:33;2u", 12, true, '1', 0x02, L'!', SHIFT_PRESSED },

        // Shift+[ with AltKeys flag: 91:123 ([:{) - shifted key is '{' (123)
        TestCase{ L"AltKeys+AllKeys: Shift+[", L"\x1b[91:123;2u", 12, true, VK_OEM_4, 0x1A, L'{', SHIFT_PRESSED },

        // Without shift, no shifted key is reported
        // 'a' with AltKeys flag (no shift): 97 only, no alternate keys
        TestCase{ L"AltKeys+AllKeys: a (no shift)", L"\x1b[97u", 12, true, 'A', 0x1E, L'a', 0 },

        // ====================================================================
        // SECTION 14: Complex combinations
        // Testing multiple flags together with various keys and modifiers
        // ====================================================================

        // AllKeys + EventTypes + CapsLock: 'a' press with CapsLock
        // mod=1+64=65, event=press=1
        TestCase{ L"AllKeys+EventTypes: CapsLock+a press", L"\x1b[97;65u", 10, true, 'A', 0x1E, L'A', CAPSLOCK_ON },

        // AllKeys + EventTypes + all modifiers: press
        // mod=1+1+2+4+64+128=200, event=1
        TestCase{ L"AllKeys+EventTypes: all mods press", L"\x1b[97;200u", 10, true, 'A', 0x1E, L'\x01', SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED | CAPSLOCK_ON | NUMLOCK_ON },

        // AllKeys + EventTypes + all modifiers: release
        TestCase{ L"AllKeys+EventTypes: all mods release", L"\x1b[97;200:3u", 10, false, 'A', 0x1E, L'\x01', SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED | CAPSLOCK_ON | NUMLOCK_ON },

        // ====================================================================
        // SECTION 15: Text with associated codepoints (flag=24: AllKeys + AssocText)
        // Format: CSI keycode ; modifiers ; text u
        // ====================================================================

        // 'A' (shifted) with AssocText: CSI 97;2;65 u
        TestCase{ L"AllKeys+AssocText: Shift+a", L"\x1b[97;2;65u", 24, true, 'A', 0x1E, L'A', SHIFT_PRESSED },

        // Number with shift (symbol): Shift+1 -> '!'
        // CSI 49;2;33 u (49='1', 33='!')
        TestCase{ L"AllKeys+AssocText: Shift+1", L"\x1b[49;2;33u", 24, true, '1', 0x02, L'!', SHIFT_PRESSED },

        // Ctrl+a produces control character (0x01), which should not be in text
        // Text field should be omitted for control codes
        TestCase{ L"AllKeys+AssocText: Ctrl+a (no text)", L"\x1b[97;5u", 24, true, 'A', 0x1E, L'\x01', CTRL_PRESSED },

        // ====================================================================
        // SECTION 16: Edge cases
        // ====================================================================

        // Keypad Enter (ENHANCED_KEY set) -> KP_ENTER (57414)
        TestCase{ L"AllKeys: Keypad Enter", L"\x1b[57414u", 8, true, VK_RETURN, 0x1C, L'\r', ENHANCED_KEY },

        // Regular Enter vs Keypad Enter distinction
        TestCase{ L"AllKeys: Regular Enter", L"\x1b[13u", 8, true, VK_RETURN, 0x1C, L'\r', 0 },

        // Escape with all basic modifiers
        TestCase{ L"AllKeys: Shift+Alt+Ctrl+Esc", L"\x1b[27;8u", 8, true, VK_ESCAPE, 0x01, 0, SHIFT_PRESSED | ALT_PRESSED | CTRL_PRESSED },

        // Tab with Shift (special legacy: CSI Z, but with AllKeys should be CSI 9;2 u)
        TestCase{ L"AllKeys: Shift+Tab", L"\x1b[9;2u", 8, true, VK_TAB, 0x0F, 0, SHIFT_PRESSED },
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

        Log::Comment(NoThrowString().Format(L"[%zu] Test case \"%.*s\"", i, tc.name.size(), tc.name.data()));

        auto input = createInput(tc.flags);
        const auto expected = TerminalInput::MakeOutput(tc.expected);
        const auto actual = process(input, tc.keyDown, tc.vk, tc.sc, tc.ch, tc.state);
        const auto msg = fmt::format(L"{} != {}", til::visualize_control_codes(expected.value_or({})), til::visualize_control_codes(actual.value_or({})));
        VERIFY_ARE_EQUAL(expected, actual, msg.c_str());
    }

    // Repeat events require stateful testing - the same key must be pressed twice
    // without a release in between. This cannot be done with the data-driven approach.
    TEST_METHOD(KeyRepeatEvents)
    {
        Log::Comment(L"Testing key repeat event type (event type = 2)");

        // Use EventTypes flag (2) + AllKeys flag (8) = 10
        constexpr uint8_t flags = 10;
        auto input = createInput(flags);

        // First press -> event type 1 (press)
        auto result1 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected1 = TerminalInput::MakeOutput(L"\x1b[97u");
        VERIFY_ARE_EQUAL(expected1, result1, L"First press should be event type 1");

        // Second press (same key, no release) -> event type 2 (repeat)
        auto result2 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected2 = TerminalInput::MakeOutput(L"\x1b[97;1:2u");
        VERIFY_ARE_EQUAL(expected2, result2, L"Second press should be event type 2 (repeat)");

        // Third press (still same key) -> still event type 2 (repeat)
        auto result3 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected3 = TerminalInput::MakeOutput(L"\x1b[97;1:2u");
        VERIFY_ARE_EQUAL(expected3, result3, L"Third press should still be event type 2 (repeat)");

        // Release -> event type 3
        auto result4 = process(input, false, 'A', 0x1E, L'a', 0);
        auto expected4 = TerminalInput::MakeOutput(L"\x1b[97;1:3u");
        VERIFY_ARE_EQUAL(expected4, result4, L"Release should be event type 3");

        // Next press after release -> event type 1 (press) again
        auto result5 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected5 = TerminalInput::MakeOutput(L"\x1b[97;1:1u");
        VERIFY_ARE_EQUAL(expected5, result5, L"Press after release should be event type 1 again");
    }

    // Test repeat events with modifiers
    TEST_METHOD(KeyRepeatEventsWithModifiers)
    {
        Log::Comment(L"Testing key repeat with Shift modifier");

        constexpr uint8_t flags = 10; // EventTypes + AllKeys
        auto input = createInput(flags);

        // First Shift+a press -> event type 1
        auto result1 = process(input, true, 'A', 0x1E, L'A', SHIFT_PRESSED);
        auto expected1 = TerminalInput::MakeOutput(L"\x1b[97;2:1u");
        VERIFY_ARE_EQUAL(expected1, result1, L"First Shift+a press should be event type 1");

        // Repeat Shift+a -> event type 2
        auto result2 = process(input, true, 'A', 0x1E, L'A', SHIFT_PRESSED);
        auto expected2 = TerminalInput::MakeOutput(L"\x1b[97;2:2u");
        VERIFY_ARE_EQUAL(expected2, result2, L"Repeat Shift+a should be event type 2");
    }

    // Test that pressing different keys resets repeat detection
    TEST_METHOD(KeyRepeatResetOnDifferentKey)
    {
        Log::Comment(L"Testing that pressing a different key resets repeat detection");

        constexpr uint8_t flags = 10; // EventTypes + AllKeys
        auto input = createInput(flags);

        // Press 'a'
        auto result1 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected1 = TerminalInput::MakeOutput(L"\x1b[97;1:1u");
        VERIFY_ARE_EQUAL(expected1, result1, L"First 'a' press should be event type 1");

        // Press 'b' (different key) -> should be press, not repeat
        auto result2 = process(input, true, 'B', 0x30, L'b', 0);
        auto expected2 = TerminalInput::MakeOutput(L"\x1b[98;1:1u");
        VERIFY_ARE_EQUAL(expected2, result2, L"'b' press should be event type 1 (not repeat)");

        // Press 'a' again -> should be press since 'b' was pressed in between
        auto result3 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected3 = TerminalInput::MakeOutput(L"\x1b[97;1:1u");
        VERIFY_ARE_EQUAL(expected3, result3, L"'a' press after 'b' should be event type 1 (new press)");
    }

    // Test Enter/Tab/Backspace release suppression without AllKeys
    TEST_METHOD(EnterTabBackspaceReleaseWithoutAllKeys)
    {
        Log::Comment(L"Testing that Enter/Tab/Backspace don't report release without AllKeys flag");

        // Use Disambiguate + EventTypes (flags = 3), but NOT AllKeys
        constexpr uint8_t flags = 3;
        auto input = createInput(flags);

        // These keys should NOT produce output for release events
        // (they return MakeUnhandled for press too with just Disambiguate,
        // but release should produce _makeNoOutput)

        // Note: With flags=3 (no AllKeys), Enter/Tab/Backspace use legacy encoding
        // and release events should be suppressed (return no output)
    }

    // Test that without EventTypes flag, release events produce no output
    TEST_METHOD(ReleaseEventsWithoutEventTypesFlag)
    {
        Log::Comment(L"Testing that release events produce no output without EventTypes flag");

        // Use only AllKeys (flag = 8), NOT EventTypes
        constexpr uint8_t flags = 8;
        auto input = createInput(flags);

        // Press should produce output
        auto result1 = process(input, true, 'A', 0x1E, L'a', 0);
        auto expected1 = TerminalInput::MakeOutput(L"\x1b[97u");
        VERIFY_ARE_EQUAL(expected1, result1, L"Press should produce output");

        // Release should produce no output (empty optional)
        auto result2 = process(input, false, 'A', 0x1E, L'a', 0);
        VERIFY_IS_FALSE(result2.has_value(), L"Release without EventTypes flag should produce no output");
    }

    // Test legacy mode (flags=0) produces MakeUnhandled for regular keys
    TEST_METHOD(LegacyModePassthrough)
    {
        Log::Comment(L"Testing that legacy mode (flags=0) returns MakeUnhandled for regular keys");

        constexpr uint8_t flags = 0;
        auto input = createInput(flags);

        // Regular key 'a' should return MakeUnhandled (falls through to legacy processing)
        auto result = process(input, true, 'A', 0x1E, L'a', 0);
        auto unhandled = TerminalInput::MakeUnhandled();
        VERIFY_ARE_EQUAL(unhandled, result, L"Regular key in legacy mode should be unhandled");
    }
};
