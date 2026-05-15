/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsExpander

Abstract:
- A collapsable container for grouping multiple SettingsCards. Based
  on the Windows Community Toolkit's SettingsExpander.

Author(s):
- Carlos Zamora - 2026 (port from CommunityToolkit.WinUI.Controls.SettingsExpander)

--*/

#pragma once

#include "SettingsExpander.g.h"
#include "SettingsExpanderAutomationPeer.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct SettingsExpander : SettingsExpanderT<SettingsExpander>
    {
    public:
        SettingsExpander();

        void OnApplyTemplate();

        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        til::typed_event<Editor::SettingsExpander, Windows::Foundation::IInspectable> Expanded;
        til::typed_event<Editor::SettingsExpander, Windows::Foundation::IInspectable> Collapsed;

        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Header);
        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Description);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::Controls::IconElement, HeaderIcon);
        DEPENDENCY_PROPERTY(Windows::Foundation::IInspectable, Content);
        DEPENDENCY_PROPERTY(bool, IsExpanded);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::UIElement, ItemsHeader);
        DEPENDENCY_PROPERTY(Windows::UI::Xaml::UIElement, ItemsFooter);

    private:
        static void _InitializeProperties();
        static void _OnIsExpandedChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
    };

    // AutomationPeer for SettingsExpander. Reports class name and falls back to
    // Header text for the name when AutomationProperties.Name is unset.
    struct SettingsExpanderAutomationPeer : SettingsExpanderAutomationPeerT<SettingsExpanderAutomationPeer>
    {
    public:
        SettingsExpanderAutomationPeer(const Editor::SettingsExpander& owner);

        Windows::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        hstring GetClassNameCore() const;
        hstring GetNameCore() const;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(SettingsExpander);
    BASIC_FACTORY(SettingsExpanderAutomationPeer);
}
