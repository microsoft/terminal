// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChordSerialization.h"

using namespace winrt::Microsoft::Terminal::Settings;

static constexpr std::wstring_view CTRL_KEY{ L"ctrl" };
static constexpr std::wstring_view SHIFT_KEY{ L"shift" };
static constexpr std::wstring_view ALT_KEY{ L"alt" };

static constexpr int MAX_CHORD_PARTS = 4;

// clang-format off
static const std::unordered_map<int32_t, std::wstring_view> vkeyNamePairs {
    { VK_BACK       , L"backspace"},
    { VK_TAB        , L"tab"},
    { VK_RETURN     , L"enter" },
    { VK_ESCAPE     , L"esc" },
    { VK_SPACE      , L"space" },
    { VK_PRIOR      , L"pgup" },
    { VK_NEXT       , L"pgdn" },
    { VK_END        , L"end" },
    { VK_HOME       , L"home" },
    { VK_LEFT       , L"left" },
    { VK_UP         , L"up" },
    { VK_RIGHT      , L"right" },
    { VK_DOWN       , L"down" },
    { VK_INSERT     , L"insert" },
    { VK_DELETE     , L"delete" },
    { VK_NUMPAD0    , L"numpad_0" },
    { VK_NUMPAD1    , L"numpad_1" },
    { VK_NUMPAD2    , L"numpad_2" },
    { VK_NUMPAD3    , L"numpad_3" },
    { VK_NUMPAD4    , L"numpad_4" },
    { VK_NUMPAD5    , L"numpad_5" },
    { VK_NUMPAD6    , L"numpad_6" },
    { VK_NUMPAD7    , L"numpad_7" },
    { VK_NUMPAD8    , L"numpad_8" },
    { VK_NUMPAD9    , L"numpad_9" },
    { VK_MULTIPLY   , L"numpad_multiply" },
    { VK_ADD        , L"numpad_plus" },
    { VK_SUBTRACT   , L"numpad_minus" },
    { VK_DECIMAL    , L"numpad_period" },
    { VK_DIVIDE     , L"numpad_divide" },
    { VK_F1         , L"f1" },
    { VK_F2         , L"f2" },
    { VK_F3         , L"f3" },
    { VK_F4         , L"f4" },
    { VK_F5         , L"f5" },
    { VK_F6         , L"f6" },
    { VK_F7         , L"f7" },
    { VK_F8         , L"f8" },
    { VK_F9         , L"f9" },
    { VK_F10        , L"f10" },
    { VK_F11        , L"f11" },
    { VK_F12        , L"f12" },
    { VK_F13        , L"f13" },
    { VK_F14        , L"f14" },
    { VK_F15        , L"f15" },
    { VK_F16        , L"f16" },
    { VK_F17        , L"f17" },
    { VK_F18        , L"f18" },
    { VK_F19        , L"f19" },
    { VK_F20        , L"f20" },
    { VK_F21        , L"f21" },
    { VK_F22        , L"f22" },
    { VK_F23        , L"f23" },
    { VK_F24        , L"f24" },
    { VK_OEM_PLUS   , L"plus" }
};
// clang-format on

// Function Description:
// - Deserializes the given string into a new KeyChord instance. If this
//   fails to translate the string into a keychord, it will throw a
//   hresult_invalid_argument exception.
// - The string should fit the format "[ctrl+][alt+][shift+]<keyName>",
//   where each modifier is optional, and keyName is either one of the
//   names listed in the vkeyNamePairs vector above, or is one of 0-9a-zA-Z.
// Arguments:
// - hstr: the string to parse into a keychord.
// Return Value:
// - a newly constructed KeyChord
winrt::Microsoft::Terminal::Settings::KeyChord KeyChordSerialization::FromString(const winrt::hstring& hstr)
{
    std::wstring wstr{ hstr };

    // Split the string on '+'
    std::wstring temp;
    std::vector<std::wstring> parts;
    std::wstringstream wss(wstr);

    while (std::getline(wss, temp, L'+'))
    {
        parts.push_back(temp);

        // If we have > 4, something's wrong.
        if (parts.size() > MAX_CHORD_PARTS)
        {
            throw winrt::hresult_invalid_argument();
        }
    }

    KeyModifiers modifiers = KeyModifiers::None;
    int32_t vkey = 0;

    // Look for ctrl, shift, alt. Anything else might be a key
    for (const auto& part : parts)
    {
        std::wstring lowercase = part;
        std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), std::towlower);
        if (lowercase == CTRL_KEY)
        {
            modifiers |= KeyModifiers::Ctrl;
        }
        else if (lowercase == ALT_KEY)
        {
            modifiers |= KeyModifiers::Alt;
        }
        else if (lowercase == SHIFT_KEY)
        {
            modifiers |= KeyModifiers::Shift;
        }
        else
        {
            bool foundKey = false;
            // For potential keys, look through the pairs of strings and vkeys
            if (part.size() == 1)
            {
                const wchar_t wch = part.at(0);
                // Quick lookup: ranges of vkeys that correlate directly to a key.
                if (wch >= L'0' && wch <= L'9')
                {
                    vkey = static_cast<int32_t>(wch);
                    foundKey = true;
                }
                else if (wch >= L'a' && wch <= L'z')
                {
                    // subtract 0x20 to shift to uppercase
                    vkey = static_cast<int32_t>(wch - 0x20);
                    foundKey = true;
                }
                else if (wch >= L'A' && wch <= L'Z')
                {
                    vkey = static_cast<int32_t>(wch);
                    foundKey = true;
                }
            }

            // If we didn't find the key with a quick lookup, search the
            // table to see if we have a matching name.
            if (!foundKey)
            {
                for (const auto& pair : vkeyNamePairs)
                {
                    if (pair.second == part)
                    {
                        vkey = pair.first;
                        foundKey = true;
                        break;
                    }
                }
            }

            // If we haven't found a key, attempt a keyboard mapping
            if (!foundKey && part.size() == 1)
            {
                auto oemVk = VkKeyScanW(part[0]);
                if (oemVk != -1)
                {
                    vkey = oemVk & 0xFF;
                    auto oemModifiers = (oemVk & 0xFF00) >> 8;
                    // We're using WI_SetFlagIf instead of WI_UpdateFlag because we want to be strictly additive
                    // to the user's specified modifiers. ctrl+| should be the same as ctrl+shift+\,
                    // but if we used WI_UpdateFlag, ctrl+shift+\ would turn _off_ Shift because \ doesn't
                    // require it.
                    WI_SetFlagIf(modifiers, KeyModifiers::Shift, WI_IsFlagSet(oemModifiers, 1U));
                    WI_SetFlagIf(modifiers, KeyModifiers::Ctrl, WI_IsFlagSet(oemModifiers, 2U));
                    WI_SetFlagIf(modifiers, KeyModifiers::Alt, WI_IsFlagSet(oemModifiers, 4U));
                    foundKey = true;
                }
            }

            // If we weren't able to find a match, throw an exception.
            if (!foundKey)
            {
                throw winrt::hresult_invalid_argument();
            }
        }
    }

    return winrt::Microsoft::Terminal::Settings::KeyChord{ modifiers, vkey };
}

// Function Description:
// - Serialize this keychord into a string represenation.
// - The string will fit the format "[ctrl+][alt+][shift+]<keyName>",
//   where each modifier is optional, and keyName is either one of the
//   names listed in the vkeyNamePairs vector above, or is one of 0-9a-z.
// Return Value:
// - a string which is an equivalent serialization of this object.
winrt::hstring KeyChordSerialization::ToString(const KeyChord& chord)
{
    bool serializedSuccessfully = false;
    const auto modifiers = chord.Modifiers();
    const auto vkey = chord.Vkey();

    std::wstring buffer{ L"" };

    // Add modifiers
    if (WI_IsFlagSet(modifiers, KeyModifiers::Ctrl))
    {
        buffer += CTRL_KEY;
        buffer += L"+";
    }
    if (WI_IsFlagSet(modifiers, KeyModifiers::Alt))
    {
        buffer += ALT_KEY;
        buffer += L"+";
    }
    if (WI_IsFlagSet(modifiers, KeyModifiers::Shift))
    {
        buffer += SHIFT_KEY;
        buffer += L"+";
    }

    // Quick lookup: ranges of vkeys that correlate directly to a key.
    if (vkey >= L'0' && vkey <= L'9')
    {
        buffer += std::wstring(1, static_cast<wchar_t>(vkey));
        serializedSuccessfully = true;
    }
    else if (vkey >= L'A' && vkey <= L'Z')
    {
        // add 0x20 to shift to lowercase
        buffer += std::wstring(1, static_cast<wchar_t>(vkey + 0x20));
        serializedSuccessfully = true;
    }
    else
    {
        if (vkeyNamePairs.find(vkey) != vkeyNamePairs.end())
        {
            buffer += vkeyNamePairs.at(vkey);
            serializedSuccessfully = true;
        }
        else
        {
            auto mappedChar = MapVirtualKeyW(vkey, MAPVK_VK_TO_CHAR);
            if (mappedChar != 0)
            {
                wchar_t mappedWch = gsl::narrow_cast<wchar_t>(mappedChar);
                buffer += std::wstring_view{ &mappedWch, 1 };
                serializedSuccessfully = true;
            }
        }
    }

    if (!serializedSuccessfully)
    {
        buffer = L"";
    }

    return winrt::hstring{ buffer };
}
