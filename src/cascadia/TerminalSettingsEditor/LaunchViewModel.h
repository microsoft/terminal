// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "LaunchViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct LaunchViewModel : LaunchViewModelT<LaunchViewModel>, ViewModelHelper<LaunchViewModel>
    {
    public:
        LaunchViewModel(Model::CascadiaSettings settings);
        Model::CascadiaSettings Settings() const;

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<IInspectable> DefaultProfiles() const;

        GETSET_BINDABLE_ENUM_SETTING(FirstWindowPreference, Model::FirstWindowPreference, _Settings.GlobalSettings().FirstWindowPreference);
        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, _Settings.GlobalSettings().LaunchMode);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, _Settings.GlobalSettings().WindowingBehavior);

    private:
        Model::CascadiaSettings _Settings;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(LaunchViewModel);
}
