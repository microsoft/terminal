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

        winrt::hstring LaunchParametersCurrentValue();
        double InitialPosX();
        double InitialPosY();
        void InitialPosX(double xCoord);
        void InitialPosY(double yCoord);
        void UseDefaultLaunchPosition(bool useDefaultPosition);
        bool UseDefaultLaunchPosition();

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<Model::Profile> DefaultProfiles() const;

        IInspectable CurrentDefaultTerminal();
        void CurrentDefaultTerminal(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<Model::DefaultTerminal> DefaultTerminals() const;

        // We cannot use the macro for LaunchMode because we want to insert an event into the setter
        winrt::Windows::Foundation::IInspectable CurrentLaunchMode();
        void CurrentLaunchMode(const winrt::Windows::Foundation::IInspectable& enumEntry);
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> LaunchModeList();

        GETSET_BINDABLE_ENUM_SETTING(FirstWindowPreference, Model::FirstWindowPreference, _Settings.GlobalSettings().FirstWindowPreference);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, _Settings.GlobalSettings().WindowingBehavior);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), CenterOnLaunch);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), StartOnUserLogin);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialRows);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialCols);

    private:
        Model::CascadiaSettings _Settings;
        bool _useDefaultLaunchPosition;

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _LaunchModeList;
        winrt::Windows::Foundation::Collections::IMap<Model::LaunchMode, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _LaunchModeMap;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(LaunchViewModel);
}
