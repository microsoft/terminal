// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/EventSynthesis.hpp"

// TODO: MSFT 14150722 - can these const values be generated at
// runtime without breaking compatibility?
static constexpr WORD altScanCode = 0x38;
static constexpr WORD leftShiftScanCode = 0x2A;

// Routine Description:
// - naively determines the width of a UCS2 encoded wchar (with caveats noted above)
#pragma warning(suppress : 4505) // this function will be deleted if numpad events are disabled
static bool IsCharFullWidth(const wchar_t wch) noexcept
{
    return (0x1100 <= wch && wch <= 0x115f) || // From Unicode 9.0, Hangul Choseong is wide
           (0x2e80 <= wch && wch <= 0x303e) || // From Unicode 9.0, this range is wide (assorted languages)
           (0x3041 <= wch && wch <= 0x3094) || // Hiragana
           (0x30a1 <= wch && wch <= 0x30f6) || // Katakana
           (0x3105 <= wch && wch <= 0x312c) || // Bopomofo
           (0x3131 <= wch && wch <= 0x318e) || // Hangul Elements
           (0x3190 <= wch && wch <= 0x3247) || // From Unicode 9.0, this range is wide
           (0x3251 <= wch && wch <= 0x4dbf) || // Unicode 9.0 CJK Unified Ideographs, Yi, Reserved, Han Ideograph (hexagrams from 4DC0..4DFF are ignored
           (0x4e00 <= wch && wch <= 0xa4c6) || // Unicode 9.0 CJK Unified Ideographs, Yi, Reserved, Han Ideograph (hexagrams from 4DC0..4DFF are ignored
           (0xa960 <= wch && wch <= 0xa97c) || // Wide Hangul Choseong
           (0xac00 <= wch && wch <= 0xd7a3) || // Korean Hangul Syllables
           (0xf900 <= wch && wch <= 0xfaff) || // From Unicode 9.0, this range is wide [CJK Compatibility Ideographs, Includes Han Compatibility Ideographs]
           (0xfe10 <= wch && wch <= 0xfe1f) || // From Unicode 9.0, this range is wide [Presentation forms]
           (0xfe30 <= wch && wch <= 0xfe6b) || // From Unicode 9.0, this range is wide [Presentation forms]
           (0xff01 <= wch && wch <= 0xff5e) || // Fullwidth ASCII variants
           (0xffe0 <= wch && wch <= 0xffe6); // Fullwidth symbol variants
}

bool Microsoft::Console::Interactivity::CharToTypedChar(const wchar_t wch, TypedChar& out)
{
    static constexpr short invalidKey = -1;
    auto keyState = OneCoreSafeVkKeyScanW(wch);

    if (keyState == invalidKey)
    {
        if constexpr (Feature_UseNumpadEventsForClipboardInput::IsEnabled())
        {
            // Determine DBCS character because these character does not know by VkKeyScan.
            // GetStringTypeW(CT_CTYPE3) & C3_ALPHA can determine all linguistic characters. However, this is
            // not include symbolic character for DBCS.
            WORD CharType = 0;
            GetStringTypeW(CT_CTYPE3, &wch, 1, &CharType);

            if (WI_IsFlagClear(CharType, C3_ALPHA) && !IsCharFullWidth(wch))
            {
                // It wasn't alphanumeric or determined to be wide by the old algorithm
                // if VkKeyScanW fails (char is not in kbd layout), we must
                // emulate the key being input through the numpad
                return false;
            }
        }
        keyState = 0; // SynthesizeKeyboardEvents would rather get 0 than -1
    }

    out = KeyStateToTypedChar(wch, keyState);
    return true;
}

// The caller provides us with the result of VkKeyScanW() in keyState.
Microsoft::Console::Interactivity::TypedChar Microsoft::Console::Interactivity::KeyStateToTypedChar(const wchar_t wch, const short keyState)
{
    const auto vk = LOBYTE(keyState);
    const auto sc = gsl::narrow<WORD>(OneCoreSafeMapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    return { wch, sc, vk, HIBYTE(keyState) };
}

size_t Microsoft::Console::Interactivity::TypedCharEventCount(const TypedChar& ch) noexcept
{
    // Shift and AltGr (= ctrl + alt) get pressed before and released after the key itself.
    const auto shiftSet = WI_IsFlagSet(ch.modifiers, 1);
    const auto altGrSet = WI_AreAllFlagsSet(ch.modifiers, 4 | 2);
    return shiftSet || altGrSet ? 4 : 2;
}

// Writes the TypedCharEventCount() key events that typing `ch` produces into `out` and returns their count.
size_t Microsoft::Console::Interactivity::TypedCharToKeyEvents(const TypedChar& ch, std::array<INPUT_RECORD, 4>& out) noexcept
{
    // The magic constants below are the expected (documented) return values from VkKeyScanW().
    const auto shiftSet = WI_IsFlagSet(ch.modifiers, 1);
    const auto ctrlSet = WI_IsFlagSet(ch.modifiers, 2);
    const auto altSet = WI_IsFlagSet(ch.modifiers, 4);
    const auto altGrSet = WI_AreAllFlagsSet(ch.modifiers, 4 | 2);

    uint32_t controlKeyState = 0;
    WI_SetFlagIf(controlKeyState, SHIFT_PRESSED, shiftSet);
    WI_SetFlagIf(controlKeyState, LEFT_CTRL_PRESSED, ctrlSet);
    WI_SetFlagIf(controlKeyState, RIGHT_ALT_PRESSED, altSet);
    const auto keyDown = SynthesizeKeyEvent(true, 1, ch.virtualKey, ch.scanCode, ch.wch, controlKeyState);
    const auto keyUp = SynthesizeKeyEvent(false, 1, ch.virtualKey, ch.scanCode, ch.wch, controlKeyState);

    // handle yucky alt-gr keys
    if (altGrSet)
    {
        out = { SynthesizeKeyEvent(true, 1, VK_MENU, altScanCode, 0, ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED), keyDown, keyUp, SynthesizeKeyEvent(false, 1, VK_MENU, altScanCode, 0, ENHANCED_KEY) };
        return 4;
    }
    if (shiftSet)
    {
        out = { SynthesizeKeyEvent(true, 1, VK_SHIFT, leftShiftScanCode, 0, SHIFT_PRESSED), keyDown, keyUp, SynthesizeKeyEvent(false, 1, VK_SHIFT, leftShiftScanCode, 0, 0) };
        return 4;
    }
    til::at(out, 0) = keyDown;
    til::at(out, 1) = keyUp;
    return 2;
}

void Microsoft::Console::Interactivity::CharToKeyEvents(const wchar_t wch, const unsigned int codepage, InputEventQueue& keyEvents)
{
    TypedChar ch;
    if (!CharToTypedChar(wch, ch))
    {
        SynthesizeNumpadEvents(wch, codepage, keyEvents);
        return;
    }

    std::array<INPUT_RECORD, 4> events;
    const auto count = TypedCharToKeyEvents(ch, events);
    keyEvents.insert(keyEvents.end(), events.begin(), events.begin() + count);
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using the keyboard
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed
// Note:
// - will throw exception on error
void Microsoft::Console::Interactivity::SynthesizeKeyboardEvents(const wchar_t wch, const short keyState, InputEventQueue& keyEvents)
{
    const auto ch = KeyStateToTypedChar(wch, keyState);
    std::array<INPUT_RECORD, 4> events;
    const auto count = TypedCharToKeyEvents(ch, events);
    keyEvents.insert(keyEvents.end(), events.begin(), events.begin() + count);
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using Alt + numpad
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed using
// alt + numpad
// Note:
// - will throw exception on error
void Microsoft::Console::Interactivity::SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage, InputEventQueue& keyEvents)
{
    char converted = 0;
    const auto result = WideCharToMultiByte(codepage, 0, &wch, 1, &converted, 1, nullptr, nullptr);

    // alt keydown
    keyEvents.push_back(SynthesizeKeyEvent(true, 1, VK_MENU, altScanCode, 0, LEFT_ALT_PRESSED));

    if (result == 1)
    {
        // It is OK if the char is "signed -1", we want to interpret that as "unsigned 255" for the
        // "integer to character" conversion below with ::to_string, thus the static_cast.
        // Prime example is nonbreaking space U+00A0 will convert to OEM by codepage 437 to 0xFF which is -1 signed.
        // But it is absolutely valid as 0xFF or 255 unsigned as the correct CP437 character.
        // We need to treat it as unsigned because we're going to pretend it was a keypad entry
        // and you don't enter negative numbers on the keypad.
        const auto charString = std::to_string(static_cast<unsigned char>(converted));

        for (const auto& ch : charString)
        {
            const WORD vk = ch - '0' + VK_NUMPAD0;
            const auto sc = gsl::narrow<WORD>(OneCoreSafeMapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
            auto keyEvent = SynthesizeKeyEvent(true, 1, vk, sc, 0, LEFT_ALT_PRESSED);
            keyEvents.push_back(keyEvent);
            keyEvent.Event.KeyEvent.bKeyDown = FALSE;
            keyEvents.push_back(keyEvent);
        }
    }

    // alt keyup
    keyEvents.push_back(SynthesizeKeyEvent(false, 1, VK_MENU, altScanCode, wch, 0));
}
