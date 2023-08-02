// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionPaletteItem.h"
#include "CommandLinePaletteItem.h"
#include "SuggestionsControl.h"
#include <LibraryResources.h>

#include "SuggestionsControl.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    SuggestionsControl::SuggestionsControl()
    {
        InitializeComponent();

        _itemTemplateSelector = Resources().Lookup(winrt::box_value(L"PaletteItemTemplateSelector")).try_as<PaletteItemTemplateSelector>();
        _listItemTemplate = Resources().Lookup(winrt::box_value(L"ListItemTemplate")).try_as<DataTemplate>();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::FilteredCommand>();
        _nestedActionStack = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _currentNestedCommands = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _allCommands = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();

        _switchToMode();

        // Whatever is hosting us will enable us by setting our visibility to
        // "Visible". When that happens, set focus to our search box.
        RegisterPropertyChangedCallback(UIElement::VisibilityProperty(), [this](auto&&, auto&&) {
            if (Visibility() == Visibility::Visible)
            {
                // Force immediate binding update so we can select an item
                Bindings->Update();
                UpdateLayout(); // THIS ONE IN PARTICULAR SEEMS LOAD BEARING.
                // Without the UpdateLayout call, our ListView won't have a
                // chance to instantiate ListViewItem's. If we don't have those,
                // then our call to `SelectedItem()` below is going to return
                // null. If it does that, then we won't be able to focus
                // ourselves when we're opened.

                // Select the correct element in the list, depending on which
                // direction we were opened in.
                //
                // Make sure to use _scrollToIndex, to move the scrollbar too!
                if (_direction == TerminalApp::SuggestionsDirection::TopDown)
                {
                    _scrollToIndex(0);
                }
                else // BottomUp
                {
                    _scrollToIndex(_filteredActionsView().Items().Size() - 1);
                }

                if (_mode == SuggestionsMode::Palette)
                {
                    // Toss focus into the search box in palette mode
                    _searchBox().Visibility(Visibility::Visible);
                    _searchBox().Focus(FocusState::Programmatic);
                }
                else if (_mode == SuggestionsMode::Menu)
                {
                    // Toss focus onto the selected item in menu mode.
                    // Don't just focus the _filteredActionsView, because that will always select the 0th element.

                    _searchBox().Visibility(Visibility::Collapsed);

                    if (const auto& dependencyObj = SelectedItem().try_as<winrt::Windows::UI::Xaml::DependencyObject>())
                    {
                        Input::FocusManager::TryFocusAsync(dependencyObj, FocusState::Programmatic);
                    }
                }

                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "SuggestionsControlOpened",
                    TraceLoggingDescription("Event emitted when the Command Palette is opened"),
                    TraceLoggingWideString(L"Action", "Mode", "which mode the palette was opened in"),
                    TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                    TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
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
            // This does only fire once, when the size changes, which is the
            // very first time it's opened. It does not fire for subsequent
            // openings.

            _sizeChangedRevoker.revoke();
        });

        _filteredActionsView().SelectionChanged({ this, &SuggestionsControl::_selectedCommandChanged });
    }

    TerminalApp::SuggestionsMode SuggestionsControl::Mode() const
    {
        return _mode;
    }
    void SuggestionsControl::Mode(TerminalApp::SuggestionsMode mode)
    {
        _mode = mode;

        if (_mode == SuggestionsMode::Palette)
        {
            _searchBox().Visibility(Visibility::Visible);
            _searchBox().Focus(FocusState::Programmatic);
        }
        else if (_mode == SuggestionsMode::Menu)
        {
            _searchBox().Visibility(Visibility::Collapsed);
            _filteredActionsView().Focus(FocusState::Programmatic);
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
    void SuggestionsControl::SelectNextItem(const bool moveDown)
    {
        auto selected = _filteredActionsView().SelectedIndex();
        const auto numItems = ::base::saturated_cast<int>(_filteredActionsView().Items().Size());

        // Do not try to select an item if
        // - the list is empty
        // - if no item is selected and "up" is pressed
        if (numItems != 0 && (selected != -1 || moveDown))
        {
            // Wraparound math. By adding numItems and then calculating modulo numItems,
            // we clamp the values to the range [0, numItems) while still supporting moving
            // upward from 0 to numItems - 1.
            const auto newIndex = ((numItems + selected + (moveDown ? 1 : -1)) % numItems);
            _filteredActionsView().SelectedIndex(newIndex);
            _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());
        }
    }

    // Method Description:
    // - Scroll the command palette to the specified index
    // Arguments:
    // - index within a list view of commands
    // Return Value:
    // - <none>
    void SuggestionsControl::_scrollToIndex(uint32_t index)
    {
        auto numItems = _filteredActionsView().Items().Size();

        if (numItems == 0)
        {
            // if the list is empty no need to scroll
            return;
        }

        auto clampedIndex = std::clamp<int32_t>(index, 0, numItems - 1);
        _filteredActionsView().SelectedIndex(clampedIndex);
        _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());
    }

    // Method Description:
    // - Computes the number of visible commands
    // Arguments:
    // - <none>
    // Return Value:
    // - the approximate number of items visible in the list (in other words the size of the page)
    uint32_t SuggestionsControl::_getNumVisibleItems()
    {
        if (const auto container = _filteredActionsView().ContainerFromIndex(0))
        {
            if (const auto item = container.try_as<winrt::Windows::UI::Xaml::Controls::ListViewItem>())
            {
                const auto itemHeight = ::base::saturated_cast<int>(item.ActualHeight());
                const auto listHeight = ::base::saturated_cast<int>(_filteredActionsView().ActualHeight());
                return listHeight / itemHeight;
            }
        }
        return 0;
    }

    // Method Description:
    // - Scrolls the focus one page up the list of commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::ScrollPageUp()
    {
        auto selected = _filteredActionsView().SelectedIndex();
        auto numVisibleItems = _getNumVisibleItems();
        _scrollToIndex(selected - numVisibleItems);
    }

    // Method Description:
    // - Scrolls the focus one page down the list of commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::ScrollPageDown()
    {
        auto selected = _filteredActionsView().SelectedIndex();
        auto numVisibleItems = _getNumVisibleItems();
        _scrollToIndex(selected + numVisibleItems);
    }

    // Method Description:
    // - Moves the focus to the top item in the list of commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::ScrollToTop()
    {
        _scrollToIndex(0);
    }

    // Method Description:
    // - Moves the focus to the bottom item in the list of commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::ScrollToBottom()
    {
        _scrollToIndex(_filteredActionsView().Items().Size() - 1);
    }

    Windows::UI::Xaml::FrameworkElement SuggestionsControl::SelectedItem()
    {
        auto index = _filteredActionsView().SelectedIndex();
        const auto container = _filteredActionsView().ContainerFromIndex(index);
        const auto item = container.try_as<winrt::Windows::UI::Xaml::Controls::ListViewItem>();
        return item;
    }

    // Method Description:
    // - Called when the command selection changes. We'll use this to preview the selected action.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void SuggestionsControl::_selectedCommandChanged(const IInspectable& /*sender*/,
                                                     const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto selectedCommand = _filteredActionsView().SelectedItem();
        const auto filteredCommand{ selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>() };

        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"SelectedItem" });

        // Make sure to not send the preview if we're collapsed. This can
        // sometimes fire after we've been closed, which can trigger us to
        // preview the action for the empty text (as we've cleared the search
        // text as a part of closing).
        const bool isVisible{ this->Visibility() == Visibility::Visible };

        if (filteredCommand != nullptr &&
            isVisible)
        {
            if (const auto actionPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
            {
                PreviewAction.raise(*this, actionPaletteItem.Command());
            }
        }
    }

    void SuggestionsControl::_previewKeyDownHandler(const IInspectable& /*sender*/,
                                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        const auto coreWindow = CoreWindow::GetForCurrentThread();
        const auto ctrlDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);

        if (key == VirtualKey::Home && ctrlDown)
        {
            ScrollToTop();
            e.Handled(true);
        }
        else if (key == VirtualKey::End && ctrlDown)
        {
            ScrollToBottom();
            e.Handled(true);
        }
        else if (key == VirtualKey::Up)
        {
            // Move focus to the next item in the list.
            SelectNextItem(false);
            e.Handled(true);
        }
        else if (key == VirtualKey::Down)
        {
            // Move focus to the previous item in the list.
            SelectNextItem(true);
            e.Handled(true);
        }
        else if (key == VirtualKey::PageUp)
        {
            // Move focus to the first visible item in the list.
            ScrollPageUp();
            e.Handled(true);
        }
        else if (key == VirtualKey::PageDown)
        {
            // Move focus to the last visible item in the list.
            ScrollPageDown();
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter ||
                 key == VirtualKey::Tab ||
                 key == VirtualKey::Right)
        {
            // If the user pressed enter, tab, or the right arrow key, then
            // we'll want to dispatch the command that's selected as they
            // accepted the suggestion.

            if (const auto& button = e.OriginalSource().try_as<Button>())
            {
                // Let the button handle the Enter key so an eventually attached click handler will be called
                e.Handled(false);
                return;
            }

            const auto selectedCommand = _filteredActionsView().SelectedItem();
            const auto filteredCommand = selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>();
            _dispatchCommand(filteredCommand);
            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
            // Dismiss the palette if the text is empty, otherwise clear the
            // search string.
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

        else if (key == VirtualKey::C && ctrlDown)
        {
            _searchBox().CopySelectionToClipboard();
            e.Handled(true);
        }
        else if (key == VirtualKey::V && ctrlDown)
        {
            _searchBox().PasteFromClipboard();
            e.Handled(true);
        }

        // If the user types a character while the menu (not in palette mode)
        // is open, then dismiss ourselves. That way, when you type a character,
        // we'll instead send it to the TermControl.
        if (_mode == SuggestionsMode::Menu && !e.Handled())
        {
            _dismissPalette();
        }
    }

    // Method Description:
    // - Implements the Alt handler
    // Return value:
    // - whether the key was handled
    bool SuggestionsControl::OnDirectKeyEvent(const uint32_t /*vkey*/, const uint8_t /*scanCode*/, const bool /*down*/)
    {
        auto handled = false;
        return handled;
    }

    void SuggestionsControl::_keyUpHandler(const IInspectable& /*sender*/,
                                           const Windows::UI::Xaml::Input::KeyRoutedEventArgs& /*e*/)
    {
    }

    // Method Description:
    // - This event is triggered when someone clicks anywhere in the bounds of
    //   the window that's _not_ the command palette UI. When that happens,
    //   we'll want to dismiss the palette.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void SuggestionsControl::_rootPointerPressed(const Windows::Foundation::IInspectable& /*sender*/,
                                                 const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        if (Visibility() != Visibility::Collapsed)
        {
            _dismissPalette();
        }
    }

    // Method Description:
    // - The purpose of this event handler is to hide the palette if it loses focus.
    // We say we lost focus if our root element and all its descendants lost focus.
    // This handler is invoked when our root element or some descendant loses focus.
    // At this point we need to learn if the newly focused element belongs to this palette.
    // To achieve this:
    // - We start with the newly focused element and traverse its visual ancestors up to the Xaml root.
    // - If one of the ancestors is this SuggestionsControl, then by our definition the focus is not lost
    // - If we reach the Xaml root without meeting this SuggestionsControl,
    // then the focus is not contained in it anymore and it should be dismissed
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void SuggestionsControl::_lostFocusHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                               const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto flyout = _searchBox().ContextFlyout();
        if (flyout && flyout.IsOpen())
        {
            return;
        }

        auto root = this->XamlRoot();
        if (!root)
        {
            return;
        }

        auto focusedElementOrAncestor = Input::FocusManager::GetFocusedElement(root).try_as<DependencyObject>();
        while (focusedElementOrAncestor)
        {
            if (focusedElementOrAncestor == *this)
            {
                // This palette is the focused element or an ancestor of the focused element. No need to dismiss.
                return;
            }

            // Go up to the next ancestor
            focusedElementOrAncestor = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(focusedElementOrAncestor);
        }

        // We got to the root (the element with no parent) and didn't meet this palette on the path.
        // It means that it lost the focus and needs to be dismissed.
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
    void SuggestionsControl::_backdropPointerPressed(const Windows::Foundation::IInspectable& /*sender*/,
                                                     const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e)
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
    void SuggestionsControl::_listItemClicked(const Windows::Foundation::IInspectable& /*sender*/,
                                              const Windows::UI::Xaml::Controls::ItemClickEventArgs& e)
    {
        const auto selectedCommand = e.ClickedItem();
        if (const auto filteredCommand = selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>())
        {
            _dispatchCommand(filteredCommand);
        }
    }

    void SuggestionsControl::_listItemSelectionChanged(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e)
    {
        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
        {
            if (const auto selectedList = e.AddedItems(); selectedList.Size() > 0)
            {
                const auto selectedCommand = selectedList.GetAt(0);
                if (const auto filteredCmd = selectedCommand.try_as<TerminalApp::FilteredCommand>())
                {
                    if (const auto paletteItem = filteredCmd.Item().try_as<TerminalApp::PaletteItem>())
                    {
                        automationPeer.RaiseNotificationEvent(
                            Automation::Peers::AutomationNotificationKind::ItemAdded,
                            Automation::Peers::AutomationNotificationProcessing::MostRecent,
                            paletteItem.Name() + L" " + paletteItem.KeyChordText(),
                            L"SuggestionsControlSelectedItemChanged" /* unique name for this notification category */);
                    }
                }
            }
        }
    }

    // Method Description:
    // This event is called when the user clicks on a ChevronLeft button right
    // next to the ParentCommandName (e.g. New Tab...) above the subcommands list.
    // It'll go up a single level when the user clicks the button.
    // Arguments:
    // - sender: the button that got clicked
    // Return Value:
    // - <none>
    void SuggestionsControl::_moveBackButtonClicked(const Windows::Foundation::IInspectable& /*sender*/,
                                                    const Windows::UI::Xaml::RoutedEventArgs&)
    {
        PreviewAction.raise(*this, nullptr);
        _searchBox().Focus(FocusState::Programmatic);

        const auto previousAction{ _nestedActionStack.GetAt(_nestedActionStack.Size() - 1) };
        _nestedActionStack.RemoveAtEnd();

        // Repopulate nested commands when the root has not been reached yet
        if (_nestedActionStack.Size() > 0)
        {
            const auto newPreviousAction{ _nestedActionStack.GetAt(_nestedActionStack.Size() - 1) };
            const auto actionPaletteItem{ newPreviousAction.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() };

            ParentCommandName(actionPaletteItem.Command().Name());
            _updateCurrentNestedCommands(actionPaletteItem.Command());
        }
        else
        {
            ParentCommandName(L"");
            _currentNestedCommands.Clear();
        }
        _updateFilteredActions();

        const auto lastSelectedIt = std::find_if(begin(_filteredActions), end(_filteredActions), [&](const auto& filteredCommand) {
            return filteredCommand.Item().Name() == previousAction.Item().Name();
        });
        const auto lastSelectedIndex = static_cast<int32_t>(std::distance(begin(_filteredActions), lastSelectedIt));
        _scrollToIndex(lastSelectedIt != end(_filteredActions) ? lastSelectedIndex : 0);
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
    void SuggestionsControl::_updateUIForStackChange()
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
        _searchBox().Focus(FocusState::Programmatic);

        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::CurrentThenMostRecent,
                fmt::format(std::wstring_view{ RS_(L"CommandPalette_NestedCommandAnnouncement") }, ParentCommandName()),
                L"SuggestionsControlNestingLevelChanged" /* unique name for this notification category */);
        }
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
    Collections::IVector<winrt::TerminalApp::FilteredCommand> SuggestionsControl::_commandsToFilter()
    {
        if (_nestedActionStack.Size() > 0)
        {
            return _currentNestedCommands;
        }

        return _allCommands;
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
    void SuggestionsControl::_dispatchCommand(const winrt::TerminalApp::FilteredCommand& filteredCommand)
    {
        if (filteredCommand)
        {
            if (const auto actionPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
            {
                if (actionPaletteItem.Command().HasNestedCommands())
                {
                    // If this Command had subcommands, then don't dispatch the
                    // action. Instead, display a new list of commands for the user
                    // to pick from.
                    _nestedActionStack.Append(filteredCommand);
                    ParentCommandName(actionPaletteItem.Command().Name());
                    _updateCurrentNestedCommands(actionPaletteItem.Command());

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

                    // A note: the command palette ignores
                    // "ToggleCommandPalette" actions. We may want to do the
                    // same with "Suggestions" actions in the future, should we
                    // ever allow non-sendInput actions.
                    DispatchCommandRequested.raise(*this, actionPaletteItem.Command());

                    TraceLoggingWrite(
                        g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                        "SuggestionsControlDispatchedAction",
                        TraceLoggingDescription("Event emitted when the user selects an action in the Command Palette"),
                        TraceLoggingUInt32(searchTextLength, "SearchTextLength", "Number of characters in the search string"),
                        TraceLoggingUInt32(nestedCommandDepth, "NestedCommandDepth", "the depth in the tree of commands for the dispatched action"),
                        TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                        TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
                }
            }
        }
    }
    // Method Description:
    // - Get all the input text in _searchBox that follows any leading spaces.
    // Arguments:
    // - <none>
    // Return Value:
    // - the string of input following any number of leading spaces
    std::wstring SuggestionsControl::_getTrimmedInput()
    {
        const std::wstring input{ _searchBox().Text() };
        if (input.empty())
        {
            return input;
        }

        // Trim leading whitespace
        const auto firstNonSpace = input.find_first_not_of(L" ");
        if (firstNonSpace == std::wstring::npos)
        {
            // All the following characters are whitespace.
            return L"";
        }

        return input.substr(firstNonSpace);
    }

    // Method Description:
    // - Helper method for closing the command palette, when the user has _not_
    //   selected an action. Also fires a tracelogging event indicating that the
    //   user closed the palette without running a command.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::_dismissPalette()
    {
        _close();

        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "SuggestionsControlDismissed",
            TraceLoggingDescription("Event emitted when the user dismisses the Command Palette without selecting an action"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    // Method Description:
    // - Event handler for when the text in the input box changes. In Action
    //   Mode, we'll update the list of displayed commands, and select the first one.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void SuggestionsControl::_filterTextChanged(const IInspectable& /*sender*/,
                                                const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        // We're setting _lastFilterTextWasEmpty here, because if the user tries
        // to backspace the last character in the input, the Backspace KeyDown
        // event will fire _before_ _filterTextChanged does. Updating the value
        // here will ensure that we can check this case appropriately.
        _lastFilterTextWasEmpty = _searchBox().Text().empty();

        _updateFilteredActions();

        // In the command line mode we want the user to explicitly select the command
        _filteredActionsView().SelectedIndex(0);

        const auto currentNeedleHasResults{ _filteredActions.Size() > 0 };
        _noMatchesText().Visibility(currentNeedleHasResults ? Visibility::Collapsed : Visibility::Visible);
        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                currentNeedleHasResults ?
                    winrt::hstring{ fmt::format(std::wstring_view{ RS_(L"CommandPalette_MatchesAvailable") }, _filteredActions.Size()) } :
                    NoMatchesText(), // what to announce if results were found
                L"SuggestionsControlResultAnnouncement" /* unique name for this group of notifications */);
        }
    }

    Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> SuggestionsControl::FilteredActions()
    {
        return _filteredActions;
    }

    void SuggestionsControl::SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
    {
        _actionMap = actionMap;
    }

    void SuggestionsControl::SetCommands(const Collections::IVector<Command>& actions)
    {
        _allCommands.Clear();
        for (const auto& action : actions)
        {
            auto actionPaletteItem{ winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(action) };
            auto filteredCommand{ winrt::make<FilteredCommand>(actionPaletteItem) };
            _allCommands.Append(filteredCommand);
        }

        if (Visibility() == Visibility::Visible)
        {
            _updateFilteredActions();
        }
        else // THIS BRANCH IS NEW
        {
            auto actions = _collectFilteredActions();
            _filteredActions.Clear();
            for (const auto& action : actions)
            {
                _filteredActions.Append(action);
            }
        }
    }

    void SuggestionsControl::_switchToMode()
    {
        const auto currentlyVisible{ Visibility() == Visibility::Visible };

        auto modeAnnouncementResourceKey{ USES_RESOURCE(L"CommandPaletteModeAnnouncement_ActionMode") };
        ParsedCommandLineText(L"");
        _searchBox().Text(L"");
        _searchBox().Select(_searchBox().Text().size(), 0);

        _nestedActionStack.Clear();
        ParentCommandName(L"");
        _currentNestedCommands.Clear();
        // Leaving this block of code outside the above if-statement
        // guarantees that the correct text is shown for the mode
        // whenever _switchToMode is called.

        SearchBoxPlaceholderText(RS_(L"CommandPalette_SearchBox/PlaceholderText"));
        NoMatchesText(RS_(L"CommandPalette_NoMatchesText/Text"));
        ControlName(RS_(L"CommandPaletteControlName"));
        // modeAnnouncementResourceKey is already set to _ActionMode
        // We did this above to deduce the type (and make it easier on ourselves later).

        if (currentlyVisible)
        {
            if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
            {
                automationPeer.RaiseNotificationEvent(
                    Automation::Peers::AutomationNotificationKind::ActionCompleted,
                    Automation::Peers::AutomationNotificationProcessing::CurrentThenMostRecent,
                    GetLibraryResourceString(modeAnnouncementResourceKey),
                    L"SuggestionsControlModeSwitch" /* unique ID for this notification */);
            }
        }

        // The smooth remove/add animations that happen during
        // UpdateFilteredActions don't work very well when switching between
        // modes because of the sheer amount of remove/adds. So, let's just
        // clear + append when switching between modes.
        _filteredActions.Clear();
        _updateFilteredActions();
    }

    // Method Description:
    // - Produce a list of filtered actions to reflect the current contents of
    //   the input box.
    // Arguments:
    // - A collection that will receive the filtered actions
    // Return Value:
    // - <none>
    std::vector<winrt::TerminalApp::FilteredCommand> SuggestionsControl::_collectFilteredActions()
    {
        std::vector<winrt::TerminalApp::FilteredCommand> actions;

        winrt::hstring searchText{ _getTrimmedInput() };

        auto commandsToFilter = _commandsToFilter();

        {
            for (const auto& action : commandsToFilter)
            {
                // Update filter for all commands
                // This will modify the highlighting but will also lead to re-computation of weight (and consequently sorting).
                // Pay attention that it already updates the highlighting in the UI
                action.UpdateFilter(searchText);

                // if there is active search we skip commands with 0 weight
                if (searchText.empty() || action.Weight() > 0)
                {
                    actions.push_back(action);
                }
            }
        }
        if (_mode == SuggestionsMode::Palette)
        {
            // We want to present the commands sorted
            std::sort(actions.begin(), actions.end(), FilteredCommand::Compare);
        }

        // Adjust the order of the results depending on if we're top-down or
        // bottom up. This way, the "first" / "best" match is always closest to
        // the cursor.
        if (_direction == TerminalApp::SuggestionsDirection::BottomUp)
        {
            // Reverse the list
            std::reverse(std::begin(actions), std::end(actions));
        }

        return actions;
    }

    // Method Description:
    // - Update our list of filtered actions to reflect the current contents of
    //   the input box.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SuggestionsControl::_updateFilteredActions()
    {
        auto actions = _collectFilteredActions();

        // Make _filteredActions look identical to actions, using only Insert and Remove.
        // This allows WinUI to nicely animate the ListView as it changes.
        for (uint32_t i = 0; i < _filteredActions.Size() && i < actions.size(); i++)
        {
            for (auto j = i; j < _filteredActions.Size(); j++)
            {
                if (_filteredActions.GetAt(j).Item() == actions[i].Item())
                {
                    for (auto k = i; k < j; k++)
                    {
                        _filteredActions.RemoveAt(i);
                    }
                    break;
                }
            }

            if (_filteredActions.GetAt(i).Item() != actions[i].Item())
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

    // Method Description:
    // - Update the list of current nested commands to match that of the
    //   given parent command.
    // Arguments:
    // - parentCommand: the command with an optional list of nested commands.
    // Return Value:
    // - <none>
    void SuggestionsControl::_updateCurrentNestedCommands(const winrt::Microsoft::Terminal::Settings::Model::Command& parentCommand)
    {
        _currentNestedCommands.Clear();
        for (const auto& nameAndCommand : parentCommand.NestedCommands())
        {
            const auto action = nameAndCommand.Value();
            auto nestedActionPaletteItem{ winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(action) };
            auto nestedFilteredCommand{ winrt::make<FilteredCommand>(nestedActionPaletteItem) };
            _currentNestedCommands.Append(nestedFilteredCommand);
        }
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
    void SuggestionsControl::_close()
    {
        Visibility(Visibility::Collapsed);

        // Clear the text box each time we close the dialog. This is consistent with VsCode.
        _searchBox().Text(L"");

        _nestedActionStack.Clear();

        ParentCommandName(L"");
        _currentNestedCommands.Clear();

        PreviewAction.raise(*this, nullptr);
    }

    // Method Description:
    // - This event is triggered when filteredActionView is looking for item container (ListViewItem)
    // to use to present the filtered actions.
    // GH#9288: unfortunately the default lookup seems to choose items with wrong data templates,
    // e.g., using DataTemplate rendering actions for tab palette items.
    // We handle this event by manually selecting an item from the cache.
    // If no item is found we allocate a new one.
    // Arguments:
    // - args: the ChoosingItemContainerEventArgs allowing to get container candidate suggested by the
    // system and replace it with another candidate if required.
    // Return Value:
    // - <none>
    void SuggestionsControl::_choosingItemContainer(
        const Windows::UI::Xaml::Controls::ListViewBase& /*sender*/,
        const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args)
    {
        const auto dataTemplate = _itemTemplateSelector.SelectTemplate(args.Item());
        const auto itemContainer = args.ItemContainer();
        if (itemContainer && itemContainer.ContentTemplate() == dataTemplate)
        {
            // If the suggested candidate is OK simply remove it from the cache
            // (so we won't allocate it until it is released) and return
            _listViewItemsCache[dataTemplate].erase(itemContainer);
        }
        else
        {
            // We need another candidate, let's look it up inside the cache
            auto& containersByTemplate = _listViewItemsCache[dataTemplate];
            if (!containersByTemplate.empty())
            {
                // There cache contains available items for required DataTemplate
                // Let's return one of them (and remove it from the cache)
                auto firstItem = containersByTemplate.begin();
                args.ItemContainer(*firstItem);
                containersByTemplate.erase(firstItem);
            }
            else
            {
                ElementFactoryGetArgs factoryArgs{};
                const auto listViewItem = _listItemTemplate.GetElement(factoryArgs).try_as<Controls::ListViewItem>();
                listViewItem.ContentTemplate(dataTemplate);

                if (dataTemplate == _itemTemplateSelector.NestedItemTemplate())
                {
                    const auto helpText = winrt::box_value(RS_(L"CommandPalette_MoreOptions/[using:Windows.UI.Xaml.Automation]AutomationProperties/HelpText"));
                    listViewItem.SetValue(Automation::AutomationProperties::HelpTextProperty(), helpText);
                }

                args.ItemContainer(listViewItem);
            }
        }
        args.IsContainerPrepared(true);
    }
    // Method Description:
    // - This event is triggered when the data item associate with filteredActionView list item is changing.
    //   We check if the item is being recycled. In this case we return it to the cache
    // Arguments:
    // - args: the ContainerContentChangingEventArgs describing the container change
    // Return Value:
    // - <none>
    void SuggestionsControl::_containerContentChanging(
        const Windows::UI::Xaml::Controls::ListViewBase& /*sender*/,
        const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args)
    {
        const auto itemContainer = args.ItemContainer();
        if (args.InRecycleQueue() && itemContainer && itemContainer.ContentTemplate())
        {
            _listViewItemsCache[itemContainer.ContentTemplate()].insert(itemContainer);
            itemContainer.DataContext(nullptr);
        }
        else
        {
            itemContainer.DataContext(args.Item());
        }
    }

    void SuggestionsControl::_setDirection(TerminalApp::SuggestionsDirection direction)
    {
        _direction = direction;
        if (_direction == TerminalApp::SuggestionsDirection::TopDown)
        {
            Controls::Grid::SetRow(_searchBox(), 0);
        }
        else // BottomUp
        {
            Controls::Grid::SetRow(_searchBox(), 4);
        }
    }

    void SuggestionsControl::Open(TerminalApp::SuggestionsMode mode,
                                  const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command>& commands,
                                  Windows::Foundation::Point anchor,
                                  Windows::Foundation::Size space,
                                  float characterHeight)
    {
        Mode(mode);
        SetCommands(commands);

        // LOAD BEARING
        // The control must become visible here, BEFORE we try to get its ActualWidth/Height.
        Visibility(commands.Size() > 0 ? Visibility::Visible : Visibility::Collapsed);

        _anchor = anchor;
        _space = space;

        const til::size actualSize{ til::math::rounding, ActualWidth(), ActualHeight() };
        // Is there space in the window below the cursor to open the menu downwards?
        const bool canOpenDownwards = (_anchor.Y + characterHeight + actualSize.height) < space.Height;
        _setDirection(canOpenDownwards ? TerminalApp::SuggestionsDirection::TopDown :
                                         TerminalApp::SuggestionsDirection::BottomUp);
        // Set the anchor below by a character height
        _anchor.Y += canOpenDownwards ? characterHeight : 0;

        // First, position horizontally.
        //
        // We want to align the left edge of the text within the control to the
        // cursor position. We'll need to scoot a little to the left, to align
        // text with cursor
        const auto proposedX = gsl::narrow_cast<int>(_anchor.X - 40);
        // If the control is too wide to fit in the window, clamp it fit inside
        // the window.
        const auto maxX = gsl::narrow_cast<int>(space.Width - actualSize.width);
        const auto clampedX = std::clamp(proposedX, 0, maxX);

        // Create a thickness for the new margins
        auto newMargin = Windows::UI::Xaml::ThicknessHelper::FromLengths(clampedX, 0, 0, 0);
        // Now, position vertically.
        if (_direction == TerminalApp::SuggestionsDirection::TopDown)
        {
            // The control should open right below the cursor, with the list
            // extending below. This is easy, we can just use the cursor as the
            // origin (more or less)
            newMargin.Top = (_anchor.Y);
        }
        else
        {
            // Position at the cursor. The suggestions UI itself will maintain
            // its own offset such that it's always above its origin
            newMargin.Top = (_anchor.Y - actualSize.height);
        }
        Margin(newMargin);
    }
}
