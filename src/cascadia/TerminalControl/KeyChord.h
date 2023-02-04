// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChord.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct KeyChord : KeyChordT<KeyChord>
    {
        KeyChord() noexcept = default;
        KeyChord(const WS::VirtualKeyModifiers modifiers, int32_t vkey, int32_t scanCode) noexcept;
        KeyChord(bool ctrl, bool alt, bool shift, bool win, int32_t vkey, int32_t scanCode) noexcept;

        uint64_t Hash() const noexcept;
        bool Equals(const MTControl::KeyChord& other) const noexcept;

        WS::VirtualKeyModifiers Modifiers() const noexcept;
        void Modifiers(const WS::VirtualKeyModifiers value) noexcept;
        int32_t Vkey() const noexcept;
        void Vkey(int32_t value) noexcept;
        int32_t ScanCode() const noexcept;
        void ScanCode(int32_t value) noexcept;

    private:
        WS::VirtualKeyModifiers _modifiers{};
        int32_t _vkey{};
        int32_t _scanCode{};
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(KeyChord);
}
