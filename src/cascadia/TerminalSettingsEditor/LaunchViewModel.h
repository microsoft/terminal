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

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> DefaultProfiles() const;

        IInspectable CurrentDefaultTerminal();
        void CurrentDefaultTerminal(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<Model::DefaultTerminal> DefaultTerminals() const;

        GETSET_BINDABLE_ENUM_SETTING(FirstWindowPreference, Model::FirstWindowPreference, _Settings.GlobalSettings().FirstWindowPreference);
        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, _Settings.GlobalSettings().LaunchMode);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, _Settings.GlobalSettings().WindowingBehavior);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), StartOnUserLogin);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialRows);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialCols);

    private:
        Model::CascadiaSettings _Settings;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(LaunchViewModel);
}
