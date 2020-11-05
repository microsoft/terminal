// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Utils.h"
#include "MainPage.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        MainPage() = delete;
        MainPage(winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);

        void OpenJsonKeyDown(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& args);
        void OpenJsonTapped(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_ItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView const& sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args);
        void SettingsNav_BackRequested(Microsoft::UI::Xaml::Controls::NavigationView const&, Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& args);
        bool On_BackRequested();

        TYPED_EVENT(OpenJson, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Settings::Model::SettingsTarget);

    private:
        // XAML should data-bind to the _settingsClone
        // When "save" is pressed, _settingsSource = _settingsClone
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settingsSource;
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settingsClone;
        winrt::Windows::Foundation::Collections::IMap<winrt::Microsoft::Terminal::Settings::Model::Profile, winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem> _profileToNavItemMap;

        void _InitializeProfilesList();
        void _CreateAndNavigateToNewProfile(const uint32_t index);
        winrt::Microsoft::UI::Xaml::Controls::NavigationViewItem _CreateProfileNavViewItem(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);

        static void _Navigate(Windows::UI::Xaml::Controls::Frame contentFrame, hstring clickedItemTag, Model::CascadiaSettings settings);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPage);
}
