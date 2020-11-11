// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabHeaderControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl>
    {
        TabHeaderControl();
        void UpdateHeaderText(winrt::hstring title);
        void SetZoomIcon(Windows::UI::Xaml::Visibility state);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl, implementation::TabHeaderControl>
    {
    };
}
