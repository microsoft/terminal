#pragma once

#include "GlobalAppearance.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance>
    {
        GlobalAppearance();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

        private:
            ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
        };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct GlobalAppearance : GlobalAppearanceT<GlobalAppearance, implementation::GlobalAppearance>
    {
    };
}
