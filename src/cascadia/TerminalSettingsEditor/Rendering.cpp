// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"
#include "MainPage.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Rendering::Rendering()
    {
        InitializeComponent();
    }

    void Rendering::OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e)
    {
        _GlobalSettings = e.Parameter().as<Model::GlobalAppSettings>();
    }
}
