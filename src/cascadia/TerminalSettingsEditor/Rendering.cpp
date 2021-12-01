// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"
#include "RenderingPageNavigationState.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Rendering::Rendering()
    {
        InitializeComponent();
    }

    void Rendering::OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e)
    {
        _Globals = e.Parameter().as<Editor::RenderingPageNavigationState>().Globals();
    }
}
