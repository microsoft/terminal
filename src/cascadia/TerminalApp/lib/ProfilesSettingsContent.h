#pragma once

#include "ProfilesSettingsContent.g.h"
#include "CascadiaSettings.h"

namespace winrt::TerminalApp::implementation
{
    struct ProfilesSettingsContent : ProfilesSettingsContentT<ProfilesSettingsContent>
    {
        ProfilesSettingsContent();

        int32_t MyProperty();
        void MyProperty(int32_t value);
        std::unique_ptr<::TerminalApp::CascadiaSettings> _settings;

        void LoadProfiles(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void Control1_Checked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void Control1_Unchecked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ProfilesSettingsContent : ProfilesSettingsContentT<ProfilesSettingsContent, implementation::ProfilesSettingsContent>
    {
    };
}
