// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/EventSynthesis.hpp"
#include "../../types/inc/convert.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../inc/VtApiRedirection.hpp"
#endif

#pragma hdrstop

// TODO: MSFT 14150722 - can these const values be generated at
// runtime without breaking compatibility?
static constexpr WORD altScanCode = 0x38;
static constexpr WORD leftShiftScanCode = 0x2A;

#ifdef __INSIDE_WINDOWS
// To reduce the risk of compatibility issues inside Windows, we're going to continue using the old
// version of GetQuickCharWidth to determine whether a character should be synthesized into numpad
// events.
static constexpr bool useNumpadEvents{ true };
#else // !defined(__INSIDE_WINDOWS)
// In Terminal, however, we will *always* use normal key events (!)
static constexpr bool useNumpadEvents{ false };
#endif // __INSIDE_WINDOWS

// Routine Description:
// - naively determines the width of a UCS2 encoded wchar (with caveats noted above)
#pragma warning(suppress : 4505) // this function will be deleted if useNumpadEvents is false
static CodepointWidth GetQuickCharWidthLegacyForNumpadEventSynthesis(const wchar_t wch) noexcept
{
    if ((0x1100 <= wch && wch <= 0x115f) // From Unicode 9.0, Hangul Choseong is wide
        || (0x2e80 <= wch && wch <= 0x303e) // From Unicode 9.0, this range is wide (assorted languages)
        || (0x3041 <= wch && wch <= 0x3094) // Hiragana
        || (0x30a1 <= wch && wch <= 0x30f6) // Katakana
        || (0x3105 <= wch && wch <= 0x312c) // Bopomofo
        || (0x3131 <= wch && wch <= 0x318e) // Hangul Elements
        || (0x3190 <= wch && wch <= 0x3247) // From Unicode 9.0, this range is wide
        || (0x3251 <= wch && wch <= 0x4dbf) // Unicode 9.0 CJK Unified Ideographs, Yi, Reserved, Han Ideograph (hexagrams from 4DC0..4DFF are ignored
        || (0x4e00 <= wch && wch <= 0xa4c6) // Unicode 9.0 CJK Unified Ideographs, Yi, Reserved, Han Ideograph (hexagrams from 4DC0..4DFF are ignored
        || (0xa960 <= wch && wch <= 0xa97c) // Wide Hangul Choseong
        || (0xac00 <= wch && wch <= 0xd7a3) // Korean Hangul Syllables
        || (0xf900 <= wch && wch <= 0xfaff) // From Unicode 9.0, this range is wide [CJK Compatibility Ideographs, Includes Han Compatibility Ideographs]
        || (0xfe10 <= wch && wch <= 0xfe1f) // From Unicode 9.0, this range is wide [Presentation forms]
        || (0xfe30 <= wch && wch <= 0xfe6b) // From Unicode 9.0, this range is wide [Presentation forms]
        || (0xff01 <= wch && wch <= 0xff5e) // Fullwidth ASCII variants
        || (0xffe0 <= wch && wch <= 0xffe6)) // Fullwidth symbol variants
    {
        return CodepointWidth::Wide;
    }

    return CodepointWidth::Invalid;
}

std::deque<std::unique_ptr<KeyEvent>> Microsoft::Console::Interactivity::CharToKeyEvents(const wchar_t wch,
                                                                                         const unsigned int codepage)
{
    const short invalidKey = -1;
    short keyState = VkKeyScanW(wch);

    if (keyState == invalidKey)
    {
        if constexpr (useNumpadEvents)
        {
            // Determine DBCS character because these character does not know by VkKeyScan.
            // GetStringTypeW(CT_CTYPE3) & C3_ALPHA can determine all linguistic characters. However, this is
            // not include symbolic character for DBCS.
            WORD CharType = 0;
            GetStringTypeW(CT_CTYPE3, &wch, 1, &CharType);

            if (!(WI_IsFlagSet(CharType, C3_ALPHA) || GetQuickCharWidthLegacyForNumpadEventSynthesis(wch) == CodepointWidth::Wide))
            {
                // It wasn't alphanumeric or determined to be wide by the old algorithm
                // if VkKeyScanW fails (char is not in kbd layout), we must
                // emulate the key being input through the numpad
                return SynthesizeNumpadEvents(wch, codepage);
            }
        }
        keyState = 0; // SynthesizeKeyboardEvents would rather get 0 than -1
    }

    return SynthesizeKeyboardEvents(wch, keyState);
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
std::deque<std::unique_ptr<KeyEvent>> Microsoft::Console::Interactivity::SynthesizeKeyboardEvents(const wchar_t wch, const short keyState)
{
    const byte modifierState = HIBYTE(keyState);

    bool altGrSet = false;
    bool shiftSet = false;
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    // add modifier key event if necessary
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        altGrSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       (ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
    }
    else if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        shiftSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       SHIFT_PRESSED));
    }

    const auto vk = LOBYTE(keyState);
    const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    KeyEvent keyEvent{ true, 1, LOBYTE(keyState), virtualScanCode, wch, 0 };

    // add modifier flags if necessary
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::Shift);
    }
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::CtrlPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::LeftCtrl);
    }
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::RightAlt);
    }

    // add key event down and up
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
    keyEvent.SetKeyDown(false);
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));

    // add modifier key up event
    if (altGrSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       ENHANCED_KEY));
    }
    else if (shiftSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       0));
    }

    return keyEvents;
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
std::deque<std::unique_ptr<KeyEvent>> Microsoft::Console::Interactivity::SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage)
{
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    //alt keydown
    keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   UNICODE_NULL,
                                                   LEFT_ALT_PRESSED));

    const int radix = 10;
    std::wstring wstr{ wch };
    const auto convertedChars = ConvertToA(codepage, wstr);
    if (convertedChars.size() == 1)
    {
        // It is OK if the char is "signed -1", we want to interpret that as "unsigned 255" for the
        // "integer to character" conversion below with ::to_string, thus the static_cast.
        // Prime example is nonbreaking space U+00A0 will convert to OEM by codepage 437 to 0xFF which is -1 signed.
        // But it is absolutely valid as 0xFF or 255 unsigned as the correct CP437 character.
        // We need to treat it as unsigned because we're going to pretend it was a keypad entry
        // and you don't enter negative numbers on the keypad.
        unsigned char const uch = static_cast<unsigned char>(convertedChars.at(0));

        // unsigned char values are in the range [0, 255] so we need to be
        // able to store up to 4 chars from the conversion (including the end of string char)
        auto charString = std::to_string(uch);

        for (auto& ch : std::string_view(charString))
        {
            if (ch == 0)
            {
                break;
            }
            const WORD virtualKey = ch - '0' + VK_NUMPAD0;
            const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));

            keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
            keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
        }
    }

    // alt keyup
    keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   wch,
                                                   0));
    return keyEvents;
}
