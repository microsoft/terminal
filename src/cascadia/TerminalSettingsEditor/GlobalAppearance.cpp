// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        InitializeComponent();
        auto map = EnumMappings::ElementTheme();
    }

    IMap<hstring, ElementTheme> GlobalAppearance::ElementThemes()
    {
        return _ElementThemeMap;
    }

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
