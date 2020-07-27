#pragma once

#include "Interaction.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Interaction : InteractionT<Interaction, implementation::Interaction>
    {
    };
}
