// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"

using namespace WUX::Navigation;
using namespace MTSM;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();
    }

    void Interaction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::InteractionViewModel>();
    }
}
