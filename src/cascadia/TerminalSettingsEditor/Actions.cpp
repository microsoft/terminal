// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "KeyBindingViewModel.g.cpp"
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

        // subscribe to the view model's FocusContainer event
        // use the KeyBindingViewModel or index provided in the event to focus the corresponding container
        _ViewModel.FocusContainer([this](const auto& /*sender*/, const auto& args) {
            if (const auto& kbdVM = args.try_as<KeyBindingViewModel>())
            {
                if (const auto& container = KeyBindingsListView().ContainerFromItem(*kbdVM))
                {
                    container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                }
            }
            else if (const auto& index = args.try_as<uint32_t>())
            {
                if (const auto& container = KeyBindingsListView().ContainerFromIndex(*index))
                {
                    container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                }                
            }
        });
    }
}
