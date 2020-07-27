#pragma once

#include "GlobalAppearance.g.h"

namespace winrt::SettingsControl::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
        GlobalAppearance();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance, implementation::GlobalAppearance>
    {
    };
}
