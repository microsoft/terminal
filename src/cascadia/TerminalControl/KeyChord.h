// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChord.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct KeyChord : KeyChordT<KeyChord>
    {
        KeyChord() noexcept;
        KeyChord(Control::KeyModifiers const& modifiers, int32_t vkey) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey) noexcept;

        Control::KeyModifiers Modifiers() noexcept;
        void Modifiers(Control::KeyModifiers const& value) noexcept;
        int32_t Vkey() noexcept;
        void Vkey(int32_t value) noexcept;

    private:
        Control::KeyModifiers _modifiers;
        int32_t _vkey;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    struct KeyChord : KeyChordT<KeyChord, implementation::KeyChord>
    {
    };
}
