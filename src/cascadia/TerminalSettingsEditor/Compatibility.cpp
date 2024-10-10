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
    CompatibilityViewModel::CompatibilityViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TextMeasurement, TextMeasurement, winrt::Microsoft::Terminal::Control::TextMeasurement, L"Globals_TextMeasurement_", L"Text");
    }

    bool CompatibilityViewModel::DebugFeaturesAvailable() const noexcept
    {
        return Feature_DebugModeUI::IsEnabled();
    }

    Compatibility::Compatibility()
    {
        InitializeComponent();
    }

    void Compatibility::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::CompatibilityViewModel>();
    }
}
