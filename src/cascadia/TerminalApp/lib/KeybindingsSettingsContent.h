#pragma once

#include "KeybindingsSettingsContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent>
    {
        KeybindingsSettingsContent();

        int32_t MyProperty();
        void MyProperty(int32_t value);

        //void ClickHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct KeybindingsSettingsContent : KeybindingsSettingsContentT<KeybindingsSettingsContent, implementation::KeybindingsSettingsContent>
    {
    };
}
