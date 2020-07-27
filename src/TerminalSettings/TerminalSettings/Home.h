#pragma once

#include "Home.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Home : HomeT<Home>
    {
        Home();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Home : HomeT<Home, implementation::Home>
    {
    };
}
