#pragma once

#include "Interaction.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Interaction : InteractionT<Interaction>
    {
        Interaction();

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

    private:
        ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Interaction : InteractionT<Interaction, implementation::Interaction>
    {
    };
}
