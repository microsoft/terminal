// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"
#include "ActionAndArgs.h"
#include "ActionArgs.h"
#include "Command.h"

#include <LibraryResources.h>

#include "CommandPalette.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::TerminalApp::implementation
{
    CommandPalette::CommandPalette() :
        _anchorKey{ VirtualKey::None },
        _switcherStartIdx{ 0 }
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Command>();
        _allCommands = winrt::single_threaded_vector<winrt::TerminalApp::Command>();
        _allTabActions = winrt::single_threaded_vector<winrt::TerminalApp::Command>();

        _switchToMode(CommandPaletteMode::ActionMode);

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
                if (_currentMode == CommandPaletteMode::TabSwitcherMode)
                {
                    if (_anchorKey != VirtualKey::None)
                    {
                        _searchBox().Visibility(Visibility::Collapsed);
                        _filteredActionsView().Focus(FocusState::Keyboard);
                    }
                    else
                    {
                        _searchBox().Focus(FocusState::Programmatic);
                    }

                    _filteredActionsView().SelectedIndex(_switcherStartIdx);
                    _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());
                }
                else
                {
                    _searchBox().Focus(FocusState::Programmatic);
                    _filteredActionsView().SelectedIndex(0);
                }

                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "CommandPaletteOpened",
                    TraceLoggingDescription("Event emitted when the Command Palette is opened"),
                    TraceLoggingWideString(L"Action", "Mode", "which mode the palette was opened in"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
            }
            else
            {
                // Raise an event to return control to the Terminal.
                _dismissPalette();
            }
        });

        // Focusing the ListView when the Command Palette control is set to Visible
        // for the first time fails because the ListView hasn't finished loading by
        // the time Focus is called. Luckily, We can listen to SizeChanged to know
        // when the ListView has been measured out and is ready, and we'll immediately
        // revoke the handler because we only needed to handle it once on initialization.
        _sizeChangedRevoker = _filteredActionsView().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (_currentMode == CommandPaletteMode::TabSwitcherMode && _anchorKey != VirtualKey::None)
            {
                _filteredActionsView().Focus(FocusState::Keyboard);
            }
            _sizeChangedRevoker.revoke();
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
        //
        // Only give anchored tab switcher the ability to cycle through tabs with the tab button.
        // For unanchored mode, accessibility becomes an issue when we try to hijack tab since it's
        // a really widely used keyboard navigation key.
        if (_currentMode == CommandPaletteMode::TabSwitcherMode &&
            key == VirtualKey::Tab &&
            _anchorKey != VirtualKey::None)
        {
            auto const state = CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift);
            if (WI_IsFlagSet(state, CoreVirtualKeyStates::Down))
            {
                _selectNextItem(false);
                e.Handled(true);
            }
            else
            {
                _selectNextItem(true);
                e.Handled(true);
            }
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
                _dispatchCommand(selectedItem.try_as<TerminalApp::Command>());
            }

            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
            // Action Mode: Dismiss the palette if the text is empty, otherwise clear the search string.
            if (_searchBox().Text().empty())
            {
                _dismissPalette();
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

        if (_currentMode == CommandPaletteMode::TabSwitcherMode)
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
                        _updateFilteredActions();
                        _dismissPalette();
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
        _dismissPalette();
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
        _dispatchCommand(e.ClickedItem().try_as<TerminalApp::Command>());
    }

    // Method Description:
    // - Retrieve the list of commands that we should currently be filtering.
    //   * If the user has command with subcommands, this will return that command's subcommands.
    //   * If we're in Tab Switcher mode, return the tab actions.
    //   * Otherwise, just return the list of all the top-level commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - A list of Commands to filter.
    Collections::IVector<TerminalApp::Command> CommandPalette::_commandsToFilter()
    {
        switch (_currentMode)
        {
        case CommandPaletteMode::ActionMode:
            return _allCommands;
        case CommandPaletteMode::TabSwitcherMode:
            return _allTabActions;
        default:
            return _allCommands;
        }
    }

    // Method Description:
    // - Helper method for retrieving the action from a command the user
    //   selected, and dispatching that command. Also fires a tracelogging event
    //   indicating that the user successfully found the action they were
    //   looking for.
    // Arguments:
    // - command: the Command to dispatch. This might be null.
    // Return Value:
    // - <none>
    void CommandPalette::_dispatchCommand(const TerminalApp::Command& command)
    {
        if (command)
        {
            // Close before we dispatch so that actions that open the command
            // palette like the Tab Switcher will be able to have the last laugh.
            _close();

            const auto actionAndArgs = command.Action();
            _dispatch.DoAction(actionAndArgs);

            TraceLoggingWrite(
                g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                "CommandPaletteDispatchedAction",
                TraceLoggingDescription("Event emitted when the user selects an action in the Command Palette"),
                TraceLoggingUInt32(_searchBox().Text().size(), "SearchTextLength", "Number of characters in the search string"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
        }
    }

    // Method Description:
    // - Helper method for closing the command palette, when the user has _not_
    //   selected an action. Also fires a tracelogging event indicating that the
    //   user closed the palette without running a command.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_dismissPalette()
    {
        _close();

        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "CommandPaletteDismissed",
            TraceLoggingDescription("Event emitted when the user dismisses the Command Palette without selecting an action"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
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

    void CommandPalette::SetCommands(Collections::IVector<TerminalApp::Command> const& actions)
    {
        _allCommands = actions;
        _updateFilteredActions();
    }

    void CommandPalette::EnableCommandPaletteMode()
    {
        _switchToMode(CommandPaletteMode::ActionMode);
        _updateFilteredActions();
    }

    void CommandPalette::_switchToMode(CommandPaletteMode mode)
    {
        // The smooth remove/add animations that happen during
        // UpdateFilteredActions don't work very well when switching between
        // modes because of the sheer amount of remove/adds. So, let's just
        // clear + append when switching between modes.
        if (mode != _currentMode)
        {
            _currentMode = mode;
            _filteredActions.Clear();
            auto commandsToFilter = _commandsToFilter();

            for (auto action : commandsToFilter)
            {
                _filteredActions.Append(action);
            }
        }

        // Leaving this block of code outside the above if-statement
        // guarantees that the correct text is shown for the mode
        // whenever _switchToMode is called.
        switch (_currentMode)
        {
        case CommandPaletteMode::TabSwitcherMode:
        {
            SearchBoxText(RS_(L"TabSwitcher_SearchBoxText"));
            NoMatchesText(RS_(L"TabSwitcher_NoMatchesText"));
            ControlName(RS_(L"TabSwitcherControlName"));
            break;
        }
        case CommandPaletteMode::ActionMode:
        default:
            SearchBoxText(RS_(L"CommandPalette_SearchBox/PlaceholderText"));
            NoMatchesText(RS_(L"CommandPalette_NoMatchesText/Text"));
            ControlName(RS_(L"CommandPaletteControlName"));
            break;
        }
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareCommandNames(const TerminalApp::Command& lhs, const TerminalApp::Command& rhs)
    {
        std::wstring_view leftName{ lhs.Name() };
        std::wstring_view rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    // This is a helper struct to aid in sorting Commands by a given weighting.
    struct WeightedCommand
    {
        TerminalApp::Command command;
        int weight;
        int inOrderCounter;

        bool operator<(const WeightedCommand& other) const
        {
            if (weight == other.weight)
            {
                // If two commands have the same weight, then we'll sort them alphabetically.
                // If they both have the same name, fall back to the order in which they were
                // pushed into the heap.
                if (command.Name() == other.command.Name())
                {
                    return inOrderCounter > other.inOrderCounter;
                }
                else
                {
                    return !_compareCommandNames(command, other.command);
                }
            }
            return weight < other.weight;
        }
    };

    // Method Description:
    // - Produce a list of filtered actions to reflect the current contents of
    //   the input box. For more details on which commands will be displayed,
    //   see `_getWeight`.
    // Arguments:
    // - A collection that will receive the filtered actions
    // Return Value:
    // - <none>
    std::vector<winrt::TerminalApp::Command> CommandPalette::_collectFilteredActions()
    {
        std::vector<winrt::TerminalApp::Command> actions;

        auto searchText = _searchBox().Text();
        const bool addAll = searchText.empty();

        auto commandsToFilter = _commandsToFilter();

        // If there's no filter text, then just add all the commands in order to the list.
        // - TODO GH#6647:Possibly add the MRU commands first in order, followed
        //   by the rest of the commands.
        if (addAll)
        {
            // If TabSwitcherMode, just add all as is. We don't want
            // them to be sorted alphabetically.
            if (_currentMode == CommandPaletteMode::TabSwitcherMode)
            {
                for (auto action : commandsToFilter)
                {
                    actions.push_back(action);
                }

                return actions;
            }

            // Add all the commands, but make sure they're sorted alphabetically.
            std::vector<TerminalApp::Command> sortedCommands;
            sortedCommands.reserve(commandsToFilter.Size());

            for (auto action : commandsToFilter)
            {
                sortedCommands.push_back(action);
            }
            std::sort(sortedCommands.begin(),
                      sortedCommands.end(),
                      _compareCommandNames);

            for (auto action : sortedCommands)
            {
                actions.push_back(action);
            }

            return actions;
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

        // TODO GH#7205: Find a better way to ensure that WCs of the same
        // weight and name stay in the order in which they were pushed onto
        // the PQ.
        uint32_t counter = 0;
        for (auto action : commandsToFilter)
        {
            const auto weight = CommandPalette::_getWeight(searchText, action.Name());
            if (weight > 0)
            {
                WeightedCommand wc;
                wc.command = action;
                wc.weight = weight;
                wc.inOrderCounter = counter++;

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
            actions.push_back(top.command);
        }

        return actions;
    }

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
        auto actions = _collectFilteredActions();

        // Make _filteredActions look identical to actions, using only Insert and Remove.
        // This allows WinUI to nicely animate the ListView as it changes.
        for (uint32_t i = 0; i < _filteredActions.Size() && i < actions.size(); i++)
        {
            for (uint32_t j = i; j < _filteredActions.Size(); j++)
            {
                if (_filteredActions.GetAt(j) == actions[i])
                {
                    for (uint32_t k = i; k < j; k++)
                    {
                        _filteredActions.RemoveAt(i);
                    }
                    break;
                }
            }

            if (_filteredActions.GetAt(i) != actions[i])
            {
                _filteredActions.InsertAt(i, actions[i]);
            }
        }

        // Remove any extra trailing items from the destination
        while (_filteredActions.Size() > actions.size())
        {
            _filteredActions.RemoveAtEnd();
        }

        // Add any extra trailing items from the source
        while (_filteredActions.Size() < actions.size())
        {
            _filteredActions.Append(actions[_filteredActions.Size()]);
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

        // Reset visibility in case anchor mode tab switcher just finished.
        _searchBox().Visibility(Visibility::Visible);

        // Clear the text box each time we close the dialog. This is consistent with VsCode.
        _searchBox().Text(L"");
    }

    // Method Description:
    // - Listens for changes to TerminalPage's _tabs vector. Updates our vector of
    //   tab switching commands accordingly.
    // Arguments:
    // - s: The vector being listened to.
    // - e: The vector changed args that tells us whether a change, insert, or removal was performed
    //      on the listened-to vector.
    // Return Value:
    // - <none>
    void CommandPalette::OnTabsChanged(const IInspectable& s, const IVectorChangedEventArgs& e)
    {
        if (auto tabList = s.try_as<IObservableVector<TerminalApp::Tab>>())
        {
            auto idx = e.Index();
            auto changedEvent = e.CollectionChange();

            switch (changedEvent)
            {
            case CollectionChange::ItemChanged:
            {
                winrt::com_ptr<Command> item;
                item.copy_from(winrt::get_self<Command>(_allTabActions.GetAt(idx)));
                item->propertyChangedRevoker.revoke();

                auto tab = tabList.GetAt(idx);
                GenerateCommandForTab(idx, false, tab);
                UpdateTabIndices(idx);
                break;
            }
            case CollectionChange::ItemInserted:
            {
                auto tab = tabList.GetAt(idx);
                GenerateCommandForTab(idx, true, tab);
                UpdateTabIndices(idx);
                break;
            }
            case CollectionChange::ItemRemoved:
            {
                winrt::com_ptr<Command> item;
                item.copy_from(winrt::get_self<Command>(_allTabActions.GetAt(idx)));
                item->propertyChangedRevoker.revoke();

                _allTabActions.RemoveAt(idx);
                UpdateTabIndices(idx);
                break;
            }
            }

            _updateFilteredActions();
        }
    }

    // Method Description:
    // - In the case where a tab is removed or reordered, the given indices of
    //   the tab switch commands following the removed/reordered tab will get out of sync by 1
    //   (e.g. if tab 1 is removed, tabs 2,3,4,... need to become tabs 1,2,3,...)
    //   This function just loops through the tabs following startIdx and adjusts their given indices.
    // Arguments:
    // - startIdx: The index to start the update loop at.
    // Return Value:
    // - <none>
    void CommandPalette::UpdateTabIndices(const uint32_t startIdx)
    {
        for (auto i = startIdx; i < _allTabActions.Size(); ++i)
        {
            auto command = _allTabActions.GetAt(i);

            command.Action().Args().as<implementation::SwitchToTabArgs>()->TabIndex(i);
        }
    }

    // Method Description:
    // - Create a tab switching command based on the given tab object and insert/update the command
    //   at the given index. The command will call a SwitchToTab action on the given idx.
    // Arguments:
    // - idx: The index to insert or update the tab switch command.
    // - tab: The tab object to refer to when creating the tab switch command.
    // Return Value:
    // - <none>
    void CommandPalette::GenerateCommandForTab(const uint32_t idx, bool inserted, TerminalApp::Tab& tab)
    {
        auto focusTabAction = winrt::make_self<implementation::ActionAndArgs>();
        auto args = winrt::make_self<implementation::SwitchToTabArgs>();
        args->TabIndex(idx);

        focusTabAction->Action(ShortcutAction::SwitchToTab);
        focusTabAction->Args(*args);

        auto command = winrt::make_self<implementation::Command>();
        command->Action(*focusTabAction);
        command->Name(tab.Title());
        command->IconSource(tab.IconSource());

        // Listen for changes to the Tab so we can update this Command's attributes accordingly.
        auto weakThis{ get_weak() };
        auto weakCommand{ command->get_weak() };
        command->propertyChangedRevoker = tab.PropertyChanged(winrt::auto_revoke, [weakThis, weakCommand, tab](auto&&, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args) {
            auto palette{ weakThis.get() };
            auto command{ weakCommand.get() };

            if (palette && command)
            {
                if (args.PropertyName() == L"Title")
                {
                    if (command->Name() != tab.Title())
                    {
                        command->Name(tab.Title());
                    }
                }
                if (args.PropertyName() == L"IconSource")
                {
                    if (command->IconSource() != tab.IconSource())
                    {
                        command->IconSource(tab.IconSource());
                    }
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
    }

    void CommandPalette::EnableTabSwitcherMode(const VirtualKey& anchorKey, const uint32_t startIdx)
    {
        _switcherStartIdx = startIdx;
        _anchorKey = anchorKey;
        _switchToMode(CommandPaletteMode::TabSwitcherMode);
        _updateFilteredActions();
    }
}
