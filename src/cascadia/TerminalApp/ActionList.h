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

        // void OnNewTabButtonClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::SplitButtonClickEventArgs const& args);

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> FilteredActions();

    private:
        Windows::Foundation::Collections::IObservableVector<TerminalApp::Action> _filteredActions{ nullptr };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct ActionList : ActionListT<ActionList, implementation::ActionList>
    {
    };
}
