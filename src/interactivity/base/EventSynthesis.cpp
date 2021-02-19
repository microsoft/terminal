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

std::deque<std::unique_ptr<KeyEvent>> Microsoft::Console::Interactivity::CharToKeyEvents(const wchar_t wch,
                                                                                         const unsigned int codepage)
{
    const short invalidKey = -1;
    short keyState = VkKeyScanW(wch);

    if (keyState == invalidKey)
    {
        // Determine DBCS character because these character does not know by VkKeyScan.
        // GetStringTypeW(CT_CTYPE3) & C3_ALPHA can determine all linguistic characters. However, this is
        // not include symbolic character for DBCS.
        WORD CharType = 0;
        GetStringTypeW(CT_CTYPE3, &wch, 1, &CharType);

        if (WI_IsFlagSet(CharType, C3_ALPHA) || GetQuickCharWidth(wch) == CodepointWidth::Wide)
        {
            keyState = 0;
        }
    }

    std::deque<std::unique_ptr<KeyEvent>> convertedEvents;
    if (keyState == invalidKey)
    {
        // if VkKeyScanW fails (char is not in kbd layout), we must
        // emulate the key being input through the numpad
        convertedEvents = SynthesizeNumpadEvents(wch, codepage);
    }
    else
    {
        convertedEvents = SynthesizeKeyboardEvents(wch, keyState);
    }

    return convertedEvents;
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
