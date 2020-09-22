// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordSerialization.g.h"
#include "../inc/cppwinrt_utils.h"

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
    BASIC_FACTORY(KeyChordSerialization);
}
