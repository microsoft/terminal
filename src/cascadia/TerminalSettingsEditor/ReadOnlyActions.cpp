// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ReadOnlyActions.h"
#include "ReadOnlyActions.g.cpp"
#include "ReadOnlyActionsPageNavigationState.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ReadOnlyActions::ReadOnlyActions()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<Command>();
    }

    void ReadOnlyActions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ReadOnlyActionsPageNavigationState>();

        std::vector<Command> keyBindingList;
        for (const auto& [_, command] : _State.Settings().GlobalSettings().ActionMap().NameMap())
        {
            // Filter out nested commands, and commands that aren't bound to a
            // key. This page is currently just for displaying the actions that
            // _are_ bound to keys.
            if (command.HasNestedCommands() || !command.Keys())
            {
                continue;
            }
            keyBindingList.push_back(command);
        }
        std::sort(begin(keyBindingList), end(keyBindingList), CommandComparator{});
        _filteredActions = single_threaded_observable_vector<Command>(std::move(keyBindingList));
    }

    Collections::IObservableVector<Command> ReadOnlyActions::FilteredActions()
    {
        return _filteredActions;
    }

    void ReadOnlyActions::_OpenSettingsClick(const IInspectable& /*sender*/,
                                             const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const bool altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto target = altPressed ? SettingsTarget::DefaultsFile : SettingsTarget::SettingsFile;

        _State.RequestOpenJson(target);
    }

}
