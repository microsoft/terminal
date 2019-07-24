#pragma once

#include "GlobalSettingsContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct GlobalSettingsContent : GlobalSettingsContentT<GlobalSettingsContent>
    {
        GlobalSettingsContent();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        /*void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);*/
        void Control1_Checked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void Control1_Unchecked(const winrt::Windows::Foundation::IInspectable sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct GlobalSettingsContent : GlobalSettingsContentT<GlobalSettingsContent, implementation::GlobalSettingsContent>
    {
    };
}
