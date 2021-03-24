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

        static winrt::Microsoft::Terminal::Control::KeyChord FromString(const winrt::hstring& str);
        static winrt::hstring ToString(const winrt::Microsoft::Terminal::Control::KeyChord& chord);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    // C++/WinRT generates a constructor even though one is not specified in the IDL
    BASIC_FACTORY(KeyChordSerialization);
}
