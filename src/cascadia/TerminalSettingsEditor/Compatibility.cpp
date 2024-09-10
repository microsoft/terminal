// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Compatibility.h"
#include "Compatibility.g.cpp"
#include "CompatibilityViewModel.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    CompatibilityViewModel::CompatibilityViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
    }

    bool CompatibilityViewModel::AllowVtChecksumReportAvailable() const noexcept
    {
        return Feature_VtChecksumReport::IsEnabled();
    }

    bool CompatibilityViewModel::AllowKeypadModeAvailable() const noexcept
    {
        return Feature_KeypadModeEnabled::IsEnabled();
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
