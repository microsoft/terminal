// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"

#include "CommandPalette.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;

namespace winrt::TerminalApp::implementation
{
    CommandPalette::CommandPalette()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Command>();
        _allActions = winrt::single_threaded_vector<winrt::TerminalApp::Command>();

        if (CommandPaletteShadow())
        {
            // Hook up the shadow on the command palette to the backdrop that
            // will actually show it. This needs to be done at runtime, and only
            // if the shadow actually exists. ThemeShadow isn't supported below
            // version 18362.
            CommandPaletteShadow().Receivers().Append(ShadowBackdrop());
            // "raise" the command palette up by 16 units, so it will cast a shadow.
            Backdrop().Translation({ 0, 0, 16 });
        }
    }

    // Method Description:
    // - Toggles the visibility of the command palette. This will auto-focus the
    //   input box within the palette.
    // - TODO GH#TODO: When we add support for commandline mode, accept a parameter here
    //   for which mode we should enter in.
    void CommandPalette::ToggleVisibility()
    {
        const bool isVisible = Visibility() == Visibility::Visible;
        if (!isVisible)
        {
            // Become visible
            Visibility(Visibility::Visible);
            _SearchBox().Focus(FocusState::Programmatic);
            _FilteredActionsView().SelectedIndex(0);
        }
        else
        {
            // Raise an event to return control to the Terminal.
            _close();
        }
    }

    // Method Description:
    // - Moves the focus up or down the list of commands. If we're at the top,
    //   we'll loop around to the bottom, and vice-versa.
    // Arguments:
    // - moveDown: if true, we're attempting to move to the next item in the
    //   list. Otherwise, we're attempting to move to the previous.
    // Return Value:
    // - <none>
    void CommandPalette::_selectNextItem(const bool moveDown)
    {
        const auto selected = _FilteredActionsView().SelectedIndex();
        const int numItems = ::base::saturated_cast<int>(_FilteredActionsView().Items().Size());
        // Wraparound math. By adding numItems and then calculating modulo numItems,
        // we clamp the values to the range [0, numItems) while still supporting moving
        // upward from 0 to numItems - 1.
        const auto newIndex = ((numItems + selected + (moveDown ? 1 : -1)) % numItems);
        _FilteredActionsView().SelectedIndex(newIndex);
        _FilteredActionsView().ScrollIntoView(_FilteredActionsView().SelectedItem());
    }

    // Method Description:
    // - Process keystrokes in the input box. This is used for moving focus up
    //   and down the list of commands in Action mode, and for executing
    //   commands in both Action mode and Commandline mode.
    // Arguments:
    // - e: the KeyRoutedEventArgs containing info about the keystroke.
    // Return Value:
    // - <none>
    void CommandPalette::_keyDownHandler(IInspectable const& /*sender*/,
                                         Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.OriginalKey();

        if (key == VirtualKey::Up)
        {
            // Action Mode: Move focus to the next item in the list.
            _selectNextItem(false);
            e.Handled(true);
        }
        else if (key == VirtualKey::Down)
        {
            // Action Mode: Move focus to the previous item in the list.
            _selectNextItem(true);
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter)
        {
            // Action Mode: Dispatch the action of the selected command.

            if (const auto selectedItem = _FilteredActionsView().SelectedItem())
            {
                if (const auto data = selectedItem.try_as<Command>())
                {
                    const auto actionAndArgs = data.Action();
                    _dispatch.DoAction(actionAndArgs);
                    _close();
                }
            }

            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
            // Action Mode: Dismiss the palette.
            _close();
        }
    }

    // Method Description:
    // - Event handler for when the text in the input box changes. In Action
    //   Mode, we'll update the list of displayed commands, and select the first one.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void CommandPalette::_filterTextChanged(IInspectable const& /*sender*/,
                                            Windows::UI::Xaml::RoutedEventArgs const& /*args*/)
    {
        _updateFilteredActions();
        _FilteredActionsView().SelectedIndex(0);
    }

    Collections::IObservableVector<Command> CommandPalette::FilteredActions()
    {
        return _filteredActions;
    }

    void CommandPalette::SetActions(Collections::IVector<TerminalApp::Command> const& actions)
    {
        _allActions = actions;
        _updateFilteredActions();
    }

    // Method Description:
    // - Update our list of filtered actions to reflect the current contents of
    //   the input box. For more details on which commands will be displayed,
    //   see `_filterMatchesName`.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_updateFilteredActions()
    {
        _filteredActions.Clear();
        auto searchText = _SearchBox().Text();
        const bool addAll = searchText.empty();

        // If there's no filter text, then just add all the commands in order to the list.
        if (addAll)
        {
            for (auto action : _allActions)
            {
                _filteredActions.Append(action);
            }
            return;
        }

        // Here, there was some filter text.
        // Show these actions in a weighted order.
        // - Matching the first character of a word, then the first char of a
        //   subsequent word seems better than just "the order they appear in
        //   the list".
        // - TODO GH#6647:"Recently used commands" ordering also seems valuable.

        auto compare = [searchText](const Command& left, const Command& right) {
            const int leftWeight = _getWeight(left.Name(), searchText);
            const int rightWeight = _getWeight(right.Name(), searchText);
            return leftWeight < rightWeight;
        };

        // Use a priority queue to order commands so that "better" matches
        // appear first in the list. The ordering will be determined by the
        // match weight produced by _getWeight.
        std::priority_queue<Command, std::vector<Command>, decltype(compare)> heap(compare);
        for (auto action : _allActions)
        {
            if (CommandPalette::_filterMatchesName(searchText, action.Name()))
            {
                heap.push(action);
            }
        }

        // Remove everything in-order from the queue, and add to the list of
        // filtered actions.
        while (!heap.empty())
        {
            _filteredActions.Append(heap.top());
            heap.pop();
        }
    }

    // Function Description:
    // - Determine if a command with the given `name` should be shown if the
    //   input box contains the string `searchText`. If all the characters of
    //   search text appear in order in `name`, then this function will return
    //   true. There can be any number of characters separating consecutive
    //   characters in searchText.
    //   * For example:
    //      "name": "New Tab"
    //      "name": "Close Tab"
    //      "name": "Close Pane"
    //      "name": "[-] Split Horizontal"
    //      "name": "[ | ] Split Vertical"
    //      "name": "Next Tab"
    //      "name": "Prev Tab"
    //      "name": "Open Settings"
    //      "name": "Open Media Controls"
    //   * "open" should return both "**Open** Settings" and "**Open** Media Controls".
    //   * "Tab" would return "New **Tab**", "Close **Tab**", "Next **Tab**" and "Prev
    //     **Tab**".
    //   * "P" would return "Close **P**ane", "[-] S**p**lit Horizontal", "[ | ]
    //     S**p**lit Vertical", "**P**rev Tab", "O**p**en Settings" and "O**p**en Media
    //     Controls".
    //   * "sv" would return "[ | ] Split Vertical" (by matching the **S** in
    //     "Split", then the **V** in "Vertical").
    // Arguments:
    // - searchText: the string of text to search for in `name`
    // - name: the name to check
    // Return Value:
    // - true if name contained all the characters of searchText
    bool CommandPalette::_filterMatchesName(const winrt::hstring& searchText, const winrt::hstring& name)
    {
        std::wstring lowercaseSearchText{ searchText.c_str() };
        std::wstring lowercaseName{ name.c_str() };
        std::transform(lowercaseSearchText.begin(), lowercaseSearchText.end(), lowercaseSearchText.begin(), std::towlower);
        std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), std::towlower);

        const wchar_t* namePtr = lowercaseName.c_str();
        const wchar_t* endOfName = lowercaseName.c_str() + lowercaseName.size();
        for (const auto& wch : lowercaseSearchText)
        {
            while (wch != *namePtr)
            {
                // increment the character to look at in the name string
                namePtr++;
                if (namePtr >= endOfName)
                {
                    // If we are at the end of the name string, we haven't found all the search chars
                    return false;
                }
            }
        }
        return true;
    }

    // Function Description:
    // - Calculates a "weighting" by which should be used to order a command
    //   name relative to other names, given a specific search string.
    //   Currently, this is based off of two factors:
    //   * The weight is incremented once for each matched character of the
    //     search text.
    //   * If a matching character from the search text was found at the start
    //     of a word in the name, then we increment the weight again.
    //     * For example, for a search string "sp", we want "Split Pane" to
    //       appear in the list before "Close Pane"
    // Arguments:
    // - searchText: the string of text to search for in `name`
    // - name: the name to check
    // Return Value:
    // - the relative weight of this match
    int CommandPalette::_getWeight(const winrt::hstring& searchText,
                                   const winrt::hstring& name)
    {
        std::wstring lowercaseSearchText{ searchText.c_str() };
        std::wstring lowercaseName{ name.c_str() };
        std::transform(lowercaseSearchText.begin(), lowercaseSearchText.end(), lowercaseSearchText.begin(), std::towlower);
        std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), std::towlower);

        int totalWeight = 0;
        bool lastCharWasSpace = true;
        const wchar_t* namePtr = lowercaseName.c_str();
        const wchar_t* endOfName = lowercaseName.c_str() + lowercaseName.size();
        for (const auto& wch : lowercaseSearchText)
        {
            while (wch != *namePtr)
            {
                // increment the character to look at in the name string
                namePtr++;
                if (namePtr >= endOfName)
                {
                    // If we are at the end of the name string, we haven't found all the search chars
                    return totalWeight;
                }
                lastCharWasSpace = *namePtr == L' ';
            }

            // We found the char, increment weight
            totalWeight++;
            totalWeight += lastCharWasSpace ? 1 : 0;
        }
        return totalWeight;
    }

    void CommandPalette::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    // Method Description:
    // - Dismiss the command palette. This will:
    //   * select all the current text in the input box
    //   * set our visibility to Hidden
    //   * raise our Closed event, so the page can return focus to the active Terminal
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_close()
    {
        Visibility(Visibility::Collapsed);

        // Clear the text box each time we close the dialog. This is consistent with VsCode.
        _SearchBox().Text(L"");
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

}
