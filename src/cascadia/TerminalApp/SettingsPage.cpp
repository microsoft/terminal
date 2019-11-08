// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsPage.h"
#include "GlobalSettingsContent.h"
#include "ProfilesSettingsContent.h"
#include "ColorSchemesSettingsContent.h"
#include "KeybindingsSettingsContent.h"
#include <winrt/Windows.UI.Xaml.Interop.h>

#include "SettingsPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace ::TerminalApp;

namespace winrt::TerminalApp::implementation
{
    SettingsPage::SettingsPage()
    {
        InitializeComponent();
    }

    SettingsPage::SettingsPage(std::shared_ptr<::TerminalApp::CascadiaSettings> settings)
    {
        InitializeComponent();

        _settings = settings;
    }

    void SettingsPage::Create()
    {
        //_globalSettingsPage = winrt::make_self<TerminalApp::GlobalSettingsContent>(); // comment out to build
        //_profileSettingsPage = winrt::make_self<TerminalApp::ProfilesSettingsContent>(); // comment out to build
        //_colorSchemesSettingsPage = winrt::make_self<TerminalApp::ColorSchemesSettingsContent>(); // comment out to build

        // TODO: THIS IS THE PROPER WAY OF DOING IT. THE ABOVE NEED SUPPORT FOR PASSING IN THE SETTINGS.
        //_keybindingsSettingsPage = winrt::make_self<TerminalApp::KeybindingsSettingsContent>(_settings); // comment out to build
    }

    void SettingsPage::SettingsNav_Loaded(IInspectable const&, RoutedEventArgs const&)
    {
        // set the initial SelectedItem
        for (uint32_t i = 0; i < SettingsNav().MenuItems().Size(); i++)
        {
            const auto item = SettingsNav().MenuItems().GetAt(i).as<Controls::NavigationViewItemBase>();
            const hstring globalsPage = L"Globals_Page";

            if (unbox_value<hstring>(item.Tag()) == globalsPage)
            {
                SettingsNav().SelectedItem() = item;
                SettingsNav().Header() = item.Tag();
                break;
            }
        }
        contentFrame().Navigate(xaml_typename<TerminalApp::GlobalSettingsContent>());
    }

    void SettingsPage::SettingsNav_SelectionChanged(Controls::NavigationView sender, Controls::NavigationViewSelectionChangedEventArgs args)
    {
    }

    void SettingsPage::SettingsNav_ItemInvoked(Controls::NavigationView sender, Controls::NavigationViewItemInvokedEventArgs args)
    {
        Controls::TextBlock item = args.InvokedItem().as<Controls::TextBlock>();

        if (item != NULL)
        {
            const hstring globalsNav = L"Globals_Nav";
            const hstring profilesNav = L"Profiles_Nav";
            const hstring colorSchemesNav = L"ColorSchemes_Nav";
            const hstring keybindingsNav = L"Keybindings_Nav";

            if (unbox_value<hstring>(item.Tag()) == globalsNav)
            {
                contentFrame().Navigate(xaml_typename<TerminalApp::GlobalSettingsContent>());
            }
            else if (unbox_value<hstring>(item.Tag()) == profilesNav)
            {
                contentFrame().Navigate(xaml_typename<TerminalApp::ProfilesSettingsContent>());
            }
            else if (unbox_value<hstring>(item.Tag()) == colorSchemesNav)
            {
                contentFrame().Navigate(xaml_typename<TerminalApp::ColorSchemesSettingsContent>());
            }
            else if (unbox_value<hstring>(item.Tag()) == keybindingsNav)
            {
                contentFrame().Navigate(xaml_typename<TerminalApp::KeybindingsSettingsContent>());
            }
        }
    }
}
