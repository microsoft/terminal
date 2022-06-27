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
        GlobalAppearanceViewModel(Model::GlobalAppSettings globalSettings);
        Model::GlobalAppSettings GlobalSettings();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        GETSET_BINDABLE_ENUM_SETTING(Theme, winrt::Windows::UI::Xaml::ElementTheme, _GlobalSettings.Theme);
        GETSET_BINDABLE_ENUM_SETTING(TabWidthMode, winrt::Microsoft::UI::Xaml::Controls::TabViewWidthMode, _GlobalSettings.TabWidthMode);

    public:
        // LanguageDisplayConverter maps the given BCP 47 tag to a localized string.
        // For instance "en-US" produces "English (United States)", while "de-DE" produces
        // "Deutsch (Deutschland)". This works independently of the user's locale.
        static winrt::hstring LanguageDisplayConverter(const winrt::hstring& tag);

        bool LanguageSelectorAvailable();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> LanguageList();
        winrt::Windows::Foundation::IInspectable CurrentLanguage();
        void CurrentLanguage(const winrt::Windows::Foundation::IInspectable& tag);

    private:
        Model::GlobalAppSettings _GlobalSettings;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> _languageList;
        winrt::Windows::Foundation::IInspectable _currentLanguage;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearanceViewModel);
}
