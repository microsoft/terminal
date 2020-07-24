#pragma once

#include "Globals.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Globals : GlobalsT<Globals>
    {
        Globals();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Globals : GlobalsT<Globals, implementation::Globals>
    {
    };
}
