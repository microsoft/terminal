// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
#include "ActionAndArgs.h"
#include "ActionArgs.h"
#include "Command.h"

#include "CommandPalette.g.cpp"
#include <winrt/Microsoft.Terminal.Settings.h>

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::TerminalApp::implementation
{
    CommandPalette::CommandPalette()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Command>();
        _allActions = winrt::single_threaded_vector<winrt::TerminalApp::Command>();
        _allTabActions = winrt::single_threaded_vector<winrt::TerminalApp::Command>();

        if (CommandPaletteShadow())
        {
            // Hook up the shadow on the command palette to the backdrop that
            // will actually show it. This needs to be done at runtime, and only
            // if the shadow actually exists. ThemeShadow isn't supported below
            // version 18362.
            CommandPaletteShadow().Receivers().Append(_shadowBackdrop());
            // "raise" the command palette up by 16 units, so it will cast a shadow.
            _backdrop().Translation({ 0, 0, 16 });
        }

        // Whatever is hosting us will enable us by setting our visibility to
        // "Visible". When that happens, set focus to our search box.
        RegisterPropertyChangedCallback(UIElement::VisibilityProperty(), [this](auto&&, auto&&) {

            if (Visibility() == Visibility::Visible)
            {
                _searchBox().Focus(FocusState::Programmatic);
                _filteredActionsView().SelectedIndex(0);
            }
            else
            {
                // Raise an event to return control to the Terminal.
                _close();
            }
        });
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
        const auto selected = _filteredActionsView().SelectedIndex();
        const int numItems = ::base::saturated_cast<int>(_filteredActionsView().Items().Size());
        // Wraparound math. By adding numItems and then calculating modulo numItems,
        // we clamp the values to the range [0, numItems) while still supporting moving
        // upward from 0 to numItems - 1.
        const auto newIndex = ((numItems + selected + (moveDown ? 1 : -1)) % numItems);
        _filteredActionsView().SelectedIndex(newIndex);
        _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());
    }

    void CommandPalette::_previewKeyDownHandler(IInspectable const& /*sender*/,
                                                Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.OriginalKey();

        // Some keypresses such as Tab, Return, Esc, and Arrow Keys are ignored by controls because
        // they're not considered input key presses. While they don't raise KeyDown events,
        // they do raise PreviewKeyDown events.
        if (_tabSwitcherMode && key == VirtualKey::Tab)
        {
            _selectNextItem(true);
            e.Handled(true);
        }
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

            if (const auto selectedItem = _filteredActionsView().SelectedItem())
            {
                if (const auto data = selectedItem.try_as<TerminalApp::Command>())
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
            // Action Mode: Dismiss the palette if the text is empty, otherwise clear the search string.
            if (_searchBox().Text().empty())
            {
                _close();
            }
            else
            {
                _searchBox().Text(L"");
            }

            e.Handled(true);
        }
    }

    void CommandPalette::_keyUpHandler(IInspectable const& /*sender*/,
                                       Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.OriginalKey();

        if (_tabSwitcherMode)
        {
            if (_anchorKey && key == _anchorKey.value())
            {
                // Once the user lifts the anchor key, we'll switch to the currently selected tab
                // then close the tab switcher.

                if (const auto selectedItem = _filteredActionsView().SelectedItem())
                {
                    if (const auto data = selectedItem.try_as<TerminalApp::Command>())
                    {
                        const auto actionAndArgs = data.Action();
                        _dispatch.DoAction(actionAndArgs);
                        _tabSwitcherMode = false;
                        _updateFilteredActions();
                        _close();
                    }
                }

                e.Handled(true);
            }
        }
    }

    // Method Description:
    // - This event is triggered when someone clicks anywhere in the bounds of
    //   the window that's _not_ the command palette UI. When that happens,
    //   we'll want to dismiss the palette.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void CommandPalette::_rootPointerPressed(Windows::Foundation::IInspectable const& /*sender*/,
                                             Windows::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
    {
        _close();
    }

    // Method Description:
    // - This event is only triggered when someone clicks in the space right
    //   next to the text box in the command palette. We _don't_ want that click
    //   to light dismiss the palette, so we'll mark it handled here.
    // Arguments:
    // - e: the PointerRoutedEventArgs that we want to mark as handled
    // Return Value:
    // - <none>
    void CommandPalette::_backdropPointerPressed(Windows::Foundation::IInspectable const& /*sender*/,
                                                 Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        e.Handled(true);
    }

    // Method Description:
    // - This event is called when the user clicks on an individual item from
    //   the list. We'll get the item that was clicked and dispatch the command
    //   that the user clicked on.
    // Arguments:
    // - e: an ItemClickEventArgs who's ClickedItem() will be the command that was clicked on.
    // Return Value:
    // - <none>
    void CommandPalette::_listItemClicked(Windows::Foundation::IInspectable const& /*sender*/,
                                          Windows::UI::Xaml::Controls::ItemClickEventArgs const& e)
    {
        if (auto command{ e.ClickedItem().try_as<TerminalApp::Command>() })
        {
            const auto actionAndArgs = command.Action();
            _dispatch.DoAction(actionAndArgs);
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
        _filteredActionsView().SelectedIndex(0);

        _noMatchesText().Visibility(_filteredActions.Size() > 0 ? Visibility::Collapsed : Visibility::Visible);
    }

    Collections::IObservableVector<TerminalApp::Command> CommandPalette::FilteredActions()
    {
        return _filteredActions;
    }

    void CommandPalette::SetCommandPaletteActions(Collections::IVector<TerminalApp::Command> const& actions)
    {
        _allCommandPaletteActions = actions;
        _updateFilteredActions();
    }

    void CommandPalette::EnableCommandPaletteMode()
    {
        _allActions = _allCommandPaletteActions;
        _updateFilteredActions();
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareCommandNames(const TerminalApp::Command& lhs, const TerminalApp::Command& rhs)
    {
        std::wstring_view leftName{ lhs.Name() };
        std::wstring_view rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    // This is a helper to sort entries when in Tab Switcher mode.
    // This compares the KeyChordText, which is used to indicate the Tab index on the Tab View.
    static bool _compareTabIndex(const TerminalApp::Command& lhs, const TerminalApp::Command& rhs)
    {
        std::wstring_view leftIndex{ lhs.KeyChordText() };
        std::wstring_view rightIndex{ rhs.KeyChordText() };
        return leftIndex.compare(rightIndex) > 0;
    }

    // This is a helper struct to aid in sorting Commands by a given weighting.
    struct WeightedCommand
    {
        TerminalApp::Command command;
        int weight;
        bool orderByTabIndex;

        bool operator<(const WeightedCommand& other) const
        {
            // If two commands have the same weight, then we'll sort them alphabetically.
            if (weight == other.weight)
            {
                if (orderByTabIndex)
                {
                    return _compareTabIndex(command, other.command);
                }
                return !_compareCommandNames(command, other.command);
            }
            return weight < other.weight;
        }
    };

    // Method Description:
    // - Update our list of filtered actions to reflect the current contents of
    //   the input box. For more details on which commands will be displayed,
    //   see `_getWeight`.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_updateFilteredActions()
    {
        _filteredActions.Clear();
        auto searchText = _searchBox().Text();
        const bool addAll = searchText.empty();

        // If there's no filter text, then just add all the commands in order to the list.
        // - TODO GH#6647:Possibly add the MRU commands first in order, followed
        //   by the rest of the commands.
        if (addAll)
        {
            // If TabSwitcherMode, just all add as is.
            if (_tabSwitcherMode)
            {
                for (auto action : _allActions)
                {
                    _filteredActions.Append(action);
                }

                return;
            }

            // Add all the commands, but make sure they're sorted alphabetically.
            std::vector<TerminalApp::Command> sortedCommands;
            sortedCommands.reserve(_allActions.Size());

            for (auto action : _allActions)
            {
                sortedCommands.push_back(action);
            }
            std::sort(sortedCommands.begin(),
                      sortedCommands.end(),
                      _compareCommandNames);

            for (auto action : sortedCommands)
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
        //      * This could be done by weighting the recently used commands
        //        higher the more recently they were used, then weighting all
        //        the unused commands as 1

        // Use a priority queue to order commands so that "better" matches
        // appear first in the list. The ordering will be determined by the
        // match weight produced by _getWeight.
        std::priority_queue<WeightedCommand> heap;
        for (auto action : _allActions)
        {
            const auto weight = CommandPalette::_getWeight(searchText, action.Name());
            if (weight > 0)
            {
                WeightedCommand wc;
                wc.command = action;
                wc.weight = weight;

                // If we're in Tab Switcher mode, we want to fall back to Index
                // order, not Command Name alphabetical order.
                wc.orderByTabIndex = _tabSwitcherMode;

                heap.push(wc);
            }
        }

        // At this point, all the commands in heap are matches. We've also
        // sorted commands with the same weight alphabetically.
        // Remove everything in-order from the queue, and add to the list of
        // filtered actions.
        while (!heap.empty())
        {
            auto top = heap.top();
            heap.pop();
            _filteredActions.Append(top.command);
        }
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
    //   * Consecutive matches will be weighted higher than matches with
    //     characters in between the search characters.
    // - This will return 0 if the command should not be shown. If all the
    //   characters of search text appear in order in `name`, then this function
    //   will return a positive number. There can be any number of characters
    //   separating consecutive characters in searchText.
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
    // - the relative weight of this match
    int CommandPalette::_getWeight(const winrt::hstring& searchText,
                                   const winrt::hstring& name)
    {
        int totalWeight = 0;
        bool lastWasSpace = true;

        auto it = name.cbegin();

        for (auto searchChar : searchText)
        {
            searchChar = std::towlower(searchChar);
            // Advance the iterator to the next character that we're looking
            // for.

            bool lastWasMatch = true;
            while (true)
            {
                // If we are at the end of the name string, we haven't found
                // it.
                if (it == name.cend())
                {
                    return false;
                }

                // found it
                if (std::towlower(*it) == searchChar)
                {
                    break;
                }

                lastWasSpace = *it == L' ';
                ++it;
                lastWasMatch = false;
            }

            // Advance the iterator by one character so that we don't
            // end up on the same character in the next iteration.
            ++it;

            totalWeight += 1;
            totalWeight += lastWasSpace ? 1 : 0;
            totalWeight += (lastWasMatch) ? 1 : 0;
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
        _searchBox().Text(L"");
    }

    void CommandPalette::OnTabsChanged(const IInspectable& s, const IVectorChangedEventArgs& e)
    {
        if (auto tabList = s.try_as<IObservableVector<TerminalApp::Tab>>())
        {
            auto idx = e.Index();
            auto changedEvent = e.CollectionChange();

            switch (changedEvent)
            {
                case CollectionChange::ItemChanged: {
                    auto tab = tabList.GetAt(idx);
                    GenerateCommandForTab(idx, false, tab);
                    break;
                }
                case CollectionChange::ItemInserted: {
                    auto tab = tabList.GetAt(idx);
                    GenerateCommandForTab(idx, true, tab);
                    UpdateTabIndices(idx);
                    break;
                }
                case CollectionChange::ItemRemoved: {
                    _allTabActions.RemoveAt(idx);
                    UpdateTabIndices(idx);
                    _updateFilteredActions();
                    break;
                }
            }
        }
    }

    void CommandPalette::UpdateTabIndices(const uint32_t startIdx)
    {
        if (startIdx != _allTabActions.Size() - 1)
        {
            for (auto i = startIdx + 1; i < _allTabActions.Size(); ++i)
            {
                auto command = _allTabActions.GetAt(i);

                command.Action().Args().as<implementation::SwitchToTabArgs>()->TabIndex(i);

                command.KeyChordText(L"index : " + to_hstring(i));
            }
        }
    }

    void CommandPalette::GenerateCommandForTab(const uint32_t idx, bool inserted, TerminalApp::Tab& tab)
    {
        auto focusTabAction = winrt::make_self<implementation::ActionAndArgs>();
        auto args = winrt::make_self<implementation::SwitchToTabArgs>();
        args->TabIndex(idx);

        focusTabAction->Action(ShortcutAction::SwitchToTab);
        focusTabAction->Args(*args);

        auto command = winrt::make_self<implementation::Command>();
        command->Action(*focusTabAction);
        command->KeyChordText(L"index : " + to_hstring(idx));
        command->Name(tab.Title());

        // Listen for changes to this particular Tab's title so we can update the corresponding Command name.
        auto weakThis{ get_weak() };
        auto weakCommand{ command->get_weak() };
        tab.PropertyChanged([weakThis, weakCommand, tab](auto&&, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
            auto palette{ weakThis.get() };
            auto command{ weakCommand.get() };
            if (palette && command)
            {
                if (args.PropertyName() == L"Title")
                {
                    command->Name(tab.Title());
                }
            }
        });

        if (inserted)
        {
            _allTabActions.InsertAt(idx, *command);
        }
        else
        {
            _allTabActions.SetAt(idx, *command);
        }

        // We don't need to update the filtered actions if the
        // command palette's actions aren't tab switcher actions.
        if (_tabSwitcherMode)
        {
            _updateFilteredActions();
        }
    }

    void CommandPalette::EnableTabSwitcherMode(const VirtualKey& anchorKey)
    {
        _anchorKey = anchorKey;
        _tabSwitcherMode = true;
        _allActions = _allTabActions;
        _updateFilteredActions();
    }
}
