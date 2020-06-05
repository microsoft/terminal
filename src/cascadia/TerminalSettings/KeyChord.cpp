// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChord.h"

#include "KeyChord.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    KeyChord::KeyChord() noexcept :
        _modifiers{ 0 },
        _vkey{ 0 }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? Settings::KeyModifiers::Ctrl : Settings::KeyModifiers::None) |
                    (alt ? Settings::KeyModifiers::Alt : Settings::KeyModifiers::None) |
                    (shift ? Settings::KeyModifiers::Shift : Settings::KeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(Settings::KeyModifiers const& modifiers, int32_t vkey) noexcept :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    Settings::KeyModifiers KeyChord::Modifiers() noexcept
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(Settings::KeyModifiers const& value) noexcept
    {
        _modifiers = value;
    }

    int32_t KeyChord::Vkey() noexcept
    {
        return _vkey;
    }

    void KeyChord::Vkey(int32_t value) noexcept
    {
        _vkey = value;
    }
}
