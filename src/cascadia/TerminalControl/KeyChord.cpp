// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChord.h"

#include "KeyChord.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    KeyChord::KeyChord() noexcept :
        _modifiers{ 0 },
        _vkey{ 0 }
    {
    }

    KeyChord::KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept :
        _modifiers{ (ctrl ? TerminalControl::KeyModifiers::Ctrl : TerminalControl::KeyModifiers::None) |
                    (alt ? TerminalControl::KeyModifiers::Alt : TerminalControl::KeyModifiers::None) |
                    (shift ? TerminalControl::KeyModifiers::Shift : TerminalControl::KeyModifiers::None) },
        _vkey{ vkey }
    {
    }

    KeyChord::KeyChord(TerminalControl::KeyModifiers const& modifiers, int32_t vkey) noexcept :
        _modifiers{ modifiers },
        _vkey{ vkey }
    {
    }

    TerminalControl::KeyModifiers KeyChord::Modifiers() noexcept
    {
        return _modifiers;
    }

    void KeyChord::Modifiers(TerminalControl::KeyModifiers const& value) noexcept
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
