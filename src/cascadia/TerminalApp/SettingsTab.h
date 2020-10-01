// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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
        SettingsTab();

        winrt::Windows::UI::Xaml::UIElement GetTabContent();

        bool IsFocused() const noexcept;
        void SetFocused(const bool focused);

        winrt::hstring GetActiveTitle() const;

        void UpdateTabViewIndex(const uint32_t idx);

        void Shutdown();

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);

        GETSET_PROPERTY(winrt::hstring, Title, L"Settings");
        GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::IconSource, IconSource, nullptr);
        GETSET_PROPERTY(winrt::TerminalApp::Command, SwitchToTabCommand, nullptr);
        GETSET_PROPERTY(uint32_t, TabViewIndex, 0);
        GETSET_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewItem, TabViewItem, nullptr);

    private:
        winrt::Microsoft::Terminal::Settings::Editor::MainPage _settingsUI{ nullptr };

        bool _focused{ false };
        void _Focus();

        void _MakeTabViewItem();
        void _CreateContextMenu();
        void _MakeSwitchToTabCommand();
        winrt::fire_and_forget _CreateIcon();
    };
}
