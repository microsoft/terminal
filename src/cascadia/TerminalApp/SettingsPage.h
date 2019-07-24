// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "SettingsPage.g.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage>
    {
        SettingsPage();

        void SettingsNav_Loaded(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);
        void SettingsNav_SelectionChanged(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs args);
        void SettingsNav_ItemInvoked(Windows::UI::Xaml::Controls::NavigationView sender, Windows::UI::Xaml::Controls::NavigationViewItemInvokedEventArgs args);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct SettingsPage : SettingsPageT<SettingsPage, implementation::SettingsPage>
    {
    };
}
