// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChord.g.h"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    struct KeyChord : KeyChordT<KeyChord>
    {
        KeyChord() noexcept;
        KeyChord(Settings::KeyModifiers const& modifiers, int32_t vkey) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey) noexcept;

        Settings::KeyModifiers Modifiers() noexcept;
        void Modifiers(Settings::KeyModifiers const& value) noexcept;
        int32_t Vkey() noexcept;
        void Vkey(int32_t value) noexcept;

    private:
        Settings::KeyModifiers _modifiers;
        int32_t _vkey;
    };
}

namespace winrt::Microsoft::Terminal::Settings::factory_implementation
{
    struct KeyChord : KeyChordT<KeyChord, implementation::KeyChord>
    {
    };
}
