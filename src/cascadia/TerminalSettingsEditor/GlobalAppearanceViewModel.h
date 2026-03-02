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
        GlobalAppearanceViewModel(Model::GlobalAppSettings globalSettings, Model::WindowSettings windowSettings);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<GlobalAppearanceViewModel>::PropertyChanged;

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::Theme>, ThemeList, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(NewTabPosition, Model::NewTabPosition, _windowSettings.NewTabPosition);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, _windowSettings.TabWidthMode);

    public:
        winrt::Windows::Foundation::IInspectable CurrentTheme();
        void CurrentTheme(const winrt::Windows::Foundation::IInspectable& tag);
        static winrt::hstring ThemeNameConverter(const Model::Theme& theme);

        bool InvertedDisableAnimations();
        void InvertedDisableAnimations(bool value);

        void ShowTitlebarToggled(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, AlwaysShowTabs);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, ShowTabsFullscreen);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, ShowTabsInTitlebar);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, UseAcrylicInTabRow);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, ShowTitleInTitlebar);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, AlwaysOnTop);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, AutoHideWindow);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AlwaysShowNotificationIcon);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, MinimizeToNotificationArea);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, ShowAdminShield);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_windowSettings, EnableUnfocusedAcrylic);

    private:
        Model::WindowSettings _windowSettings;
        Model::GlobalAppSettings _GlobalSettings;
        winrt::Windows::Foundation::IInspectable _currentTheme;

        void _UpdateThemeList();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearanceViewModel);
}
