// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"
#include "InteractionPageNavigationState.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();
    }

    void Interaction::OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e)
    {
        _Globals = e.Parameter().as<Editor::InteractionPageNavigationState>().Globals();
    }
}
