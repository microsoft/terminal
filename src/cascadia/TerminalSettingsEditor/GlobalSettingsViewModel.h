// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalSettingsViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "..\TerminalSettingsModel\MTSMSettings.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalSettingsViewModel : GlobalSettingsViewModelT<GlobalSettingsViewModel>, ViewModelHelper<GlobalSettingsViewModel>
    {
    public:
        GlobalSettingsViewModel(const Model::GlobalAppSettings& globalSettings, const Model::CascadiaSettings& appSettings);

        bool ShowFirstWindowPreference() const noexcept;
        bool FeatureNotificationIconEnabled() const noexcept;
        void SetInvertedDisableAnimationsValue(bool invertedDisableAnimationsValue);

        IInspectable CurrentDefaultProfile();
        void CurrentDefaultProfile(const IInspectable& value);
        winrt::Windows::Foundation::Collections::IObservableVector<IInspectable> DefaultProfiles() const;

        // LanguageDisplayConverter maps the given BCP 47 tag to a localized string.
        // For instance "en-US" produces "English (United States)", while "de-DE" produces
        // "Deutsch (Deutschland)". This works independently of the user's locale.
        static winrt::hstring LanguageDisplayConverter(const winrt::hstring& tag);

        bool LanguageSelectorAvailable();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> LanguageList();
        winrt::Windows::Foundation::IInspectable CurrentLanguage();
        void CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag);

        GETSET_BINDABLE_ENUM_SETTING(FirstWindowPreference, Model::FirstWindowPreference, _globals, FirstWindowPreference);
        GETSET_BINDABLE_ENUM_SETTING(LaunchMode, Model::LaunchMode, _globals, LaunchMode);
        GETSET_BINDABLE_ENUM_SETTING(WindowingBehavior, Model::WindowingMode, _globals, WindowingBehavior);
        GETSET_BINDABLE_ENUM_SETTING(Theme, winrt::Windows::UI::Xaml::ElementTheme, _globals, Theme);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, _globals, TabWidthMode);
        GETSET_BINDABLE_ENUM_SETTING(TabSwitcherMode, Model::TabSwitcherMode, _globals, TabSwitcherMode);
        GETSET_BINDABLE_ENUM_SETTING(CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, _globals, CopyFormatting);

        _GET_SETTING_FUNC(_globals, DefaultProfile);
        _SET_SETTING_FUNC(_globals, DefaultProfile);
        _CLEAR_SETTING_FUNC(_globals, Language);

#define GLOBAL_SETTINGS_INITIALIZE(type, name, ...) \
    PERMANENT_OBSERVABLE_PROJECTED_SETTING(_globals, name)
        MTSM_GLOBAL_SETTINGS(GLOBAL_SETTINGS_INITIALIZE)
#undef GLOBAL_SETTINGS_INITIALIZE

    private:
        Model::GlobalAppSettings _globals;
        Model::CascadiaSettings _appSettings;

        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> _languageList;
        winrt::Windows::Foundation::IInspectable _currentLanguage;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalSettingsViewModel);
}
