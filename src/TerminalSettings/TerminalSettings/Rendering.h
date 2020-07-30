#pragma once

#include "Rendering.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

        private:
            ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Rendering : RenderingT<Rendering, implementation::Rendering>
    {
    };
}
