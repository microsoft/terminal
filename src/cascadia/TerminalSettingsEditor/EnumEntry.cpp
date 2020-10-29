// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EnumEntry.h"
#include "EnumEntry.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    EnumEntry::EnumEntry(const winrt::hstring enumName, const winrt::Windows::Foundation::IInspectable& enumValue):
        _EnumName{ enumName },
        _EnumValue{ enumValue }
    {
    }
}
