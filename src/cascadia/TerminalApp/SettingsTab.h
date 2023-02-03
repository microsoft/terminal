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

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct SettingsTab : SettingsTabT<SettingsTab, TabBase>
    {
    public:
        SettingsTab(winrt::Microsoft::Terminal::Settings::Editor::MainPage settingsUI,
                    winrt::Windows::UI::Xaml::ElementTheme requestedTheme);

        void UpdateSettings(winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);
        void Focus(winrt::Windows::UI::Xaml::FocusState focusState) override;

        std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions() const override;

    private:
        winrt::Windows::UI::Xaml::ElementTheme _requestedTheme;

        void _MakeTabViewItem() override;
        winrt::fire_and_forget _CreateIcon();

        virtual winrt::Windows::UI::Xaml::Media::Brush _BackgroundBrush() override;
    };
}
