// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionList.h"

#include "ActionList.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    ActionList::ActionList()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Action>();
        _allActions = winrt::single_threaded_vector<winrt::TerminalApp::Action>();

        Action action1{};
        action1.Name(L"Foo");
        action1.Command(winrt::TerminalApp::ShortcutAction::NewTab);

        Action action2{};
        action2.Name(L"Bar");
        action2.Command(winrt::TerminalApp::ShortcutAction::CloseTab);

        _filteredActions.Append(action1);
        _filteredActions.Append(action2);
    }

    Windows::Foundation::Collections::IObservableVector<Action> ActionList::FilteredActions()
    {
        return _filteredActions;
    }

    void ActionList::SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Action> const& actions)
    {
        _allActions = actions;

        _filteredActions.Clear();
        for (auto action : _allActions)
        {
            _filteredActions.Append(action);
        }
    }
}
