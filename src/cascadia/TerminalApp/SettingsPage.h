// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "SettingsPage.g.h"
#include "CascadiaSettings.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
    public:
        SettingsPage();
        SettingsPage(std::shared_ptr<::TerminalApp::CascadiaSettings> settings);

        void Create();

        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_SelectionChanged(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs args);
        void SettingsNav_ItemInvoked(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs args);

    private:
        std::shared_ptr<::TerminalApp::CascadiaSettings> _settings{ nullptr };

        // Navigation Pages
        std::unique_ptr<TerminalApp::GlobalSettingsContent> _globalSettingsPage{ nullptr };
        std::unique_ptr<TerminalApp::ProfilesSettingsContent> _profileSettingsPage{ nullptr };
        std::unique_ptr<TerminalApp::ColorSchemesSettingsContent> _colorSchemesSettingsPage{ nullptr };
        std::unique_ptr<TerminalApp::KeybindingsSettingsContent> _keybindingsSettingsPage{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
