// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include "inc/cppwinrt_utils.h"

#include "ParentPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ParentPane : ParentPaneT<ParentPane>
    {
    public:
        ParentPane();
        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();
        void FocusPane(uint32_t id);

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{};
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(ParentPane);
}
