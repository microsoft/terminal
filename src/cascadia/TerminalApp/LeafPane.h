// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include "inc/cppwinrt_utils.h"

#include "LeafPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct LeafPane : LeafPaneT<LeafPane>
    {
        LeafPane();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(LeafPane);
}
