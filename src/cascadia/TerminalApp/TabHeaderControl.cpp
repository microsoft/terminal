// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabHeaderControl.h"

#include "TabHeaderControl.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TabHeaderControl::TabHeaderControl()
    {
        InitializeComponent();
    }

    void TabHeaderControl::UpdateHeaderText(winrt::hstring title)
    {
        HeaderTextBlock().Text(title);
    }

    void TabHeaderControl::SetZoomIcon(Windows::UI::Xaml::Visibility state)
    {
        HeaderZoomIcon().Visibility(state);
    }
}
