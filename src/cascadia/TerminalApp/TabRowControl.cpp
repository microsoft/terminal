// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabRowControl.h"

#include "TabRowControl.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TabRowControl::TabRowControl()
    {
        InitializeComponent();
    }

    // Method Description:
    // - Bound in the Xaml editor to the [+] button.
    // Arguments:
    // <unused>
    void TabRowControl::OnNewTabButtonClick(IInspectable const&, Controls::SplitButtonClickEventArgs const&)
    {
    }
}
