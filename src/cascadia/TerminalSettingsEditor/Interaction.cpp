// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MainPage.h"
#include "Interaction.h"
#include "Interaction.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();
    }

    GlobalAppSettings Interaction::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }
}
