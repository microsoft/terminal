// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"
#include "InteractionPageNavigationState.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();
    }

    void Interaction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::InteractionPageNavigationState>();
    }
}
