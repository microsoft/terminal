// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChord.g.h"

namespace winrt::Microsoft::Terminal::Settings::implementation
{
    struct KeyChord : KeyChordT<KeyChord>
    {
        KeyChord() = default;
        KeyChord(Settings::KeyModifiers const& modifiers, int32_t vkey);
        KeyChord(bool ctrl, bool alt, bool shift, int32_t vkey);

        Settings::KeyModifiers Modifiers();
        void Modifiers(Settings::KeyModifiers const& value);
        int32_t Vkey();
        void Vkey(int32_t value);

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
