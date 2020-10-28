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

        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_ItemInvoked(Microsoft::UI::Xaml::Controls::NavigationView const& sender, Microsoft::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs const& args);
        void SettingsNav_BackRequested(Microsoft::UI::Xaml::Controls::NavigationView const&, Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& args);
        bool On_BackRequested();
        static void Navigate(Windows::UI::Xaml::Controls::Frame contentFrame, hstring clickedItemTag);

        static winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings Settings();

    private:
        // XAML should data-bind to the _settingsClone
        // When "save" is pressed, _settingsSource = _settingsClone
        static winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settingsSource;
        winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings _settingsClone{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(MainPage);
}
