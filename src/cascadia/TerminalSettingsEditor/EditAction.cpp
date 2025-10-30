// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EditAction.h"
#include "EditAction.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    EditAction::EditAction()
    {
        InitializeComponent();
    }

    void EditAction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToCommandArgs>();
        _ViewModel = args.Command();
        _ViewModel.PropagateWindowRootRequested([windowRoot = args.WindowRoot()](const IInspectable& /*sender*/, const Editor::ArgWrapper& wrapper) {
            if (wrapper)
            {
                wrapper.WindowRoot(windowRoot);
            }
        });
        auto weakThis = get_weak();
        _ViewModel.FocusContainer([weakThis](const auto& /*sender*/, const auto& args) {
            if (auto page{ weakThis.get() })
            {
                if (auto kcVM{ args.try_as<KeyChordViewModel>() })
                {
                    if (const auto& container = page->KeyChordListView().ContainerFromItem(*kcVM))
                    {
                        container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                    }
                }
            }
        });
        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            CommandNameTextBox().Focus(FocusState::Programmatic);
        });
    }
}
