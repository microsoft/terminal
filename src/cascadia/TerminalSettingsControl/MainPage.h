#pragma once

#include "MainPage.g.h"
#include "winrt/Microsoft.UI.Xaml.Controls.h"

namespace winrt::Microsoft::Terminal::Settings::Control::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        MainPage();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_SelectionChanged(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs args);
        void SettingsNav_ItemInvoked(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs args);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Control::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
