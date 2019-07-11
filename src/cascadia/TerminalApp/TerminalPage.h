// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TerminalPage.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage>
    {
        TerminalPage();

        void OnNewTabButtonClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::SplitButtonClickEventArgs const& args);
        void TabView_SelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
