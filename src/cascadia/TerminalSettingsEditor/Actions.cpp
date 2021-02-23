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

        for (const auto& [k, v] : _State.Settings().GlobalSettings().Commands())
        {
            _filteredActions.Append(v);
        }
    }

    Collections::IObservableVector<Command> Actions::FilteredActions()
    {
        return _filteredActions;
    }

}
