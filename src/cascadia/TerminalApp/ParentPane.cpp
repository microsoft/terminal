// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ParentPane.h"

#include "ParentPane.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    ParentPane::ParentPane()
    {
        InitializeComponent();
    }

    Controls::Grid ParentPane::GetRootElement()
    {
        return _root;
    }

    void ParentPane::FocusPane(uint32_t /*id*/)
    {
    }
}
