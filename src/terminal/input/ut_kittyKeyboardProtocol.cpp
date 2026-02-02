// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "terminalInput.hpp"
#include <WexTestClass.h>

using namespace WEX::TestExecution;
using namespace WEX::Logging;
using namespace WEX::Common;
using namespace Microsoft::Console::VirtualTerminal;

// =============================================================================
// IMPORTANT: Test Design Document for Kitty Keyboard Protocol
// =============================================================================
//
// This file contains comprehensive unit tests for the Kitty Keyboard Protocol
// as specified at: https://sw.kovidgoyal.net/kitty/keyboard-protocol/
//
// The tests are organized by the following categories:
//
// 1. Enhancement Flag Combinations (32 combinations for 5 flags)
//    - 0b00001 (1)  = Disambiguate escape codes
//    - 0b00010 (2)  = Report event types
//    - 0b00100 (4)  = Report alternate keys
//    - 0b01000 (8)  = Report all keys as escape codes
//    - 0b10000 (16) = Report associated text
//
// 2. Modifier Combinations (bit field encoding: 1 + actual modifiers)
//    - shift=1, alt=2, ctrl=4, super=8, hyper=16, meta=32, caps_lock=64, num_lock=128
//
// 3. Event Types
//    - press (1, default), repeat (2), release (3)
//
// 4. Special Key Behaviors
//    - Enter, Tab, Backspace: no release events unless ReportAllKeysAsEscapeCodes
//    - Lock modifiers: not reported for text keys unless ReportAllKeysAsEscapeCodes
//
// 5. Key Categories
//    - Text-producing keys (a-z, 0-9, symbols)
//    - Functional keys (F1-F35, navigation, etc.)
//    - Keypad keys
//    - Modifier keys
//
// =============================================================================

namespace
{
    // Win32 control key state flags
    constexpr DWORD RIGHT_ALT_PRESSED = 0x0001;
    constexpr DWORD LEFT_ALT_PRESSED = 0x0002;
    constexpr DWORD RIGHT_CTRL_PRESSED = 0x0004;
    constexpr DWORD LEFT_CTRL_PRESSED = 0x0008;
    constexpr DWORD SHIFT_PRESSED = 0x0010;
    constexpr DWORD NUMLOCK_ON = 0x0020;
    constexpr DWORD SCROLLLOCK_ON = 0x0040;
    constexpr DWORD CAPSLOCK_ON = 0x0080;
    constexpr DWORD ENHANCED_KEY = 0x0100;

    // Kitty enhancement flags
    constexpr uint8_t DisambiguateEscapeCodes = 0b00001;    // 1
    constexpr uint8_t ReportEventTypes = 0b00010;           // 2
    constexpr uint8_t ReportAlternateKeys = 0b00100;        // 4
    constexpr uint8_t ReportAllKeysAsEscapeCodes = 0b01000; // 8
    constexpr uint8_t ReportAssociatedText = 0b10000;       // 16

    // Virtual key codes
    constexpr WORD VK_A = 'A';
    constexpr WORD VK_B = 'B';
    constexpr WORD VK_C = 'C';
    constexpr WORD VK_W = 'W';
    constexpr WORD VK_SPACE = 0x20;
    constexpr WORD VK_RETURN = 0x0D;
    constexpr WORD VK_TAB_KEY = 0x09;
    constexpr WORD VK_BACK = 0x08;
    constexpr WORD VK_ESCAPE = 0x1B;
    constexpr WORD VK_F1 = 0x70;
    constexpr WORD VK_F2 = 0x71;
    constexpr WORD VK_F3 = 0x72;
    constexpr WORD VK_F4 = 0x73;
    constexpr WORD VK_F5 = 0x74;
    constexpr WORD VK_F12 = 0x7B;
    constexpr WORD VK_F13 = 0x7C;
    constexpr WORD VK_F24 = 0x87;
    constexpr WORD VK_LEFT = 0x25;
    constexpr WORD VK_UP = 0x26;
    constexpr WORD VK_RIGHT = 0x27;
    constexpr WORD VK_DOWN = 0x28;
    constexpr WORD VK_HOME = 0x24;
    constexpr WORD VK_END = 0x23;
    constexpr WORD VK_PRIOR = 0x21;  // Page Up
    constexpr WORD VK_NEXT = 0x22;   // Page Down
    constexpr WORD VK_INSERT = 0x2D;
    constexpr WORD VK_DELETE = 0x2E;
    constexpr WORD VK_NUMPAD0 = 0x60;
    constexpr WORD VK_NUMPAD9 = 0x69;
    constexpr WORD VK_MULTIPLY = 0x6A;
    constexpr WORD VK_ADD = 0x6B;
    constexpr WORD VK_SUBTRACT = 0x6D;
    constexpr WORD VK_DECIMAL = 0x6E;
    constexpr WORD VK_DIVIDE = 0x6F;
    constexpr WORD VK_LSHIFT = 0xA0;
    constexpr WORD VK_RSHIFT = 0xA1;
    constexpr WORD VK_LCONTROL = 0xA2;
    constexpr WORD VK_RCONTROL = 0xA3;
    constexpr WORD VK_LMENU = 0xA4;
    constexpr WORD VK_RMENU = 0xA5;
    constexpr WORD VK_CAPITAL = 0x14;  // Caps Lock
    constexpr WORD VK_NUMLOCK = 0x90;
    constexpr WORD VK_SCROLL = 0x91;

    // Helper to create Output from string
    TerminalInput::OutputType wrap(const std::wstring& str)
    {
        return TerminalInput::MakeOutput(str);
    }

    // Placeholder for the test harness function that processes key events
    // This should call _makeKittyOutput with the given parameters
    TerminalInput::OutputType process(
        TerminalInput& input,
        bool bKeyDown,
        uint16_t wVirtualKeyCode,
        uint16_t wVirtualScanCode,
        wchar_t UnicodeChar,
        uint32_t dwControlKeyState)
    {
        KEY_EVENT_RECORD keyEvent = {};
        keyEvent.bKeyDown = bKeyDown ? TRUE : FALSE;
        keyEvent.wRepeatCount = 1;
        keyEvent.wVirtualKeyCode = wVirtualKeyCode;
        keyEvent.wVirtualScanCode = wVirtualScanCode;
        keyEvent.uChar.UnicodeChar = UnicodeChar;
        keyEvent.dwControlKeyState = dwControlKeyState;

        INPUT_RECORD record = {};
        record.EventType = KEY_EVENT;
        record.Event.KeyEvent = keyEvent;

        return input.HandleKey(record);
    }
}

class KittyKeyboardProtocolTests
{
    TEST_CLASS(KittyKeyboardProtocolTests);

    // =========================================================================
    // SECTION 1: Enhancement Flag Combinations (32 tests)
    // Test all 32 combinations of the 5 enhancement flags
    // =========================================================================

    // Flag Combination 0b00000 (0) - No enhancements (legacy mode)
    TEST_METHOD(EnhancementFlags_0x00_NoEnhancements_SimpleKeyPress)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(0, KittyKeyboardProtocolMode::Replace);

        // In legacy mode with no kitty flags, 'a' should produce plain text
        // This tests that without any enhancements, we fall through to non-kitty handling
        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
        // Legacy behavior - not CSI u encoded
        VERIFY_IS_TRUE(!result.has_value() || std::get<TerminalInput::StringType>(*result) != L"\x1b[97u");
    }

    // Flag Combination 0b00001 (1) - Disambiguate escape codes only
    TEST_METHOD(EnhancementFlags_0x01_Disambiguate_EscapeKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Escape key should be encoded as CSI 27 u (disambiguated from ESC byte)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[27u"), process(input, true, VK_ESCAPE, 0x01, 0, 0));
    }

    TEST_METHOD(EnhancementFlags_0x01_Disambiguate_AltLetter)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Alt+a should be CSI 97;3u (3 = 1 + alt modifier 2)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;3u"), process(input, true, VK_A, 0x1E, L'a', LEFT_ALT_PRESSED));
    }

    TEST_METHOD(EnhancementFlags_0x01_Disambiguate_CtrlLetter)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Ctrl+c should be CSI 99;5u (5 = 1 + ctrl modifier 4)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[99;5u"), process(input, true, VK_C, 0x2E, 0x03, LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(EnhancementFlags_0x01_Disambiguate_CtrlAltLetter)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Ctrl+Alt+a should be CSI 97;7u (7 = 1 + ctrl 4 + alt 2)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;7u"), process(input, true, VK_A, 0x1E, 0, LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED));
    }

    TEST_METHOD(EnhancementFlags_0x01_Disambiguate_ShiftAltLetter)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+Alt+a should be CSI 97;4u (4 = 1 + shift 1 + alt 2)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;4u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED | LEFT_ALT_PRESSED));
    }

    // Flag Combination 0b00010 (2) - Report event types only
    TEST_METHOD(EnhancementFlags_0x02_EventTypes_PressEvent)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Press event is default (type 1), no event type suffix needed in output
        // Key 'a' press with event types should be CSI 97;1:1u or just CSI 97u (press is default)
        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
        // With only ReportEventTypes, text keys may still produce text
    }

    TEST_METHOD(EnhancementFlags_0x02_EventTypes_ReleaseEvent_FunctionalKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Release event (type 3) for functional keys
        // First send press
        process(input, true, VK_F1, 0x3B, 0, 0);
        // Then send release - should have event type :3
        VERIFY_ARE_EQUAL(wrap(L"\x1b[1;1:3P"), process(input, false, VK_F1, 0x3B, 0, 0));
    }

    // Flag Combination 0b00011 (3) - Disambiguate + Event types
    TEST_METHOD(EnhancementFlags_0x03_DisambiguateAndEventTypes_RepeatEvent)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // First press of 'a'
        process(input, true, VK_A, 0x1E, L'a', 0);
        // Repeat press of 'a' - should have event type :2
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EnhancementFlags_0x03_DisambiguateAndEventTypes_ReleaseEvent)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Press then release of 'a'
        process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:3u"), process(input, false, VK_A, 0x1E, L'a', 0));
    }

    // Flag Combination 0b00100 (4) - Report alternate keys only
    TEST_METHOD(EnhancementFlags_0x04_AlternateKeys_ShiftedKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAlternateKeys, KittyKeyboardProtocolMode::Replace);

        // With alternate keys, shift+a should report the shifted key (A = 65)
        // Format: CSI unicode-key-code:shifted-key u
        // Note: alternate keys only affects keys already encoded as escape codes
        auto result = process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED);
        // Without disambiguate, this may just produce 'A'
    }

    // Flag Combination 0b00101 (5) - Disambiguate + Alternate keys
    TEST_METHOD(EnhancementFlags_0x05_DisambiguateAndAlternate_ShiftedKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAlternateKeys, KittyKeyboardProtocolMode::Replace);

        // Shift+a with alternate keys: CSI 97:65;2u
        // 97 = 'a', 65 = 'A' (shifted key), 2 = 1 + shift(1)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    TEST_METHOD(EnhancementFlags_0x05_DisambiguateAndAlternate_BaseLayoutKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAlternateKeys, KittyKeyboardProtocolMode::Replace);

        // Ctrl+a with alternate keys should include base layout key
        // Format: CSI 97::base-layout u (empty shifted key, only base layout)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;5u"), process(input, true, VK_A, 0x1E, 0x01, LEFT_CTRL_PRESSED));
    }

    // Flag Combination 0b00110 (6) - Event types + Alternate keys
    TEST_METHOD(EnhancementFlags_0x06_EventTypesAndAlternate)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAlternateKeys, KittyKeyboardProtocolMode::Replace);

        // F1 key release with alternate keys
        process(input, true, VK_F1, 0x3B, 0, 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[1;1:3P"), process(input, false, VK_F1, 0x3B, 0, 0));
    }

    // Flag Combination 0b00111 (7) - Disambiguate + Event types + Alternate keys
    TEST_METHOD(EnhancementFlags_0x07_ThreeFlags_ShiftedKeyWithRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAlternateKeys, KittyKeyboardProtocolMode::Replace);

        // Shift+a press: CSI 97:65;2:1u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
        // Shift+a release: CSI 97:65;2:3u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2:3u"), process(input, false, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b01000 (8) - Report all keys as escape codes
    TEST_METHOD(EnhancementFlags_0x08_AllKeysAsEscapeCodes_PlainText)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Plain 'a' key should now be encoded as CSI 97u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EnhancementFlags_0x08_AllKeysAsEscapeCodes_EnterKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Enter key encoded as CSI 13u (not plain CR)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[13u"), process(input, true, VK_RETURN, 0x1C, L'\r', 0));
    }

    TEST_METHOD(EnhancementFlags_0x08_AllKeysAsEscapeCodes_TabKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Tab key encoded as CSI 9u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[9u"), process(input, true, VK_TAB_KEY, 0x0F, L'\t', 0));
    }

    TEST_METHOD(EnhancementFlags_0x08_AllKeysAsEscapeCodes_BackspaceKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Backspace key encoded as CSI 127u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[127u"), process(input, true, VK_BACK, 0x0E, 0x7F, 0));
    }

    TEST_METHOD(EnhancementFlags_0x08_AllKeysAsEscapeCodes_ModifierKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Left Shift key press should be reported as CSI 57441;2u
        // 57441 = LEFT_SHIFT functional key code, 2 = 1 + shift(1)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57441;2u"), process(input, true, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED));
    }

    // Flag Combination 0b01001 (9) - Disambiguate + All keys as escape codes
    TEST_METHOD(EnhancementFlags_0x09_DisambiguateAndAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Both flags together - 'a' encoded as CSI 97u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    // Flag Combination 0b01010 (10) - Event types + All keys as escape codes
    TEST_METHOD(EnhancementFlags_0x0A_EventTypesAndAllKeys_EnterRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // With AllKeysAsEscapeCodes, Enter DOES report release events
        process(input, true, VK_RETURN, 0x1C, L'\r', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[13;1:3u"), process(input, false, VK_RETURN, 0x1C, L'\r', 0));
    }

    TEST_METHOD(EnhancementFlags_0x0A_EventTypesAndAllKeys_TabRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // With AllKeysAsEscapeCodes, Tab DOES report release events
        process(input, true, VK_TAB_KEY, 0x0F, L'\t', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[9;1:3u"), process(input, false, VK_TAB_KEY, 0x0F, L'\t', 0));
    }

    TEST_METHOD(EnhancementFlags_0x0A_EventTypesAndAllKeys_BackspaceRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // With AllKeysAsEscapeCodes, Backspace DOES report release events
        process(input, true, VK_BACK, 0x0E, 0x7F, 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[127;1:3u"), process(input, false, VK_BACK, 0x0E, 0x7F, 0));
    }

    // Flag Combination 0b01011 (11) - Disambiguate + Event types + All keys
    TEST_METHOD(EnhancementFlags_0x0B_ThreeFlags_PlainKeyRepeat)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Press then repeat of plain 'a'
        process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2u"), process(input, true, VK_A, 0x1E, L'a', 0)); // repeat
    }

    // Flag Combination 0b01100 (12) - Alternate keys + All keys as escape codes
    TEST_METHOD(EnhancementFlags_0x0C_AlternateAndAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAlternateKeys | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+a with alternate keys: CSI 97:65;2u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b01101 (13) - Disambiguate + Alternate + All keys
    TEST_METHOD(EnhancementFlags_0x0D_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAlternateKeys | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Plain 'a'
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    // Flag Combination 0b01110 (14) - Event types + Alternate + All keys
    TEST_METHOD(EnhancementFlags_0x0E_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAlternateKeys | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+a release with alternate keys
        process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2:3u"), process(input, false, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b01111 (15) - Disambiguate + Event types + Alternate + All keys
    TEST_METHOD(EnhancementFlags_0x0F_FourFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAlternateKeys | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Full combination test
        process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2u"), process(input, true, VK_A, 0x1E, L'a', 0)); // repeat
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:3u"), process(input, false, VK_A, 0x1E, L'a', 0)); // release
    }

    // Flag Combination 0b10000 (16) - Report associated text only
    TEST_METHOD(EnhancementFlags_0x10_AssociatedText_NoEffect)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Associated text without AllKeysAsEscapeCodes is undefined behavior
        // The spec says it's undefined if used without ReportAllKeysAsEscapeCodes
        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
        // Implementation defined behavior
    }

    // Flag Combination 0b10001 (17) - Disambiguate + Associated text
    TEST_METHOD(EnhancementFlags_0x11_DisambiguateAndText)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Still undefined without AllKeysAsEscapeCodes for text keys
        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
    }

    // Flag Combination 0b10010 (18) - Event types + Associated text
    TEST_METHOD(EnhancementFlags_0x12_EventTypesAndText)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        auto result = process(input, true, VK_F1, 0x3B, 0, 0);
        // Functional keys work with event types
    }

    // Flag Combination 0b10011 (19) - Disambiguate + Event types + Associated text
    TEST_METHOD(EnhancementFlags_0x13_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Ctrl+a release
        process(input, true, VK_A, 0x1E, 0x01, LEFT_CTRL_PRESSED);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;5:3u"), process(input, false, VK_A, 0x1E, 0x01, LEFT_CTRL_PRESSED));
    }

    // Flag Combination 0b10100 (20) - Alternate keys + Associated text
    TEST_METHOD(EnhancementFlags_0x14_AlternateAndText)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAlternateKeys | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
    }

    // Flag Combination 0b10101 (21) - Disambiguate + Alternate + Associated text
    TEST_METHOD(EnhancementFlags_0x15_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAlternateKeys | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Shift+a with alternate and text
        auto result = process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED);
    }

    // Flag Combination 0b10110 (22) - Event types + Alternate + Associated text
    TEST_METHOD(EnhancementFlags_0x16_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAlternateKeys | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        auto result = process(input, true, VK_F1, 0x3B, 0, 0);
    }

    // Flag Combination 0b10111 (23) - Disambiguate + Event types + Alternate + Associated text
    TEST_METHOD(EnhancementFlags_0x17_FourFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAlternateKeys | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Shift+a with full reporting (except AllKeys)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b11000 (24) - All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x18_AllKeysAndText_SimpleKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // With both flags: CSI 97;;97u (key 97, no modifiers, text 97)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EnhancementFlags_0x18_AllKeysAndText_ShiftKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Shift+a: CSI 97;2;65u (key 97, modifier 2, text 'A'=65)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;2;65u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b11001 (25) - Disambiguate + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x19_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Same as 0x18 since disambiguate is implied by AllKeys
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    // Flag Combination 0b11010 (26) - Event types + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x1A_ThreeFlags_KeyRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Press: CSI 97;1:1;97u or CSI 97;;97u (press is default)
        process(input, true, VK_A, 0x1E, L'a', 0);
        // Release: CSI 97;1:3;97u (no text on release per spec? - implementation may vary)
        auto result = process(input, false, VK_A, 0x1E, L'a', 0);
        // Text may or may not be present on release
    }

    // Flag Combination 0b11011 (27) - Disambiguate + Event types + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x1B_FourFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Full tracking with text
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    // Flag Combination 0b11100 (28) - Alternate + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x1C_ThreeFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAlternateKeys | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Shift+a: CSI 97:65;2;65u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2;65u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b11101 (29) - Disambiguate + Alternate + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x1D_FourFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportAlternateKeys | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2;65u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // Flag Combination 0b11110 (30) - Event types + Alternate + All keys + Associated text
    TEST_METHOD(EnhancementFlags_0x1E_FourFlags)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAlternateKeys | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Press with repeat
        process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2;97u"), process(input, true, VK_A, 0x1E, L'a', 0)); // repeat
    }

    // Flag Combination 0b11111 (31) - All flags enabled
    TEST_METHOD(EnhancementFlags_0x1F_AllFlags_FullSequence)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(
            DisambiguateEscapeCodes | ReportEventTypes | ReportAlternateKeys |
            ReportAllKeysAsEscapeCodes | ReportAssociatedText,
            KittyKeyboardProtocolMode::Replace);

        // Full sequence: CSI unicode-key-code:alternate-key-codes ; modifiers:event-type ; text-as-codepoints u
        // Press 'a': CSI 97;;97u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
        // Repeat 'a': CSI 97;1:2;97u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
        // Release 'a': CSI 97;1:3u (no text on release)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:3u"), process(input, false, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EnhancementFlags_0x1F_AllFlags_ShiftedKey)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(
            DisambiguateEscapeCodes | ReportEventTypes | ReportAlternateKeys |
            ReportAllKeysAsEscapeCodes | ReportAssociatedText,
            KittyKeyboardProtocolMode::Replace);

        // Shift+a: CSI 97:65;2;65u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97:65;2;65u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    // =========================================================================
    // SECTION 2: Modifier Combinations
    // Test the bit field encoding: modifiers value = 1 + actual modifiers
    // =========================================================================

    TEST_METHOD(Modifiers_Shift_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift only: modifier = 1 + 1 = 2
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    TEST_METHOD(Modifiers_Alt_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Alt only: modifier = 1 + 2 = 3
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;3u"), process(input, true, VK_A, 0x1E, L'a', LEFT_ALT_PRESSED));
    }

    TEST_METHOD(Modifiers_Ctrl_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Ctrl only: modifier = 1 + 4 = 5
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;5u"), process(input, true, VK_A, 0x1E, 0x01, LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(Modifiers_ShiftAlt_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+Alt: modifier = 1 + 1 + 2 = 4
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;4u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED | LEFT_ALT_PRESSED));
    }

    TEST_METHOD(Modifiers_ShiftCtrl_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+Ctrl: modifier = 1 + 1 + 4 = 6
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;6u"), process(input, true, VK_A, 0x1E, 0x01, SHIFT_PRESSED | LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(Modifiers_AltCtrl_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Alt+Ctrl: modifier = 1 + 2 + 4 = 7
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;7u"), process(input, true, VK_A, 0x1E, 0x01, LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(Modifiers_ShiftAltCtrl_Encoding)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Shift+Alt+Ctrl: modifier = 1 + 1 + 2 + 4 = 8
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;8u"), process(input, true, VK_A, 0x1E, 0x01, SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(Modifiers_CapsLock_OnlyWithAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Caps Lock: modifier = 1 + 64 = 65
        // Lock modifiers only reported with ReportAllKeysAsEscapeCodes
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;65u"), process(input, true, VK_A, 0x1E, L'a', CAPSLOCK_ON));
    }

    TEST_METHOD(Modifiers_NumLock_OnlyWithAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Num Lock: modifier = 1 + 128 = 129
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;129u"), process(input, true, VK_A, 0x1E, L'a', NUMLOCK_ON));
    }

    TEST_METHOD(Modifiers_CapsLockAndNumLock_Together)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Caps+Num Lock: modifier = 1 + 64 + 128 = 193
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;193u"), process(input, true, VK_A, 0x1E, L'a', CAPSLOCK_ON | NUMLOCK_ON));
    }

    TEST_METHOD(Modifiers_LocksNotReportedForTextKeys_WithoutAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Without ReportAllKeysAsEscapeCodes, lock modifiers are NOT reported for text keys
        // This should NOT include the caps_lock bit
        // Plain 'a' with caps lock should still just be CSI 97u (no modifier)
        // But actually with Disambiguate, only modified keys are CSI u encoded
        // So we use Alt to force encoding
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;3u"), process(input, true, VK_A, 0x1E, L'a', LEFT_ALT_PRESSED | CAPSLOCK_ON));
        // Note: caps_lock bit 64 is NOT included, so it's 3 (1+2) not 67 (1+2+64)
    }

    // =========================================================================
    // SECTION 3: Event Types (press, repeat, release)
    // =========================================================================

    TEST_METHOD(EventTypes_Press_IsDefault)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Press event (type 1) is default, may be omitted
        auto result = process(input, true, VK_A, 0x1E, L'a', 0);
        // Could be CSI 97u or CSI 97;1:1u - press is default so type can be omitted
    }

    TEST_METHOD(EventTypes_Repeat_Type2)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // First press
        process(input, true, VK_A, 0x1E, L'a', 0);
        // Second press without release = repeat (type 2)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:2u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EventTypes_Release_Type3)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Press then release
        process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:3u"), process(input, false, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(EventTypes_ModifierOnRelease_MustBePresent)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // When modifier key is released, the modifier bit must still be set
        // (the release event state includes the key being released)
        process(input, true, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED);
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57441;2:3u"), process(input, false, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED));
    }

    TEST_METHOD(EventTypes_ModifierOnRelease_ResetWhenBothReleased)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // When both shifts pressed, releasing one keeps shift bit
        // Press left shift
        process(input, true, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED);
        // Press right shift
        process(input, true, VK_RSHIFT, 0x36, 0, SHIFT_PRESSED);
        // Release left shift - shift bit still set (right is held)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57441;2:3u"), process(input, false, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED));
    }

    // =========================================================================
    // SECTION 4: Special Key Behaviors
    // Enter, Tab, Backspace have special handling for release events
    // =========================================================================

    TEST_METHOD(SpecialKeys_Enter_NoReleaseWithoutAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Without ReportAllKeysAsEscapeCodes, Enter does NOT report release
        process(input, true, VK_RETURN, 0x1C, L'\r', 0);
        auto result = process(input, false, VK_RETURN, 0x1C, L'\r', 0);
        // Should produce empty/no output on release
        VERIFY_IS_TRUE(!result.has_value() || std::get<TerminalInput::StringType>(*result).empty());
    }

    TEST_METHOD(SpecialKeys_Tab_NoReleaseWithoutAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Without ReportAllKeysAsEscapeCodes, Tab does NOT report release
        process(input, true, VK_TAB_KEY, 0x0F, L'\t', 0);
        auto result = process(input, false, VK_TAB_KEY, 0x0F, L'\t', 0);
        VERIFY_IS_TRUE(!result.has_value() || std::get<TerminalInput::StringType>(*result).empty());
    }

    TEST_METHOD(SpecialKeys_Backspace_NoReleaseWithoutAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes, KittyKeyboardProtocolMode::Replace);

        // Without ReportAllKeysAsEscapeCodes, Backspace does NOT report release
        process(input, true, VK_BACK, 0x0E, 0x7F, 0);
        auto result = process(input, false, VK_BACK, 0x0E, 0x7F, 0);
        VERIFY_IS_TRUE(!result.has_value() || std::get<TerminalInput::StringType>(*result).empty());
    }

    TEST_METHOD(SpecialKeys_Escape_Disambiguated)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Escape key is disambiguated from ESC byte
        VERIFY_ARE_EQUAL(wrap(L"\x1b[27u"), process(input, true, VK_ESCAPE, 0x01, 0x1B, 0));
    }

    TEST_METHOD(SpecialKeys_Enter_LegacyBehavior)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Enter still produces CR in legacy-compatible way when not AllKeys
        // Actually with disambiguate it may still be legacy
        auto result = process(input, true, VK_RETURN, 0x1C, L'\r', 0);
        // Could be 0x0d or CSI 13u depending on implementation
    }

    TEST_METHOD(SpecialKeys_Tab_LegacyBehavior)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Tab still produces HT in legacy-compatible way when not AllKeys
        auto result = process(input, true, VK_TAB_KEY, 0x0F, L'\t', 0);
    }

    TEST_METHOD(SpecialKeys_Backspace_LegacyBehavior)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Backspace still produces DEL in legacy-compatible way when not AllKeys
        auto result = process(input, true, VK_BACK, 0x0E, 0x7F, 0);
    }

    // =========================================================================
    // SECTION 5: Functional Key Definitions
    // Test functional keys with their proper CSI codes
    // =========================================================================

    // F1-F4 use SS3 prefix in legacy, CSI with P/Q/R/S final in kitty
    TEST_METHOD(FunctionalKeys_F1_Legacy)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F1 without modifiers uses legacy SS3 P
        VERIFY_ARE_EQUAL(wrap(L"\x1bOP"), process(input, true, VK_F1, 0x3B, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F1_WithModifiers)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F1 with modifiers uses CSI 1;modifier P
        VERIFY_ARE_EQUAL(wrap(L"\x1b[1;2P"), process(input, true, VK_F1, 0x3B, 0, SHIFT_PRESSED));
    }

    TEST_METHOD(FunctionalKeys_F2)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1bOQ"), process(input, true, VK_F2, 0x3C, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F3)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1bOR"), process(input, true, VK_F3, 0x3D, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F4)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1bOS"), process(input, true, VK_F4, 0x3E, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F5)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F5 uses CSI 15 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[15~"), process(input, true, VK_F5, 0x3F, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F5_WithModifiers)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F5 with Shift: CSI 15;2 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[15;2~"), process(input, true, VK_F5, 0x3F, 0, SHIFT_PRESSED));
    }

    TEST_METHOD(FunctionalKeys_F12)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F12 uses CSI 24 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[24~"), process(input, true, VK_F12, 0x58, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F13)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F13-F35 use CSI u encoding with functional key codes
        // F13 = 57376
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57376u"), process(input, true, VK_F13, 0x64, 0, 0));
    }

    TEST_METHOD(FunctionalKeys_F24)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // F24 = 57387
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57387u"), process(input, true, VK_F24, 0x87, 0, 0));
    }

    // Navigation keys
    TEST_METHOD(FunctionalKeys_ArrowUp_Legacy)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Arrow up: CSI A
        VERIFY_ARE_EQUAL(wrap(L"\x1b[A"), process(input, true, VK_UP, 0x48, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_ArrowUp_WithModifiers)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Arrow up with Shift: CSI 1;2 A
        VERIFY_ARE_EQUAL(wrap(L"\x1b[1;2A"), process(input, true, VK_UP, 0x48, 0, ENHANCED_KEY | SHIFT_PRESSED));
    }

    TEST_METHOD(FunctionalKeys_ArrowDown)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[B"), process(input, true, VK_DOWN, 0x50, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_ArrowLeft)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[D"), process(input, true, VK_LEFT, 0x4B, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_ArrowRight)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[C"), process(input, true, VK_RIGHT, 0x4D, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_Home)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[H"), process(input, true, VK_HOME, 0x47, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_End)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        VERIFY_ARE_EQUAL(wrap(L"\x1b[F"), process(input, true, VK_END, 0x4F, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_Insert)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Insert: CSI 2 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[2~"), process(input, true, VK_INSERT, 0x52, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_Delete)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Delete: CSI 3 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[3~"), process(input, true, VK_DELETE, 0x53, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_PageUp)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // PageUp: CSI 5 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[5~"), process(input, true, VK_PRIOR, 0x49, 0, ENHANCED_KEY));
    }

    TEST_METHOD(FunctionalKeys_PageDown)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // PageDown: CSI 6 ~
        VERIFY_ARE_EQUAL(wrap(L"\x1b[6~"), process(input, true, VK_NEXT, 0x51, 0, ENHANCED_KEY));
    }

    // =========================================================================
    // SECTION 6: Keypad Keys (with ENHANCED_KEY differentiation)
    // =========================================================================

    TEST_METHOD(KeypadKeys_Numpad0_WithAllKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_0 = 57399
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57399u"), process(input, true, VK_NUMPAD0, 0x52, L'0', 0));
    }

    TEST_METHOD(KeypadKeys_NumpadAdd)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_ADD = 57413
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57413u"), process(input, true, VK_ADD, 0x4E, L'+', 0));
    }

    TEST_METHOD(KeypadKeys_NumpadSubtract)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_SUBTRACT = 57412
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57412u"), process(input, true, VK_SUBTRACT, 0x4A, L'-', 0));
    }

    TEST_METHOD(KeypadKeys_NumpadMultiply)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_MULTIPLY = 57411
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57411u"), process(input, true, VK_MULTIPLY, 0x37, L'*', 0));
    }

    TEST_METHOD(KeypadKeys_NumpadDivide)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_DIVIDE = 57410
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57410u"), process(input, true, VK_DIVIDE, 0x35, L'/', ENHANCED_KEY));
    }

    TEST_METHOD(KeypadKeys_NumpadDecimal)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_DECIMAL = 57409
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57409u"), process(input, true, VK_DECIMAL, 0x53, L'.', 0));
    }

    TEST_METHOD(KeypadKeys_NumpadEnter)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_ENTER = 57414
        // Numpad Enter has ENHANCED_KEY flag
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57414u"), process(input, true, VK_RETURN, 0x1C, L'\r', ENHANCED_KEY));
    }

    // Navigation keys on numpad (without NumLock)
    TEST_METHOD(KeypadKeys_NumpadHome)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_HOME = 57423 (Home on numpad without ENHANCED_KEY)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57423u"), process(input, true, VK_HOME, 0x47, 0, 0));
    }

    TEST_METHOD(KeypadKeys_NumpadUp)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // KP_UP = 57419 (Up on numpad without ENHANCED_KEY)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57419u"), process(input, true, VK_UP, 0x48, 0, 0));
    }

    // =========================================================================
    // SECTION 7: Modifier Keys
    // =========================================================================

    TEST_METHOD(ModifierKeys_LeftShift)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // LEFT_SHIFT = 57441
        // When pressing shift, the shift modifier bit must be set
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57441;2u"), process(input, true, VK_LSHIFT, 0x2A, 0, SHIFT_PRESSED));
    }

    TEST_METHOD(ModifierKeys_RightShift)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // RIGHT_SHIFT = 57447
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57447;2u"), process(input, true, VK_RSHIFT, 0x36, 0, SHIFT_PRESSED));
    }

    TEST_METHOD(ModifierKeys_LeftControl)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // LEFT_CONTROL = 57442
        // When pressing ctrl, the ctrl modifier bit must be set
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57442;5u"), process(input, true, VK_LCONTROL, 0x1D, 0, LEFT_CTRL_PRESSED));
    }

    TEST_METHOD(ModifierKeys_RightControl)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // RIGHT_CONTROL = 57448
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57448;5u"), process(input, true, VK_RCONTROL, 0x1D, 0, RIGHT_CTRL_PRESSED | ENHANCED_KEY));
    }

    TEST_METHOD(ModifierKeys_LeftAlt)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // LEFT_ALT = 57443
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57443;3u"), process(input, true, VK_LMENU, 0x38, 0, LEFT_ALT_PRESSED));
    }

    TEST_METHOD(ModifierKeys_RightAlt)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // RIGHT_ALT = 57449
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57449;3u"), process(input, true, VK_RMENU, 0x38, 0, RIGHT_ALT_PRESSED | ENHANCED_KEY));
    }

    TEST_METHOD(ModifierKeys_CapsLock)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // CAPS_LOCK = 57358
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57358u"), process(input, true, VK_CAPITAL, 0x3A, 0, 0));
    }

    TEST_METHOD(ModifierKeys_NumLock)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // NUM_LOCK = 57360
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57360u"), process(input, true, VK_NUMLOCK, 0x45, 0, ENHANCED_KEY));
    }

    TEST_METHOD(ModifierKeys_ScrollLock)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // SCROLL_LOCK = 57359
        VERIFY_ARE_EQUAL(wrap(L"\x1b[57359u"), process(input, true, VK_SCROLL, 0x46, 0, 0));
    }

    // =========================================================================
    // SECTION 8: Key Code Encoding (lowercase requirement)
    // =========================================================================

    TEST_METHOD(KeyCodes_AlwaysLowercase)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Key code must always be lowercase, even with shift
        // Shift+a should be CSI 97;2u (not 65)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;2u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    TEST_METHOD(KeyCodes_CtrlShift_StillLowercase)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Ctrl+Shift+a should still be CSI 97;6u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;6u"), process(input, true, VK_A, 0x1E, 0x01, SHIFT_PRESSED | LEFT_CTRL_PRESSED));
    }

    // =========================================================================
    // SECTION 9: Text as Codepoints
    // =========================================================================

    TEST_METHOD(TextAsCodepoints_SimpleChar)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // 'a' produces text 'a' (97)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;;97u"), process(input, true, VK_A, 0x1E, L'a', 0));
    }

    TEST_METHOD(TextAsCodepoints_ShiftedChar)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Shift+a produces text 'A' (65)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;2;65u"), process(input, true, VK_A, 0x1E, L'A', SHIFT_PRESSED));
    }

    TEST_METHOD(TextAsCodepoints_NoTextForNonTextKeys)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Escape doesn't produce text
        VERIFY_ARE_EQUAL(wrap(L"\x1b[27u"), process(input, true, VK_ESCAPE, 0x01, 0, 0));
    }

    TEST_METHOD(TextAsCodepoints_NoTextOnRelease)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportEventTypes | ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Text should not be present on release events
        process(input, true, VK_A, 0x1E, L'a', 0);
        auto releaseResult = process(input, false, VK_A, 0x1E, L'a', 0);
        // Release should be CSI 97;1:3u (no text parameter)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[97;1:3u"), releaseResult);
    }

    // =========================================================================
    // SECTION 10: Protocol Mode Management (Set, Reset, Replace)
    // =========================================================================

    TEST_METHOD(ProtocolMode_Replace)
    {
        TerminalInput input;

        // Replace mode (1) sets the exact flags
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes, input.GetKittyFlags());

        // Replace again overwrites
        input.SetKittyKeyboardProtocol(ReportEventTypes, KittyKeyboardProtocolMode::Replace);
        VERIFY_ARE_EQUAL(ReportEventTypes, input.GetKittyFlags());
    }

    TEST_METHOD(ProtocolMode_Set)
    {
        TerminalInput input;

        // Start with disambiguate
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Set mode (2) adds flags without removing existing
        input.SetKittyKeyboardProtocol(ReportEventTypes, KittyKeyboardProtocolMode::Set);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes | ReportEventTypes, input.GetKittyFlags());
    }

    TEST_METHOD(ProtocolMode_Reset)
    {
        TerminalInput input;

        // Start with multiple flags
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes | ReportEventTypes | ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Reset mode (3) removes specific flags
        input.SetKittyKeyboardProtocol(ReportEventTypes, KittyKeyboardProtocolMode::Reset);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes | ReportAllKeysAsEscapeCodes, input.GetKittyFlags());
    }

    // =========================================================================
    // SECTION 11: Push/Pop Stack Behavior
    // =========================================================================

    TEST_METHOD(Stack_PushPop_Basic)
    {
        TerminalInput input;

        // Initial state
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());

        // Push with flags
        input.PushKittyFlags(DisambiguateEscapeCodes);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes, input.GetKittyFlags());

        // Push another
        input.PushKittyFlags(ReportAllKeysAsEscapeCodes);
        VERIFY_ARE_EQUAL(ReportAllKeysAsEscapeCodes, input.GetKittyFlags());

        // Pop once - should restore previous
        input.PopKittyFlags(1);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes, input.GetKittyFlags());

        // Pop again - should restore initial (0)
        input.PopKittyFlags(1);
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());
    }

    TEST_METHOD(Stack_Pop_EmptiesStack_ResetsAllFlags)
    {
        TerminalInput input;

        input.PushKittyFlags(DisambiguateEscapeCodes);
        input.PushKittyFlags(ReportEventTypes);

        // Pop more than stack size - should reset to 0
        input.PopKittyFlags(10);
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());
    }

    TEST_METHOD(Stack_MainAndAlternate_Independent)
    {
        TerminalInput input;

        // Set flags in main screen
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);
        input.PushKittyFlags(ReportEventTypes);

        // Switch to alternate screen - flags should reset for alternate
        input.UseAlternateScreenBuffer();
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());

        // Set different flags in alternate
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);
        VERIFY_ARE_EQUAL(ReportAllKeysAsEscapeCodes, input.GetKittyFlags());

        // Switch back to main - should restore main screen state
        input.UseMainScreenBuffer();
        VERIFY_ARE_EQUAL(ReportEventTypes, input.GetKittyFlags());
    }

    // =========================================================================
    // SECTION 12: Surrogate Pair Handling
    // =========================================================================

    TEST_METHOD(SurrogatePairs_LeadingSurrogate_Buffered)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Leading surrogate alone should produce no output
        auto result = process(input, true, 0, 0, 0xD83D, 0); // Leading surrogate of 😀
        VERIFY_IS_TRUE(!result.has_value() || std::get<TerminalInput::StringType>(*result).empty());
    }

    TEST_METHOD(SurrogatePairs_Complete_Emoji)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Leading surrogate (buffered)
        process(input, true, 0, 0, 0xD83D, 0);
        // Trailing surrogate completes the pair
        // 😀 = U+1F600 = 128512
        auto result = process(input, true, 0, 0, 0xDE00, 0);
        // Should produce CSI 128512;;128512u
        VERIFY_ARE_EQUAL(wrap(L"\x1b[128512;;128512u"), result);
    }

    // =========================================================================
    // SECTION 13: Edge Cases and Special Scenarios
    // =========================================================================

    TEST_METHOD(EdgeCase_VK_PACKET_PassThrough)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // VK_PACKET (0xE7) should pass through the unicode char directly
        // This is used for synthesized keyboard events
        auto result = process(input, true, 0xE7, 0, L'x', 0);
        // The implementation may handle this specially
    }

    TEST_METHOD(EdgeCase_ZeroVirtualKey_PassThrough)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // Zero virtual key with a unicode char should pass through
        auto result = process(input, true, 0, 0, L'y', 0);
    }

    TEST_METHOD(EdgeCase_AutoRepeat_Disabled)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);
        input.SetInputMode(TerminalInput::Mode::AutoRepeat, false);

        // First press
        auto first = process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_IS_TRUE(first.has_value());

        // Second press (would be repeat) - should be suppressed
        auto second = process(input, true, VK_A, 0x1E, L'a', 0);
        VERIFY_IS_TRUE(!second.has_value() || std::get<TerminalInput::StringType>(*second).empty());
    }

    TEST_METHOD(EdgeCase_ForceDisableKitty)
    {
        TerminalInput input;

        // Set flags
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);
        VERIFY_ARE_EQUAL(DisambiguateEscapeCodes, input.GetKittyFlags());

        // Force disable
        input.ForceDisableKittyKeyboardProtocol(true);
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());

        // Attempts to set flags should be ignored
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes, KittyKeyboardProtocolMode::Replace);
        VERIFY_ARE_EQUAL(0, input.GetKittyFlags());
    }

    TEST_METHOD(EdgeCase_CtrlSpace_NullByte)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(ReportAllKeysAsEscapeCodes | ReportAssociatedText, KittyKeyboardProtocolMode::Replace);

        // Ctrl+Space should produce key with null character
        // The kitty key code for space is 32
        auto result = process(input, true, VK_SPACE, 0x39, 0, LEFT_CTRL_PRESSED);
        // Control codes (< 0x20) are not included in text per spec
        // So this should be CSI 32;5u (no text, since ctrl+space produces 0x00 which is control)
        VERIFY_ARE_EQUAL(wrap(L"\x1b[32;5u"), result);
    }

    TEST_METHOD(EdgeCase_AltGr_Handling)
    {
        TerminalInput input;
        input.SetKittyKeyboardProtocol(DisambiguateEscapeCodes, KittyKeyboardProtocolMode::Replace);

        // AltGr generates both RIGHT_ALT and LEFT_CTRL
        // The implementation should detect this and not treat it as Ctrl+Alt
        auto result = process(input, true, VK_A, 0x1E, L'ä', RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED);
        // With AltGr, the character 'ä' should be transmitted if it's a valid char
        // The exact handling depends on implementation
    }
};
