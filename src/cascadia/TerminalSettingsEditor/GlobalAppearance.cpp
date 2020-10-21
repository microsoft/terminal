// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include "MainPage.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        InitializeComponent();
    }

    GlobalAppSettings GlobalAppearance::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
