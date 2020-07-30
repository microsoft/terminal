#pragma once

#include "Profiles.g.h"
#include "ObjectModel/ProfileModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Profiles : ProfilesT<Profiles>
    {
        Profiles();
        Profiles(ObjectModel::ProfileModel profile);

        ObjectModel::ProfileModel ProfileModel();

        void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void cursorColorPickerConfirmColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void cursorColorPickerCancelColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void foregroundColorPickerConfirmColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void foregroundColorPickerCancelColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void backgroundColorPickerConfirmColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void backgroundColorPickerCancelColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void selectionBackgroundColorPickerConfirmColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void selectionBackgroundColorPickerCancelColor_Click(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

    private:
        ObjectModel::ProfileModel m_profileModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Profiles : ProfilesT<Profiles, implementation::Profiles>
    {
    };
}
