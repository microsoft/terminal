// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>

class KeyChordSerialization final
{
public:
    static winrt::Microsoft::Terminal::TerminalControl::KeyChord FromString(const winrt::hstring& str);
    static winrt::hstring ToString(const winrt::Microsoft::Terminal::TerminalControl::KeyChord& chord);
};
