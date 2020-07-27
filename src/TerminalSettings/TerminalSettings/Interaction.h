#pragma once

#include "Interaction.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Interaction : InteractionT<Interaction, implementation::Interaction>
    {
    };
}
