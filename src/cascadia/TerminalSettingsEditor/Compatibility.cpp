// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Compatibility.h"
#include "Compatibility.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Compatibility::Compatibility()
    {
        InitializeComponent();
    }

    void Compatibility::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::CompatibilityViewModel>();
    }
}
