// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Compatibility.h"
#include "EnumEntry.h"
#include "Compatibility.g.cpp"
#include "CompatibilityViewModel.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    CompatibilityViewModel::CompatibilityViewModel(Model::CascadiaSettings settings) :
        _settings{ settings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TextMeasurement, TextMeasurement, winrt::Microsoft::Terminal::Control::TextMeasurement, L"Globals_TextMeasurement_", L"Text");
    }

    bool CompatibilityViewModel::DebugFeaturesAvailable() const noexcept
    {
        return Feature_DebugModeUI::IsEnabled();
    }

    void CompatibilityViewModel::ResetApplicationState()
    {
        _settings.ResetApplicationState();
    }

    void CompatibilityViewModel::ResetToDefaultSettings()
    {
        _settings.ResetToDefaultSettings();
    }

    Compatibility::Compatibility()
    {
        InitializeComponent();
    }

    void Compatibility::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::CompatibilityViewModel>();
    }

    void Compatibility::ResetApplicationStateButton_Click(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _ViewModel.ResetApplicationState();
        ResetCacheFlyout().Hide();
    }
}
