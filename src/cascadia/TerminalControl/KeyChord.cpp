// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChord.h"

#include "KeyChord.g.cpp"

using VirtualKeyModifiers = winrt::Windows::System::VirtualKeyModifiers;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    KeyChord::KeyChord() noexcept :
        _modifiers{ 0 },
        _vkey{ 0 }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? VirtualKeyModifiers::Control : VirtualKeyModifiers::None) |
                    (alt ? VirtualKeyModifiers::Menu : VirtualKeyModifiers::None) |
                    (shift ? VirtualKeyModifiers::Shift : VirtualKeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? VirtualKeyModifiers::Control : VirtualKeyModifiers::None) |
                    (alt ? VirtualKeyModifiers::Menu : VirtualKeyModifiers::None) |
                    (shift ? VirtualKeyModifiers::Shift : VirtualKeyModifiers::None) |
                    (win ? VirtualKeyModifiers::Windows : VirtualKeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(VirtualKeyModifiers const& modifiers, int32_t vkey) noexcept :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    VirtualKeyModifiers KeyChord::Modifiers() noexcept
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(VirtualKeyModifiers const& value) noexcept
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
