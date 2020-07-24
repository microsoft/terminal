#pragma once

#include "Keybindings.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Keybindings : KeybindingsT<Keybindings>
    {
        Keybindings();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Keybindings : KeybindingsT<Keybindings, implementation::Keybindings>
    {
    };
}
