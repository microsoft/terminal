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
#include "TabBase.h"
#include "SettingsTab.g.h"

namespace winrt::TerminalApp::implementation
{
    struct SettingsTab : SettingsTabT<SettingsTab, TabBase>
    {
    public:
        SettingsTab(winrt::Microsoft::Terminal::Settings::Editor::MainPage settingsUI,
                    WUX::ElementTheme requestedTheme);

        void UpdateSettings(MTSM::CascadiaSettings settings);
        void Focus(WUX::FocusState focusState) override;

        std::vector<MTSM::ActionAndArgs> BuildStartupActions() const override;

    private:
        WUX::ElementTheme _requestedTheme;

        void _MakeTabViewItem() override;
        winrt::fire_and_forget _CreateIcon();

        virtual WUXMedia::Brush _BackgroundBrush() override;
    };
}
