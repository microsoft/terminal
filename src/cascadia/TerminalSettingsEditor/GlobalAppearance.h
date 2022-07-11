// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "GlobalAppearancePageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct GlobalAppearancePageNavigationState : GlobalAppearancePageNavigationStateT<GlobalAppearancePageNavigationState>
    {
    public:
        GlobalAppearancePageNavigationState(const Model::GlobalAppSettings& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Model::GlobalAppSettings, Globals, nullptr)
    };

    struct GlobalAppearance : public HasScrollViewer<GlobalAppearance>, GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::GlobalAppearancePageNavigationState, State, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, State().Globals().TabWidthMode);

        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::Theme>, ThemeList, nullptr);

    public:
        // LanguageDisplayConverter maps the given BCP 47 tag to a localized string.
        // For instance "en-US" produces "English (United States)", while "de-DE" produces
        // "Deutsch (Deutschland)". This works independently of the user's locale.
        static winrt::hstring LanguageDisplayConverter(const winrt::hstring& tag);

        bool LanguageSelectorAvailable();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> LanguageList();
        winrt::Windows::Foundation::IInspectable CurrentLanguage();
        void CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag);

        winrt::Windows::Foundation::IInspectable CurrentTheme();
        void CurrentTheme(const winrt::Windows::Foundation::IInspectable& tag);
        static winrt::hstring ThemeNameConverter(const Model::Theme& theme);

    private:
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> _languageList;
        winrt::Windows::Foundation::IInspectable _currentLanguage;
        winrt::Windows::Foundation::IInspectable _currentTheme;

        void _UpdateThemeList();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
