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
        _modifiers{ (ctrl ? winrt::Windows::System::VirtualKeyModifiers::Control : winrt::Windows::System::VirtualKeyModifiers::None) |
                    (alt ? winrt::Windows::System::VirtualKeyModifiers::Menu : winrt::Windows::System::VirtualKeyModifiers::None) |
                    (shift ? winrt::Windows::System::VirtualKeyModifiers::Shift : winrt::Windows::System::VirtualKeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? winrt::Windows::System::VirtualKeyModifiers::Control : winrt::Windows::System::VirtualKeyModifiers::None) |
                    (alt ? winrt::Windows::System::VirtualKeyModifiers::Menu : winrt::Windows::System::VirtualKeyModifiers::None) |
                    (shift ? winrt::Windows::System::VirtualKeyModifiers::Shift : winrt::Windows::System::VirtualKeyModifiers::None) |
                    (win ? winrt::Windows::System::VirtualKeyModifiers::Windows : winrt::Windows::System::VirtualKeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(winrt::Windows::System::VirtualKeyModifiers const& modifiers, int32_t vkey) noexcept :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    winrt::Windows::System::VirtualKeyModifiers KeyChord::Modifiers() noexcept
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(winrt::Windows::System::VirtualKeyModifiers const& value) noexcept
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
