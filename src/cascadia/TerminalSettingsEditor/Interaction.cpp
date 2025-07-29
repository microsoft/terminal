// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"

#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(WarnAboutMultiLinePaste, WarnAboutMultiLinePaste, winrt::Microsoft::Terminal::Control::WarnAboutMultiLinePaste, L"Globals_WarnAboutMultiLinePaste", L"Content");
    }

    void Interaction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::InteractionViewModel>();
    }
}
