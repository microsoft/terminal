// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearanceViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearanceViewModel : GlobalAppearanceViewModelT<GlobalAppearanceViewModel>, ViewModelHelper<GlobalAppearanceViewModel>
    {
    public:
        GlobalAppearanceViewModel(MTSM::GlobalAppSettings globalSettings);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<GlobalAppearanceViewModel>::PropertyChanged;

        WINRT_PROPERTY(WFC::IObservableVector<MTSM::Theme>, ThemeList, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(NewTabPosition, MTSM::NewTabPosition, _GlobalSettings.NewTabPosition);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, MUXC::TabViewWidthMode, _GlobalSettings.TabWidthMode);

    public:
        // LanguageDisplayConverter maps the given BCP 47 tag to a localized string.
        // For instance "en-US" produces "English (United States)", while "de-DE" produces
        // "Deutsch (Deutschland)". This works independently of the user's locale.
        static winrt::hstring LanguageDisplayConverter(const winrt::hstring& tag);

        bool LanguageSelectorAvailable();
        WFC::IObservableVector<winrt::hstring> LanguageList();
        WF::IInspectable CurrentLanguage();
        void CurrentLanguage(const WF::IInspectable& tag);

        WF::IInspectable CurrentTheme();
        void CurrentTheme(const WF::IInspectable& tag);
        static winrt::hstring ThemeNameConverter(const MTSM::Theme& theme);

        bool InvertedDisableAnimations();
        void InvertedDisableAnimations(bool value);

        void ShowTitlebarToggled(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AlwaysShowTabs);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, ShowTabsInTitlebar);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, UseAcrylicInTabRow);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, ShowTitleInTitlebar);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AlwaysOnTop);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AutoHideWindow);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AlwaysShowNotificationIcon);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, MinimizeToNotificationArea);

    private:
        MTSM::GlobalAppSettings _GlobalSettings;
        WFC::IObservableVector<winrt::hstring> _languageList;
        WF::IInspectable _currentLanguage;
        WF::IInspectable _currentTheme;

        void _UpdateThemeList();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearanceViewModel);
}
