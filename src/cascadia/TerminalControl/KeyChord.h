// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChord.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct KeyChord : KeyChordT<KeyChord>
    {
        KeyChord() noexcept;
        KeyChord(winrt::Windows::System::VirtualKeyModifiers const& modifiers, int32_t vkey) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey) noexcept;

        winrt::Windows::System::VirtualKeyModifiers Modifiers() noexcept;
        void Modifiers(winrt::Windows::System::VirtualKeyModifiers const& value) noexcept;
        int32_t Vkey() noexcept;
        void Vkey(int32_t value) noexcept;

    private:
        winrt::Windows::System::VirtualKeyModifiers _modifiers;
        int32_t _vkey;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    struct KeyChord : KeyChordT<KeyChord, implementation::KeyChord>
    {
    };
}
