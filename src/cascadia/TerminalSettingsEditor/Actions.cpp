// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Actions.h"
#include "Actions.g.cpp"
#include "ActionsPageNavigationState.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Actions::Actions()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Model::Command>();
    }

    void Actions::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ActionsPageNavigationState>();

        for (const auto& [k, command] : _State.Settings().GlobalSettings().Commands())
        {
            // Filter out nested commands, and commands that aren't bound to a
            // key. This page is currently just for displaying the actions that
            // _are_ bound to keys.
            if (command.HasNestedCommands() || command.KeyChordText().empty())
            {
                continue;
            }
            _filteredActions.Append(command);
        }
    }

    Collections::IObservableVector<Command> Actions::FilteredActions()
    {
        return _filteredActions;
    }

}
