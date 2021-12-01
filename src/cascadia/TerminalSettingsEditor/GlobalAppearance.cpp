// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include "GlobalAppearancePageNavigationState.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        InitializeComponent();
    }

    void GlobalAppearance::OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e)
    {
        _Globals = e.Parameter().as<Editor::GlobalAppearancePageNavigationState>().Globals();
    }
}
