// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChord.h"

#include "KeyChord.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    KeyChord::KeyChord() noexcept :
        _modifiers{ 0 },
        _vkey{ 0 }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? Control::KeyModifiers::Ctrl : Control::KeyModifiers::None) |
                    (alt ? Control::KeyModifiers::Alt : Control::KeyModifiers::None) |
                    (shift ? Control::KeyModifiers::Shift : Control::KeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? Control::KeyModifiers::Ctrl : Control::KeyModifiers::None) |
                    (alt ? Control::KeyModifiers::Alt : Control::KeyModifiers::None) |
                    (shift ? Control::KeyModifiers::Shift : Control::KeyModifiers::None) |
                    (win ? Control::KeyModifiers::Windows : Control::KeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(Control::KeyModifiers const& modifiers, int32_t vkey) noexcept :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    Control::KeyModifiers KeyChord::Modifiers() noexcept
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(Control::KeyModifiers const& value) noexcept
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
