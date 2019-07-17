// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"

#include "TerminalPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage()
    {
        InitializeComponent();
    }

    // Method Description:
    // - Bound in the Xaml editor to the [+] button.
    // Arguments:
    // - sender
    // - event arguments
    void TerminalPage::OnNewTabButtonClick(IInspectable const&, Controls::SplitButtonClickEventArgs const&)
    {
    }
}
