// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChord.h"

#include "KeyChord.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) :
        _modifiers{ (ctrl ? Settings::KeyModifiers::Ctrl : Settings::KeyModifiers::None) |
                    (alt ? Settings::KeyModifiers::Alt : Settings::KeyModifiers::None) |
                    (shift ? Settings::KeyModifiers::Shift : Settings::KeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(Settings::KeyModifiers const& modifiers, int32_t vkey) :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    Settings::KeyModifiers KeyChord::Modifiers()
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(Settings::KeyModifiers const& value)
    {
        _modifiers = value;
    }

    int32_t KeyChord::Vkey()
    {
        return _vkey;
    }

    void KeyChord::Vkey(int32_t value)
    {
        _vkey = value;
    }
}
