#pragma once

#include "Profiles.g.h"

namespace winrt::SettingsControl::implementation
{
    struct Profiles : ProfilesT<Profiles>
    {
        Profiles();
        Profiles(winrt::hstring const& name);

        int32_t MyProperty();
        void MyProperty(int32_t value);
        winrt::hstring Name();
        void Name(winrt::hstring const& value);

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
        winrt::hstring m_name;
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    struct Profiles : ProfilesT<Profiles, implementation::Profiles>
    {
    };
}
