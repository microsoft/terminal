// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"

#include "TerminalPage.g.cpp"

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage() :
        _settings{ nullptr }
    {
        InitializeComponent();
    }

    void TerminalPage::SetSettings(::TerminalApp::CascadiaSettings* settings)
    {
        if (!_settings)
        {
            _settings = settings;
        }
    }

    void TerminalPage::_Create()
    {
        _tabContent = this->TabContent();
        _tabView = this->TabView();
        _tabRow = this->TabRow();
        _newTabButton = this->NewTabButton();

        //Event Bindings (Early)
    }

    void TerminalPage::_CreateNewTabFlyout()
    {
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
