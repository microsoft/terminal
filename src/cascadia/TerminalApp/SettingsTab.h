// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ColorPickupFlyout.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsTab : public winrt::implements<SettingsTab, ITab>
    {
    public:
        SettingsTab() = delete;
        SettingsTab(winrt::Windows::UI::Xaml::UIElement settingsUI);

        winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
        winrt::Windows::UI::Xaml::UIElement GetRootElement();

        bool IsFocused() const noexcept;
        void SetFocused(const bool focused);

        winrt::fire_and_forget UpdateIcon(const winrt::hstring iconPath);

        winrt::hstring GetActiveTitle() const;

        void Shutdown();

        void UpdateTabViewIndex(const uint32_t idx);

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::IconSource, IconSource, _PropertyChangedHandlers, nullptr);
        OBSERVABLE_GETSET_PROPERTY(winrt::TerminalApp::Command, SwitchToTabCommand, _PropertyChangedHandlers, nullptr);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        // This is needed since Tab is going to be managing its own SwitchToTab command.
        OBSERVABLE_GETSET_PROPERTY(uint32_t, TabViewIndex, _PropertyChangedHandlers, 0);

    private:
        winrt::hstring _lastIconPath{};

        bool _focused{ false };
        winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };
        winrt::Windows::UI::Xaml::UIElement _settingsUI{ nullptr };

        void _MakeTabViewItem();
        void _Focus();

        void _CreateContextMenu();

        winrt::fire_and_forget _UpdateTitle();

        void _MakeSwitchToTabCommand();
    };
}
