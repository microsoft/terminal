// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/IInputEvent.hpp"

#include <unordered_map>

// Routine Description:
// - checks if flag is present in flags
// Arguments:
// - flags - bit battern to check for flag
// - flag - bit pattern to search for
// Return Value:
// - true if flag is present in flags
// Note:
// - The wil version of IsFlagSet is only to operate on values that
// are compile time constants. This will work with runtime calculated
// values.
constexpr bool RuntimeIsFlagSet(const DWORD flags, const DWORD flag) noexcept
{
    return !!(flags & flag);
}

std::unordered_set<ModifierKeyState> FromVkKeyScan(const short vkKeyScanFlags)
{
    std::unordered_set<ModifierKeyState> keyState;

    switch (vkKeyScanFlags)
    {
    case VkKeyScanModState::None:
        break;
    case VkKeyScanModState::ShiftPressed:
        keyState.insert(ModifierKeyState::Shift);
        break;
    case VkKeyScanModState::CtrlPressed:
        keyState.insert(ModifierKeyState::LeftCtrl);
        keyState.insert(ModifierKeyState::RightCtrl);
        break;
    case VkKeyScanModState::ShiftAndCtrlPressed:
        keyState.insert(ModifierKeyState::Shift);
        keyState.insert(ModifierKeyState::LeftCtrl);
        keyState.insert(ModifierKeyState::RightCtrl);
        break;
    case VkKeyScanModState::AltPressed:
        keyState.insert(ModifierKeyState::LeftAlt);
        keyState.insert(ModifierKeyState::RightAlt);
        break;
    case VkKeyScanModState::ShiftAndAltPressed:
        keyState.insert(ModifierKeyState::Shift);
        keyState.insert(ModifierKeyState::LeftAlt);
        keyState.insert(ModifierKeyState::RightAlt);
        break;
    case VkKeyScanModState::CtrlAndAltPressed:
        keyState.insert(ModifierKeyState::LeftCtrl);
        keyState.insert(ModifierKeyState::RightCtrl);
        keyState.insert(ModifierKeyState::LeftAlt);
        keyState.insert(ModifierKeyState::RightAlt);
        break;
    case VkKeyScanModState::ModPressed:
        keyState.insert(ModifierKeyState::Shift);
        keyState.insert(ModifierKeyState::LeftCtrl);
        keyState.insert(ModifierKeyState::RightCtrl);
        keyState.insert(ModifierKeyState::LeftAlt);
        keyState.insert(ModifierKeyState::RightAlt);
        break;
    default:
        THROW_HR(E_INVALIDARG);
        break;
    }

    return keyState;
}

using ModifierKeyStateMapping = std::pair<ModifierKeyState, DWORD>;

constexpr static ModifierKeyStateMapping ModifierKeyStateTranslationTable[] = {
    { ModifierKeyState::RightAlt, RIGHT_ALT_PRESSED },
    { ModifierKeyState::LeftAlt, LEFT_ALT_PRESSED },
    { ModifierKeyState::RightCtrl, RIGHT_CTRL_PRESSED },
    { ModifierKeyState::LeftCtrl, LEFT_CTRL_PRESSED },
    { ModifierKeyState::Shift, SHIFT_PRESSED },
    { ModifierKeyState::NumLock, NUMLOCK_ON },
    { ModifierKeyState::ScrollLock, SCROLLLOCK_ON },
    { ModifierKeyState::CapsLock, CAPSLOCK_ON },
    { ModifierKeyState::EnhancedKey, ENHANCED_KEY },
    { ModifierKeyState::NlsDbcsChar, NLS_DBCSCHAR },
    { ModifierKeyState::NlsAlphanumeric, NLS_ALPHANUMERIC },
    { ModifierKeyState::NlsKatakana, NLS_KATAKANA },
    { ModifierKeyState::NlsHiragana, NLS_HIRAGANA },
    { ModifierKeyState::NlsRoman, NLS_ROMAN },
    { ModifierKeyState::NlsImeConversion, NLS_IME_CONVERSION },
    { ModifierKeyState::AltNumpad, ALTNUMPAD_BIT },
    { ModifierKeyState::NlsImeDisable, NLS_IME_DISABLE }
};

static_assert(size(ModifierKeyStateTranslationTable) == static_cast<int>(ModifierKeyState::ENUM_COUNT),
              "ModifierKeyStateTranslationTable must have a valid mapping for each modifier value");

// Routine Description:
// - Expands legacy control keys bitsets into a stl set
// Arguments:
// - flags - legacy bitset to expand
// Return Value:
// - set of ModifierKeyState values that represent flags
std::unordered_set<ModifierKeyState> FromConsoleControlKeyFlags(const DWORD flags)
{
    std::unordered_set<ModifierKeyState> keyStates;

    for (const ModifierKeyStateMapping& mapping : ModifierKeyStateTranslationTable)
    {
        if (RuntimeIsFlagSet(flags, mapping.second))
        {
            keyStates.insert(mapping.first);
        }
    }

    return keyStates;
}

// Routine Description:
// - Converts ModifierKeyState back to the bizarre console bitflag associated with it.
// Arguments:
// - modifierKey - modifier to convert
// Return Value:
// - console bitflag associated with modifierKey
DWORD ToConsoleControlKeyFlag(const ModifierKeyState modifierKey) noexcept
{
    for (const ModifierKeyStateMapping& mapping : ModifierKeyStateTranslationTable)
    {
        if (mapping.first == modifierKey)
        {
            return mapping.second;
        }
    }
    return 0;
}
