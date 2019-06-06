// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.Settings.h>

class KeyChordSerialization final
{
public:
    static winrt::Microsoft::Terminal::Settings::KeyChord FromString(const winrt::hstring& str);
    static winrt::hstring ToString(const winrt::Microsoft::Terminal::Settings::KeyChord& chord);
};
