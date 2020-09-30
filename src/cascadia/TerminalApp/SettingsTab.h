// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "SettingsTab.g.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsTab : SettingsTabT<SettingsTab>
    {
    public:
        SettingsTab() = delete;
        SettingsTab(winrt::Windows::UI::Xaml::UIElement settingsUI);

        winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
        winrt::Windows::UI::Xaml::UIElement GetRootElement();

        bool IsFocused() const noexcept;
        void SetFocused(const bool focused);

        winrt::fire_and_forget UpdateIcon();

        winrt::hstring GetActiveTitle() const;

        void Shutdown();

        void UpdateTabViewIndex(const uint32_t idx);

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);

        GETSET_PROPERTY(winrt::hstring, Title, L"Settings");
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::IconSource, IconSource, nullptr);
        GETSET_PROPERTY(winrt::TerminalApp::Command, SwitchToTabCommand, nullptr);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        // This is needed since Tab is going to be managing its own SwitchToTab command.
        GETSET_PROPERTY(uint32_t, TabViewIndex, 0);

    private:
        bool _focused{ false };
        winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };
        winrt::Windows::UI::Xaml::UIElement _settingsUI{ nullptr };

        void _MakeTabViewItem();
        void _Focus();

        void _CreateContextMenu();

        void _MakeSwitchToTabCommand();
    };
}
