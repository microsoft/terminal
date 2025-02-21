// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "LaunchViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"
#include <cppwinrt_utils.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct LaunchViewModel : LaunchViewModelT<LaunchViewModel>, ViewModelHelper<LaunchViewModel>
    {
    public:
        LaunchViewModel(Model::CascadiaSettings settings);

        // LanguageDisplayConverter maps the given BCP 47 tag to a localized string.
        // For instance "en-US" produces "English (United States)", while "de-DE" produces
        // "Deutsch (Deutschland)". This works independently of the user's locale.
        static winrt::hstring LanguageDisplayConverter(const winrt::hstring& tag);

        bool LanguageSelectorAvailable();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> LanguageList();
        winrt::Windows::Foundation::IInspectable CurrentLanguage();
        void CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag);

        winrt::hstring LaunchSizeCurrentValue() const;
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

        GETSET_BINDABLE_ENUM_SETTING(DefaultInputScope, winrt::Microsoft::Terminal::Control::DefaultInputScope, _Settings.GlobalSettings().DefaultInputScope);
        GETSET_BINDABLE_ENUM_SETTING(FirstWindowPreference, Model::FirstWindowPreference, _Settings.GlobalSettings().FirstWindowPreference);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, _Settings.GlobalSettings().WindowingBehavior);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), CenterOnLaunch);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialRows);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_Settings.GlobalSettings(), InitialCols);

        bool StartOnUserLoginAvailable();
        safe_void_coroutine PrepareStartOnUserLoginSettings();
        bool StartOnUserLoginConfigurable();
        winrt::hstring StartOnUserLoginStatefulHelpText();
        bool StartOnUserLogin();
        safe_void_coroutine StartOnUserLogin(bool enable);

    private:
        Model::CascadiaSettings _Settings;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> _languageList;
        winrt::Windows::Foundation::IInspectable _currentLanguage;
        bool _useDefaultLaunchPosition;

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _LaunchModeList;
        winrt::Windows::Foundation::Collections::IMap<Model::LaunchMode, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _LaunchModeMap;

        winrt::Windows::ApplicationModel::StartupTask _startOnUserLoginTask{ nullptr };
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(LaunchViewModel);
}
