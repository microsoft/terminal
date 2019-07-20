// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TerminalPage.g.h"
#include "Tab.h"
#include "CascadiaSettings.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage>
    {
    public:
        TerminalPage();

        void SetSettings(::TerminalApp::CascadiaSettings* settings);

        void OnNewTabButtonClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::SplitButtonClickEventArgs const& args);

    private:
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Windows::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        ::TerminalApp::CascadiaSettings* _settings;

        std::vector<std::shared_ptr<Tab>> _tabs;

        void _Create();

        void _CreateNewTabFlyout();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
