// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"

#include <LibraryResources.h>

#include "CommandPalette.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    CommandPalette::CommandPalette() :
        _switcherStartIdx{ 0 }
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<Command>();
        _nestedActionStack = winrt::single_threaded_vector<Command>();
        _currentNestedCommands = winrt::single_threaded_vector<Command>();
        _allCommands = winrt::single_threaded_vector<Command>();
        _allTabActions = winrt::single_threaded_vector<Command>();

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
                if (_currentMode == CommandPaletteMode::TabSwitchMode)
                {
                    _searchBox().Visibility(Visibility::Collapsed);
                    _filteredActionsView().Focus(FocusState::Keyboard);
                    _filteredActionsView().SelectedIndex(_switcherStartIdx);
                    _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());

                    // Do this right after becoming visible so we can quickly catch scenarios where
                    // modifiers aren't held down (e.g. command palette invocation).
                    _anchorKeyUpHandler();
                }
                else
                {
                    _searchBox().Focus(FocusState::Programmatic);
                    _updateFilteredActions();
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
            if (_currentMode == CommandPaletteMode::TabSwitchMode)
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
    void CommandPalette::SelectNextItem(const bool moveDown)
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
        if (_currentMode == CommandPaletteMode::TabSwitchMode && key == VirtualKey::Tab)
        {
            auto const state = CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift);
            if (WI_IsFlagSet(state, CoreVirtualKeyStates::Down))
            {
                SelectNextItem(false);
                e.Handled(true);
            }
            else
            {
                SelectNextItem(true);
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
            SelectNextItem(false);
            e.Handled(true);
        }
        else if (key == VirtualKey::Down)
        {
            // Action Mode: Move focus to the previous item in the list.
            SelectNextItem(true);
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter)
        {
            // Action, TabSwitch or TabSearchMode Mode: Dispatch the action of the selected command.
            if (_currentMode != CommandPaletteMode::CommandlineMode)
            {
                if (const auto selectedItem = _filteredActionsView().SelectedItem())
                {
                    _dispatchCommand(selectedItem.try_as<Command>());
                }
            }
            // Commandline Mode: Use the input to synthesize an ExecuteCommandline action
            else if (_currentMode == CommandPaletteMode::CommandlineMode)
            {
                _dispatchCommandline();
            }

            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
            // Action, TabSearch, TabSwitch Mode: Dismiss the palette if the
            // text is empty, otherwise clear the search string.
            if (_currentMode != CommandPaletteMode::CommandlineMode)
            {
                if (_searchBox().Text().empty())
                {
                    _dismissPalette();
                }
                else
                {
                    _searchBox().Text(L"");
                }
            }
            else if (_currentMode == CommandPaletteMode::CommandlineMode)
            {
                const auto currentInput = _getPostPrefixInput();
                if (currentInput.empty())
                {
                    // The user's only input "> " so far. We should just dismiss
                    // the palette. This is like dismissing the Action mode with
                    // empty input.
                    _dismissPalette();
                }
                else
                {
                    // Clear out the current input. We'll leave a ">" in the
                    // input (to stay in commandline mode), and a leading space
                    // (if they currently had one).
                    const bool hasLeadingSpace = (_searchBox().Text().size()) - (currentInput.size()) > 1;
                    _searchBox().Text(hasLeadingSpace ? L"> " : L">");

                    // This will conveniently move the cursor to the end of the
                    // text input for us.
                    _searchBox().Select(_searchBox().Text().size(), 0);
                }
            }

            e.Handled(true);
        }
        else
        {
            const auto vkey = ::gsl::narrow_cast<WORD>(e.OriginalKey());

            // In the interest of not telling all modes to check for keybindings, limit to TabSwitch mode for now.
            if (_currentMode == CommandPaletteMode::TabSwitchMode)
            {
                auto const ctrlDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);
                auto const altDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Menu), CoreVirtualKeyStates::Down);
                auto const shiftDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift), CoreVirtualKeyStates::Down);

                auto success = _bindings.TryKeyChord({
                    ctrlDown,
                    altDown,
                    shiftDown,
                    vkey,
                });

                if (success)
                {
                    e.Handled(true);
                }
            }
        }
    }

    // Method Description:
    // - Implements the Alt handler
    // Return value:
    // - whether the key was handled
    bool CommandPalette::OnDirectKeyEvent(const uint32_t vkey, const uint8_t /*scanCode*/, const bool down)
    {
        auto handled = false;
        if (_currentMode == CommandPaletteMode::TabSwitchMode)
        {
            if (vkey == VK_MENU && !down)
            {
                _anchorKeyUpHandler();
                handled = true;
            }
        }
        return handled;
    }

    void CommandPalette::_keyUpHandler(IInspectable const& /*sender*/,
                                       Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (_currentMode == CommandPaletteMode::TabSwitchMode)
        {
            _anchorKeyUpHandler();
            e.Handled(true);
        }
    }

    // Method Description:
    // - Handles anchor key ups during TabSwitchMode.
    //   We assume that at least one modifier key should be held down in order to "anchor"
    //   the ATS UI in place. So this function is called to check if any modifiers are
    //   still held down, and if not, dispatch the selected tab action and close the ATS.
    // Return value:
    // - <none>
    void CommandPalette::_anchorKeyUpHandler()
    {
        auto const ctrlDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);
        auto const altDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Menu), CoreVirtualKeyStates::Down);
        auto const shiftDown = WI_IsFlagSet(CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift), CoreVirtualKeyStates::Down);

        if (!ctrlDown && !altDown && !shiftDown)
        {
            if (const auto selectedItem = _filteredActionsView().SelectedItem())
            {
                if (const auto data = selectedItem.try_as<Command>())
                {
                    _dispatchCommand(data);
                }
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
        _dispatchCommand(e.ClickedItem().try_as<Command>());
    }

    // Method Description:
    // - This is called when the user selects a command with subcommands. It
    //   will update our UI to now display the list of subcommands instead, and
    //   clear the search text so the user can search from the new list of
    //   commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_updateUIForStackChange()
    {
        if (_searchBox().Text().empty())
        {
            // Manually call _filterTextChanged, because setting the text to the
            // empty string won't update it for us (as it won't actually change value.)
            _filterTextChanged(nullptr, nullptr);
        }

        // Changing the value of the search box will trigger _filterTextChanged,
        // which will cause us to refresh the list of filterable commands.
        _searchBox().Text(L"");
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
    Collections::IVector<Command> CommandPalette::_commandsToFilter()
    {
        switch (_currentMode)
        {
        case CommandPaletteMode::ActionMode:
            if (_nestedActionStack.Size() > 0)
            {
                return _currentNestedCommands;
            }

            return _allCommands;
        case CommandPaletteMode::TabSearchMode:
        case CommandPaletteMode::TabSwitchMode:
            return _allTabActions;
        case CommandPaletteMode::CommandlineMode:
            return winrt::single_threaded_vector<Command>();
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
    void CommandPalette::_dispatchCommand(const Command& command)
    {
        if (command)
        {
            if (command.HasNestedCommands())
            {
                // If this Command had subcommands, then don't dispatch the
                // action. Instead, display a new list of commands for the user
                // to pick from.
                _nestedActionStack.Append(command);
                ParentCommandName(command.Name());
                _currentNestedCommands.Clear();
                for (const auto& nameAndCommand : command.NestedCommands())
                {
                    _currentNestedCommands.Append(nameAndCommand.Value());
                }

                _updateUIForStackChange();
            }
            else
            {
                // First stash the search text length, because _close will clear this.
                const auto searchTextLength = _searchBox().Text().size();

                // An action from the root command list has depth=0
                const auto nestedCommandDepth = _nestedActionStack.Size();

                // Close before we dispatch so that actions that open the command
                // palette like the Tab Switcher will be able to have the last laugh.
                _close();

                const auto actionAndArgs = command.Action();
                _dispatch.DoAction(actionAndArgs);

                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "CommandPaletteDispatchedAction",
                    TraceLoggingDescription("Event emitted when the user selects an action in the Command Palette"),
                    TraceLoggingUInt32(searchTextLength, "SearchTextLength", "Number of characters in the search string"),
                    TraceLoggingUInt32(nestedCommandDepth, "NestedCommandDepth", "the depth in the tree of commands for the dispatched action"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
            }
        }
    }

    // Method Description:
    // - Get all the input text in _searchBox that follows the prefix character
    //   and any whitespace following that prefix character. This can be used in
    //   commandline mode to get all the useful input that the user input after
    //   the leading ">" prefix.
    // - Note that this will behave unexpectedly in Action Mode.
    // Arguments:
    // - <none>
    // Return Value:
    // - the string of input following the prefix character.
    std::wstring CommandPalette::_getPostPrefixInput()
    {
        const std::wstring input{ _searchBox().Text() };
        if (input.empty())
        {
            return input;
        }

        const auto rawCmdline{ input.substr(1) };

        // Trim leading whitespace
        const auto firstNonSpace = rawCmdline.find_first_not_of(L" ");
        if (firstNonSpace == std::wstring::npos)
        {
            // All the following characters are whitespace.
            return L"";
        }

        return rawCmdline.substr(firstNonSpace);
    }

    // Method Description:
    // - Dispatch the current search text as a ExecuteCommandline action.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_dispatchCommandline()
    {
        const auto input = _getPostPrefixInput();
        if (input.empty())
        {
            return;
        }
        winrt::hstring cmdline{ input };

        // Build the ExecuteCommandline action from the values we've parsed on the commandline.
        ExecuteCommandlineArgs args{ cmdline };
        ActionAndArgs executeActionAndArgs{ ShortcutAction::ExecuteCommandline, args };

        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "CommandPaletteDispatchedCommandline",
            TraceLoggingDescription("Event emitted when the user runs a commandline in the Command Palette"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        if (_dispatch.DoAction(executeActionAndArgs))
        {
            _close();
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
        if (_currentMode == CommandPaletteMode::CommandlineMode || _currentMode == CommandPaletteMode::ActionMode)
        {
            _evaluatePrefix();
        }

        _updateFilteredActions();
        _filteredActionsView().SelectedIndex(0);

        _noMatchesText().Visibility(_filteredActions.Size() > 0 ? Visibility::Collapsed : Visibility::Visible);
    }

    void CommandPalette::_evaluatePrefix()
    {
        auto newMode = CommandPaletteMode::ActionMode;

        auto inputText = _searchBox().Text();
        if (inputText.size() > 0)
        {
            if (inputText[0] == L'>')
            {
                newMode = CommandPaletteMode::CommandlineMode;
            }
        }

        if (newMode != _currentMode)
        {
            _switchToMode(newMode);
        }
    }

    Collections::IObservableVector<Command> CommandPalette::FilteredActions()
    {
        return _filteredActions;
    }

    void CommandPalette::SetKeyBindings(Microsoft::Terminal::TerminalControl::IKeyBindings bindings)
    {
        _bindings = bindings;
    }

    void CommandPalette::SetCommands(Collections::IVector<Command> const& actions)
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
        case CommandPaletteMode::TabSearchMode:
        case CommandPaletteMode::TabSwitchMode:
        {
            SearchBoxText(RS_(L"TabSwitcher_SearchBoxText"));
            NoMatchesText(RS_(L"TabSwitcher_NoMatchesText"));
            ControlName(RS_(L"TabSwitcherControlName"));
            break;
        }
        case CommandPaletteMode::CommandlineMode:
            NoMatchesText(RS_(L"CmdPalCommandlinePrompt"));
            ControlName(RS_(L"CommandPaletteControlName"));
            break;
        case CommandPaletteMode::ActionMode:
        default:
            SearchBoxText(RS_(L"CommandPalette_SearchBox/PlaceholderText"));
            NoMatchesText(RS_(L"CommandPalette_NoMatchesText/Text"));
            ControlName(RS_(L"CommandPaletteControlName"));
            break;
        }
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareCommandNames(const Command& lhs, const Command& rhs)
    {
        std::wstring_view leftName{ lhs.Name() };
        std::wstring_view rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    // This is a helper struct to aid in sorting Commands by a given weighting.
    struct WeightedCommand
    {
        Command command;
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
    std::vector<Command> CommandPalette::_collectFilteredActions()
    {
        std::vector<Command> actions;

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
            if (_currentMode == CommandPaletteMode::TabSearchMode || _currentMode == CommandPaletteMode::TabSwitchMode)
            {
                for (auto action : commandsToFilter)
                {
                    actions.push_back(action);
                }

                return actions;
            }

            // Add all the commands, but make sure they're sorted alphabetically.
            std::vector<Command> sortedCommands;
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
        if (_currentMode == CommandPaletteMode::CommandlineMode)
        {
            _filteredActions.Clear();
            return;
        }

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

        _nestedActionStack.Clear();

        ParentCommandName(L"");
        _currentNestedCommands.Clear();
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
                break;
            }
            case CollectionChange::ItemInserted:
            {
                auto tab = tabList.GetAt(idx);
                _allTabActions.InsertAt(idx, tab.SwitchToTabCommand());
                break;
            }
            case CollectionChange::ItemRemoved:
            {
                _allTabActions.RemoveAt(idx);
                break;
            }
            }

            _updateFilteredActions();
        }
    }

    void CommandPalette::EnableTabSwitcherMode(const bool searchMode, const uint32_t startIdx)
    {
        _switcherStartIdx = startIdx;

        if (searchMode)
        {
            _switchToMode(CommandPaletteMode::TabSearchMode);
        }
        else
        {
            _switchToMode(CommandPaletteMode::TabSwitchMode);
        }

        _updateFilteredActions();
    }
}
