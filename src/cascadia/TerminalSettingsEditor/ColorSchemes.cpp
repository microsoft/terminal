// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemes.h"
#include "ColorSchemes.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemes::ColorSchemes()
    {
        InitializeComponent();
    }

    ColorTableEntry::ColorTableEntry(const winrt::hstring& name, const Windows::UI::Color& color)
    {
        Name(name);
        Color(color);
    }
}
