// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabRowControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabRowControl : TabRowControlT<TabRowControl>
    {
        TabRowControl();

        void HideDragBar();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TabRowControl : TabRowControlT<TabRowControl, implementation::TabRowControl>
    {
    };
}
