// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordSerialization.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct KeyChordSerialization
    {
        KeyChordSerialization() = default;

        static winrt::Microsoft::Terminal::TerminalControl::KeyChord FromString(const winrt::hstring& str);
        static winrt::hstring ToString(const winrt::Microsoft::Terminal::TerminalControl::KeyChord& chord);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    struct KeyChordSerialization : KeyChordSerializationT<KeyChordSerialization, implementation::KeyChordSerialization>
    {
    };
}
