#pragma once

#include "Launch.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Launch : LaunchT<Launch>
    {
        Launch();
        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        private:
            ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };

;
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Launch : LaunchT<Launch, implementation::Launch>
    {
    };
}
