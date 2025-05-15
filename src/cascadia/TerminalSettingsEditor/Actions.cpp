// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Actions::Actions()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"Actions_AddNewTextBlock/Text"));
    }

    Automation::Peers::AutomationPeer Actions::OnCreateAutomationPeer()
    {
        _ViewModel.OnAutomationPeerAttached();
        return nullptr;
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ActionsViewModel>();
        _ViewModel.CurrentPage(ActionsSubPage::Base);
    }

    void Actions::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*eventArgs*/)
    {
        _ViewModel.AddNewCommand();
    }
}
