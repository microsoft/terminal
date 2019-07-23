// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "ActionList.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionList : ActionListT<ActionList>
    {
        ActionList();

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> FilteredActions();
        void SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Action> const& actions);

    private:
        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Action> _allActions{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ActionList : ActionListT<ActionList, implementation::ActionList>
    {
    };
}
