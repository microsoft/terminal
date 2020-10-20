/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SettingsTab.h

Abstract:
- The SettingsTab is a tab whose content is a Settings UI control. They can
  coexist in a TabView with all other types of tabs, like the TerminalTab.
  There should only be at most one SettingsTab open at any given time.

Author(s):
- Leon Liang - October 2020

--*/

#pragma once
#include "SettingsTab.g.h"
#include <winrt/TerminalApp.h>
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsTab : SettingsTabT<SettingsTab>
    {
    public:
        SettingsTab(winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);
        void Focus(winrt::Windows::UI::Xaml::FocusState focusState);
        winrt::Windows::UI::Xaml::FocusState FocusState() const noexcept;

        void Shutdown();

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);
        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        GETSET_PROPERTY(winrt::hstring, Title, L"Settings");
        GETSET_PROPERTY(winrt::hstring, Icon);
        GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Command, SwitchToTabCommand, nullptr);
        GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewItem, TabViewItem, nullptr);
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::Page, Content, nullptr);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        // This is needed since Tab is going to be managing its own SwitchToTab command.
        OBSERVABLE_GETSET_PROPERTY(uint32_t, TabViewIndex, _PropertyChangedHandlers, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        OBSERVABLE_GETSET_PROPERTY(uint32_t, TabViewNumTabs, _PropertyChangedHandlers, 0);

    private:
        winrt::Windows::UI::Xaml::FocusState _focusState{ winrt::Windows::UI::Xaml::FocusState::Unfocused };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeOtherTabsMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeTabsAfterMenuItem{};
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _MakeTabViewItem();
        void _CreateContextMenu();
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem _CreateCloseSubMenu();
        void _EnableCloseMenuItems();
        winrt::fire_and_forget _CreateIcon();
        void _CloseTabsAfter();
        void _CloseOtherTabs();
    };
}
