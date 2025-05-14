// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionPaletteItem.h"
#include "TabPaletteItem.h"
#include "CommandLinePaletteItem.h"
#include "CommandPalette.h"
#include <LibraryResources.h>

#include "CommandPalette.g.cpp"

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
    CommandPalette::CommandPalette() :
        _switcherStartIdx{ 0 }
    {
        InitializeComponent();

        _itemTemplateSelector = Resources().Lookup(winrt::box_value(L"PaletteItemTemplateSelector")).try_as<PaletteItemTemplateSelector>();
        _listItemTemplate = Resources().Lookup(winrt::box_value(L"ListItemTemplate")).try_as<DataTemplate>();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::FilteredCommand>();
        _nestedActionStack = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _currentNestedCommands = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _allCommands = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _tabActions = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();
        _mruTabActions = winrt::single_threaded_vector<winrt::TerminalApp::FilteredCommand>();

        _switchToMode(CommandPaletteMode::ActionMode);

        // Whatever is hosting us will enable us by setting our visibility to
        // "Visible". When that happens, set focus to our search box.
        RegisterPropertyChangedCallback(UIElement::VisibilityProperty(), [this](auto&&, auto&&) {
            if (Visibility() == Visibility::Visible)
            {
                // Force immediate binding update so we can select an item
                Bindings->Update();

                if (_currentMode == CommandPaletteMode::TabSwitchMode)
                {
                    _searchBox().Visibility(Visibility::Collapsed);
                    _filteredActionsView().SelectedIndex(_switcherStartIdx);
                    _filteredActionsView().ScrollIntoView(_filteredActionsView().SelectedItem());
                    _filteredActionsView().Focus(FocusState::Keyboard);

                    // Do this right after becoming visible so we can quickly catch scenarios where
                    // modifiers aren't held down (e.g. command palette invocation).
                    _anchorKeyUpHandler();
                }
                else
                {
                    _filteredActionsView().SelectedIndex(0);
                    _searchBox().Focus(FocusState::Programmatic);
                }

                TraceLoggingWrite(
                    g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                    "CommandPaletteOpened",
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
            if (_currentMode == CommandPaletteMode::TabSwitchMode)
            {
                _filteredActionsView().Focus(FocusState::Keyboard);
            }
            _sizeChangedRevoker.revoke();
        });

        _filteredActionsView().SelectionChanged({ this, &CommandPalette::_selectedCommandChanged });

        _appArgs.DisableHelpInExitMessage();
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
    void CommandPalette::_scrollToIndex(uint32_t index)
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
    uint32_t CommandPalette::_getNumVisibleItems()
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
    void CommandPalette::ScrollPageUp()
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
    void CommandPalette::ScrollPageDown()
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
    void CommandPalette::ScrollToTop()
    {
        _scrollToIndex(0);
    }

    // Method Description:
    // - Moves the focus to the bottom item in the list of commands.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::ScrollToBottom()
    {
        _scrollToIndex(_filteredActionsView().Items().Size() - 1);
    }

    // Method Description:
    // - Called when the command selection changes. We'll use this in the tab
    //   switcher to "preview" tabs as the user navigates the list of tabs. To
    //   do that, we'll dispatch the switch to tab command for this tab, but not
    //   dismiss the switcher.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void CommandPalette::_selectedCommandChanged(const IInspectable& /*sender*/,
                                                 const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        const auto selectedCommand = _filteredActionsView().SelectedItem();
        const auto filteredCommand{ selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>() };
        if (_currentMode == CommandPaletteMode::TabSwitchMode)
        {
            _switchToTab(filteredCommand);
        }
        else if (_currentMode == CommandPaletteMode::ActionMode && filteredCommand != nullptr)
        {
            if (const auto actionPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
            {
                PreviewAction.raise(*this, actionPaletteItem.Command());
            }
        }
        else if (_currentMode == CommandPaletteMode::CommandlineMode)
        {
            if (filteredCommand)
            {
                SearchBoxPlaceholderText(filteredCommand.Item().Name());
            }
            else
            {
                SearchBoxPlaceholderText(RS_(L"CmdPalCommandlinePrompt"));
            }
        }
    }

    void CommandPalette::_previewKeyDownHandler(const IInspectable& /*sender*/,
                                                const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        const auto scanCode = e.KeyStatus().ScanCode;
        const auto coreWindow = CoreWindow::GetForCurrentThread();
        const auto ctrlDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);
        const auto altDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Menu), CoreVirtualKeyStates::Down);
        const auto shiftDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Shift), CoreVirtualKeyStates::Down);

        // Some keypresses such as Tab, Return, Esc, and Arrow Keys are ignored by controls because
        // they're not considered input key presses. While they don't raise KeyDown events,
        // they do raise PreviewKeyDown events.
        //
        // Only give anchored tab switcher the ability to cycle through tabs with the tab button.
        // For unanchored mode, accessibility becomes an issue when we try to hijack tab since it's
        // a really widely used keyboard navigation key.
        if (_currentMode == CommandPaletteMode::TabSwitchMode && _actionMap)
        {
            winrt::Microsoft::Terminal::Control::KeyChord kc{ ctrlDown, altDown, shiftDown, false, static_cast<int32_t>(key), static_cast<int32_t>(scanCode) };
            if (const auto cmd{ _actionMap.GetActionByKeyChord(kc) })
            {
                if (cmd.ActionAndArgs().Action() == ShortcutAction::PrevTab)
                {
                    SelectNextItem(false);
                    e.Handled(true);
                    return;
                }
                else if (cmd.ActionAndArgs().Action() == ShortcutAction::NextTab)
                {
                    SelectNextItem(true);
                    e.Handled(true);
                    return;
                }
            }
        }

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
        else if (key == VirtualKey::PageUp)
        {
            // Action Mode: Move focus to the first visible item in the list.
            ScrollPageUp();
            e.Handled(true);
        }
        else if (key == VirtualKey::PageDown)
        {
            // Action Mode: Move focus to the last visible item in the list.
            ScrollPageDown();
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter)
        {
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
        else if (key == VirtualKey::Back && _searchBox().Text().empty() && _lastFilterTextWasEmpty && _currentMode == CommandPaletteMode::ActionMode)
        {
            // If the last filter text was empty, and we're backspacing from
            // that state, then the user "backspaced" the virtual '>' we're
            // using as the action mode indicator. Switch into commandline mode.
            _switchToMode(CommandPaletteMode::CommandlineMode);
            e.Handled(true);
        }
        else if ((key == VirtualKey::C || key == VirtualKey::Insert) && ctrlDown)
        {
            _searchBox().CopySelectionToClipboard();
            e.Handled(true);
        }
        else if (key == VirtualKey::V && ctrlDown)
        {
            _searchBox().PasteFromClipboard();
            e.Handled(true);
        }
        else if (key == VirtualKey::Right && _currentMode == CommandPaletteMode::CommandlineMode)
        {
            if (const auto command{ _filteredActionsView().SelectedItem().try_as<winrt::TerminalApp::FilteredCommand>() })
            {
                _searchBox().Text(command.Item().Name());
                _searchBox().Select(_searchBox().Text().size(), 0);
                _searchBox().Focus(FocusState::Programmatic);
                _filteredActionsView().SelectedIndex(-1);
                e.Handled(true);
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

    void CommandPalette::_keyUpHandler(const IInspectable& /*sender*/,
                                       const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
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
        const auto coreWindow = CoreWindow::GetForCurrentThread();
        const auto ctrlDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);
        const auto altDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Menu), CoreVirtualKeyStates::Down);
        const auto shiftDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Shift), CoreVirtualKeyStates::Down);

        if (!ctrlDown && !altDown && !shiftDown)
        {
            const auto selectedCommand = _filteredActionsView().SelectedItem();
            if (const auto filteredCommand = selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>())
            {
                _dispatchCommand(filteredCommand);
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
    void CommandPalette::_rootPointerPressed(const Windows::Foundation::IInspectable& /*sender*/,
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
    // - If one of the ancestors is this CommandPalette, then by our definition the focus is not lost
    // - If we reach the Xaml root without meeting this CommandPalette,
    // then the focus is not contained in it anymore and it should be dismissed
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void CommandPalette::_lostFocusHandler(const Windows::Foundation::IInspectable& /*sender*/,
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
    void CommandPalette::_backdropPointerPressed(const Windows::Foundation::IInspectable& /*sender*/,
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
    void CommandPalette::_listItemClicked(const Windows::Foundation::IInspectable& /*sender*/,
                                          const Windows::UI::Xaml::Controls::ItemClickEventArgs& e)
    {
        const auto selectedCommand = e.ClickedItem();
        if (const auto filteredCommand = selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>())
        {
            _dispatchCommand(filteredCommand);
        }
    }

    // Method Description:
    // - This event is called when the user's mouse pointer enters an individual
    //   item from the list. We'll get the item that was hovered and "preview"
    //   the command that the user hovered. To do that, we'll dispatch the switch
    //   to tab command for this tab, but not dismiss the switcher.
    //
    // Arguments:
    // - sender: the UI element that raised the event.
    // Return Value:
    // - <none>
    void CommandPalette::_listItemPointerEntered(const winrt::Windows::Foundation::IInspectable& sender,
                                                 const winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*args*/)
    {
        // cancel any pending exit timer to prevent an unwanted preview revert
        if (_pointerExitTimer)
        {
            _pointerExitTimer.Stop();
        }

        const auto listViewItem = sender.try_as<winrt::Windows::UI::Xaml::Controls::ListViewItem>();
        if (_currentMode == CommandPaletteMode::ActionMode && listViewItem)
        {
            const auto enteredItem = listViewItem.Content();
            if (const auto filteredCommand{ enteredItem.try_as<winrt::TerminalApp::FilteredCommand>() })
            {
                if (const auto actionPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
                {
                    // immediately preview the hovered command
                    PreviewAction.raise(*this, actionPaletteItem.Command());
                }
            }
        }
    }

    // Method Description:
    // - This event is called when the user's mouse pointer exits an individual
    //   item from the list. We then revert to previewing the selected item rather
    //   than the hovered one, using a short delay (via a DispatcherTimer) to smooth
    //   transitions when rapidly moving between items.
    //
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void CommandPalette::_listItemPointerExited(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                const winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*args*/)
    {
        // if there is no exit timer, create one
        if (!_pointerExitTimer)
        {
            _pointerExitTimer = winrt::Windows::UI::Xaml::DispatcherTimer();
            _pointerExitTimer.Interval(std::chrono::milliseconds(10));
            _pointerExitTimer.Tick([this](auto const&, auto const&) {
                // when the timer ticks, revert the preview to the selected command
                const auto selectedCommand = _filteredActionsView().SelectedItem();
                if (const auto filteredCommand{ selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>() })
                {
                    if (_currentMode == CommandPaletteMode::ActionMode && filteredCommand)
                    {
                        if (const auto actionPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::ActionPaletteItem>() })
                        {
                            PreviewAction.raise(*this, actionPaletteItem.Command());
                        }
                    }
                }
                _pointerExitTimer.Stop();
            });
        }

        // restart the timer
        _pointerExitTimer.Start();
    }

    void CommandPalette::_listItemSelectionChanged(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e)
    {
        // We don't care about...
        // - CommandlineMode: it doesn't have any selectable items in the list view
        // - TabSwitchMode: focus and selected item are in sync
        if (_currentMode == CommandPaletteMode::ActionMode || _currentMode == CommandPaletteMode::TabSearchMode)
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
                                L"CommandPaletteSelectedItemChanged" /* unique name for this notification category */);
                        }
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
    void CommandPalette::_moveBackButtonClicked(const Windows::Foundation::IInspectable& /*sender*/,
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
        _searchBox().Focus(FocusState::Programmatic);

        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::CurrentThenMostRecent,
                RS_fmt(L"CommandPalette_NestedCommandAnnouncement", ParentCommandName()),
                L"CommandPaletteNestingLevelChanged" /* unique name for this notification category */);
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
    Collections::IVector<winrt::TerminalApp::FilteredCommand> CommandPalette::_commandsToFilter()
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
            return _tabActions;
        case CommandPaletteMode::TabSwitchMode:
            return _tabSwitcherMode == TabSwitcherMode::MostRecentlyUsed ? _mruTabActions : _tabActions;
        case CommandPaletteMode::CommandlineMode:
            return _loadRecentCommands();
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
    void CommandPalette::_dispatchCommand(const winrt::TerminalApp::FilteredCommand& filteredCommand)
    {
        if (_currentMode == CommandPaletteMode::CommandlineMode)
        {
            _dispatchCommandline(filteredCommand);
        }
        else if (_currentMode == CommandPaletteMode::TabSwitchMode || _currentMode == CommandPaletteMode::TabSearchMode)
        {
            _switchToTab(filteredCommand);
            _close();
        }
        else if (filteredCommand)
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

                    // But make an exception for the Toggle Command Palette action: we don't want the dispatch
                    // make the command palette - that was just closed - visible again.
                    // All other actions can just be dispatched.
                    if (actionPaletteItem.Command().ActionAndArgs().Action() != ShortcutAction::ToggleCommandPalette)
                    {
                        DispatchCommandRequested.raise(*this, actionPaletteItem.Command());
                    }

                    TraceLoggingWrite(
                        g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                        "CommandPaletteDispatchedAction",
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
    std::wstring CommandPalette::_getTrimmedInput()
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
    // - Dispatch switch to tab action.
    // Arguments:
    // - filteredCommand - Selected filtered command - might be null
    // Return Value:
    // - <none>
    void CommandPalette::_switchToTab(const winrt::TerminalApp::FilteredCommand& filteredCommand)
    {
        if (filteredCommand)
        {
            if (const auto tabPaletteItem{ filteredCommand.Item().try_as<winrt::TerminalApp::TabPaletteItem>() })
            {
                if (const auto tab{ tabPaletteItem.Tab() })
                {
                    SwitchToTabRequested.raise(*this, tab);
                }
            }
        }
    }

    // Method Description:
    // - Dispatch the current search text as a ExecuteCommandline action.
    // Arguments:
    // - filteredCommand - Selected filtered command - might be null
    // Return Value:
    // - <none>
    void CommandPalette::_dispatchCommandline(const winrt::TerminalApp::FilteredCommand& command)
    {
        const auto filteredCommand = command ? command : _buildCommandLineCommand(winrt::hstring(_getTrimmedInput()));
        if (filteredCommand.has_value())
        {
            _updateRecentCommands(filteredCommand.value().Item().Name());

            TraceLoggingWrite(
                g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
                "CommandPaletteDispatchedCommandline",
                TraceLoggingDescription("Event emitted when the user runs a commandline in the Command Palette"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

            if (const auto commandLinePaletteItem{ filteredCommand.value().Item().try_as<winrt::TerminalApp::CommandLinePaletteItem>() })
            {
                CommandLineExecutionRequested.raise(*this, commandLinePaletteItem.CommandLine());
                _close();
            }
        }
    }

    std::optional<TerminalApp::FilteredCommand> CommandPalette::_buildCommandLineCommand(const hstring& commandLine)
    {
        if (commandLine.empty())
        {
            return std::nullopt;
        }

        auto commandLinePaletteItem{ winrt::make<CommandLinePaletteItem>(commandLine) };
        return winrt::make<FilteredCommand>(commandLinePaletteItem);
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
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    // Method Description:
    // - Event handler for when the text in the input box changes. In Action
    //   Mode, we'll update the list of displayed commands, and select the first one.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void CommandPalette::_filterTextChanged(const IInspectable& /*sender*/,
                                            const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        // When we are executing the _SelectNextTab in the TabManagement.cpp, this method
        // is getting triggered because we set up the default value for that CommandPalette
        // with an empty string. Therefore, to avoid the reset of the index when executing
        // the Next/Prev tab command, we are skipping this execution.
        // Check issue https://github.com/microsoft/terminal/issues/11146
        if (_currentMode == CommandPaletteMode::TabSwitchMode)
        {
            return;
        }

        if (_currentMode == CommandPaletteMode::CommandlineMode)
        {
            _evaluatePrefix();
        }

        // We're setting _lastFilterTextWasEmpty here, because if the user tries
        // to backspace the last character in the input, the Backspace KeyDown
        // event will fire _before_ _filterTextChanged does. Updating the value
        // here will ensure that we can check this case appropriately.
        _lastFilterTextWasEmpty = _searchBox().Text().empty();

        _updateFilteredActions();

        // In the command line mode we want the user to explicitly select the command
        _filteredActionsView().SelectedIndex(_currentMode == CommandPaletteMode::CommandlineMode ? -1 : 0);

        if (_currentMode == CommandPaletteMode::TabSearchMode || _currentMode == CommandPaletteMode::ActionMode)
        {
            const auto currentNeedleHasResults{ _filteredActions.Size() > 0 };
            _noMatchesText().Visibility(currentNeedleHasResults ? Visibility::Collapsed : Visibility::Visible);
            if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
            {
                automationPeer.RaiseNotificationEvent(
                    Automation::Peers::AutomationNotificationKind::ActionCompleted,
                    Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                    currentNeedleHasResults ?
                        winrt::hstring{ RS_fmt(L"CommandPalette_MatchesAvailable", _filteredActions.Size()) } :
                        NoMatchesText(), // what to announce if results were found
                    L"CommandPaletteResultAnnouncement" /* unique name for this group of notifications */);
            }
        }
        else
        {
            _noMatchesText().Visibility(Visibility::Collapsed);
        }

        if (_currentMode == CommandPaletteMode::CommandlineMode)
        {
            ParsedCommandLineText(L"");

            const auto commandLine = _getTrimmedInput();
            if (!commandLine.empty())
            {
                ExecuteCommandlineArgs args{ commandLine };
                _appArgs.FullResetState();
                if (_appArgs.ParseArgs(args) == 0)
                {
                    const auto& commands = _appArgs.GetStartupActions();
                    if (commands.size() > 0)
                    {
                        std::wstring commandDescription{ RS_(L"CommandPalette_ParsedCommandLine") };
                        for (const auto& command : commands)
                        {
                            commandDescription += L"\n\t" + command.Args().GenerateName();
                        }
                        ParsedCommandLineText(commandDescription.data());
                    }
                }
                else
                {
                    ParsedCommandLineText(RS_(L"CommandPalette_FailedParsingCommandLine") + L"\n\t" + til::u8u16(_appArgs.GetExitMessage()));
                }
            }
        }
    }

    void CommandPalette::_evaluatePrefix()
    {
        // This will take you from commandline mode, into action mode. The
        // backspace handler in _keyDownHandler will handle taking us from
        // action mode to commandline mode.
        auto newMode = CommandPaletteMode::CommandlineMode;

        auto inputText = _getTrimmedInput();
        if (inputText.size() > 0)
        {
            if (inputText[0] == L'>')
            {
                newMode = CommandPaletteMode::ActionMode;
            }
        }

        if (newMode != _currentMode)
        {
            //_switchToMode will remove the '>' character from the input.
            _switchToMode(newMode);
        }
    }

    Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> CommandPalette::FilteredActions()
    {
        return _filteredActions;
    }

    void CommandPalette::SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap)
    {
        _actionMap = actionMap;
        _populateCommands();
    }

    void CommandPalette::_populateCommands()
    {
        _allCommands.Clear();
        if (_actionMap)
        {
            const auto expandedCommands{ _actionMap.ExpandedCommands() };
            for (const auto& action : expandedCommands)
            {
                const auto keyChordText{ KeyChordSerialization::ToString(_actionMap.GetKeyBindingForAction(action.ID())) };
                auto actionPaletteItem{ winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(action, keyChordText) };
                auto filteredCommand{ winrt::make<FilteredCommand>(actionPaletteItem) };
                _allCommands.Append(filteredCommand);
            }

            if (Visibility() == Visibility::Visible && _currentMode == CommandPaletteMode::ActionMode)
            {
                _updateFilteredActions();
            }
        }
    }

    // Method Description:
    // - Replaces a list of filtered commands in the target collection with new
    //   commands based on the tabs in the source collection.
    // Although the source observable we still don't register on it,
    // so the palette user will need to reset the binding manually every time
    // the source collection changes
    // Arguments:
    // - source: the tabs to use for creation filtered commands
    // - target: the collection to store newly created filtered commands
    // Return Value:
    // - <none>
    void CommandPalette::_bindTabs(
        const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase>& source,
        const Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand>& target)
    {
        target.Clear();
        for (const auto& tab : source)
        {
            auto tabPaletteItem{ winrt::make<winrt::TerminalApp::implementation::TabPaletteItem>(tab) };
            auto filteredCommand{ winrt::make<FilteredCommand>(tabPaletteItem) };
            target.Append(filteredCommand);
        }
    }

    void CommandPalette::SetTabs(const Collections::IObservableVector<TabBase>& tabs, const Collections::IObservableVector<TabBase>& mruTabs)
    {
        _bindTabs(tabs, _tabActions);
        _bindTabs(mruTabs, _mruTabActions);
    }

    void CommandPalette::EnableCommandPaletteMode(const CommandPaletteLaunchMode launchMode)
    {
        const auto mode = (launchMode == CommandPaletteLaunchMode::CommandLine) ?
                              CommandPaletteMode::CommandlineMode :
                              CommandPaletteMode::ActionMode;

        _switchToMode(mode);
    }

    void CommandPalette::_switchToMode(CommandPaletteMode mode)
    {
        _currentMode = mode;

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
        switch (_currentMode)
        {
        case CommandPaletteMode::TabSearchMode:
        case CommandPaletteMode::TabSwitchMode:
        {
            SearchBoxPlaceholderText(RS_(L"TabSwitcher_SearchBoxText"));
            NoMatchesText(RS_(L"TabSwitcher_NoMatchesText"));
            ControlName(RS_(L"TabSwitcherControlName"));
            PrefixCharacter(L"");
            modeAnnouncementResourceKey = USES_RESOURCE(L"CommandPaletteModeAnnouncement_TabSearchSwitchMode");
            break;
        }
        case CommandPaletteMode::CommandlineMode:
            SearchBoxPlaceholderText(RS_(L"CmdPalCommandlinePrompt"));
            NoMatchesText(L"");
            ControlName(RS_(L"CommandPaletteControlName"));
            PrefixCharacter(L"");
            modeAnnouncementResourceKey = USES_RESOURCE(L"CommandPaletteModeAnnouncement_CommandlineMode");
            break;
        case CommandPaletteMode::ActionMode:
        default:
            SearchBoxPlaceholderText(RS_(L"CommandPalette_SearchBox/PlaceholderText"));
            NoMatchesText(RS_(L"CommandPalette_NoMatchesText/Text"));
            ControlName(RS_(L"CommandPaletteControlName"));
            PrefixCharacter(L">");
            // modeAnnouncementResourceKey is already set to _ActionMode
            // We did this above to deduce the type (and make it easier on ourselves later).
            break;
        }

        if (currentlyVisible)
        {
            if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
            {
                automationPeer.RaiseNotificationEvent(
                    Automation::Peers::AutomationNotificationKind::ActionCompleted,
                    Automation::Peers::AutomationNotificationProcessing::CurrentThenMostRecent,
                    GetLibraryResourceString(modeAnnouncementResourceKey),
                    L"CommandPaletteModeSwitch" /* unique ID for this notification */);
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
    std::vector<winrt::TerminalApp::FilteredCommand> CommandPalette::_collectFilteredActions()
    {
        std::vector<winrt::TerminalApp::FilteredCommand> actions;

        winrt::hstring searchText{ _getTrimmedInput() };

        auto commandsToFilter = _commandsToFilter();

        if (_currentMode == CommandPaletteMode::TabSwitchMode)
        {
            std::copy(begin(commandsToFilter), end(commandsToFilter), std::back_inserter(actions));
        }
        else if (_currentMode == CommandPaletteMode::TabSearchMode || _currentMode == CommandPaletteMode::ActionMode || _currentMode == CommandPaletteMode::CommandlineMode)
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

        // We want to present the commands sorted
        if (_currentMode == CommandPaletteMode::ActionMode)
        {
            std::sort(actions.begin(), actions.end(), FilteredCommand::Compare);
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
    void CommandPalette::_updateFilteredActions()
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
    void CommandPalette::_updateCurrentNestedCommands(const winrt::Microsoft::Terminal::Settings::Model::Command& parentCommand)
    {
        _currentNestedCommands.Clear();
        for (const auto& nameAndCommand : parentCommand.NestedCommands())
        {
            const auto action = nameAndCommand.Value();
            // nested commands cannot have keys bound to them, so just pass in the command and no keys
            auto nestedActionPaletteItem{ winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(action, winrt::hstring{}) };
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

        // revert any preview
        _filteredActionsView().SelectedIndex(-1);
        PreviewAction.raise(*this, nullptr);
    }

    void CommandPalette::EnableTabSwitcherMode(const uint32_t startIdx, TabSwitcherMode tabSwitcherMode)
    {
        // The _switcherStartIdx field allows us to select the current tab
        // We need to take it into account only in the in-order mode,
        // as an MRU mode the current tab is on top by definition.
        _switcherStartIdx = tabSwitcherMode == TabSwitcherMode::InOrder ? startIdx : 0;
        _tabSwitcherMode = tabSwitcherMode;
        _switchToMode(CommandPaletteMode::TabSwitchMode);
    }

    void CommandPalette::EnableTabSearchMode()
    {
        _switchToMode(CommandPaletteMode::TabSearchMode);
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
    void CommandPalette::_choosingItemContainer(
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
    void CommandPalette::_containerContentChanging(
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

            // attach the pointer event handlers to the container
            itemContainer.PointerEntered({ this, &CommandPalette::_listItemPointerEntered });
            itemContainer.PointerExited({ this, &CommandPalette::_listItemPointerExited });
        }
    }

    // Method Description:
    // - Reads the list of recent commands from the persistent application state
    // Return Value:
    // - The list of FilteredCommand representing the ones stored in the state
    IVector<TerminalApp::FilteredCommand> CommandPalette::_loadRecentCommands()
    {
        const auto recentCommands = ApplicationState::SharedInstance().RecentCommands();
        // If this is the first time we've opened the commandline mode and
        // there aren't any recent commands, then just return an empty vector.
        if (!recentCommands)
        {
            return single_threaded_vector<TerminalApp::FilteredCommand>();
        }

        std::vector<TerminalApp::FilteredCommand> parsedCommands;
        parsedCommands.reserve(std::min(recentCommands.Size(), CommandLineHistoryLength));

        for (const auto& c : recentCommands)
        {
            if (parsedCommands.size() >= CommandLineHistoryLength)
            {
                // Don't load more than CommandLineHistoryLength commands
                break;
            }

            if (const auto parsedCommand = _buildCommandLineCommand(c))
            {
                parsedCommands.push_back(*parsedCommand);
            }
        }
        return single_threaded_vector(std::move(parsedCommands));
    }

    // Method Description:
    // - Update recent commands by putting the provided command as most recent.
    // Upon race condition might override an update made by another window.
    // Return Value:
    // - <none>
    void CommandPalette::_updateRecentCommands(const hstring& command)
    {
        const auto recentCommands = ApplicationState::SharedInstance().RecentCommands();
        // If this is the first time we've opened the commandline mode and
        // there aren't any recent commands, then just store the new command.
        if (!recentCommands)
        {
            ApplicationState::SharedInstance().RecentCommands(single_threaded_vector(std::vector{ command }));
            return;
        }

        const auto numNewRecentCommands = std::min(recentCommands.Size() + 1, CommandLineHistoryLength);

        std::vector<hstring> newRecentCommands;
        newRecentCommands.reserve(numNewRecentCommands);

        std::unordered_set<hstring> uniqueCommands;
        uniqueCommands.reserve(numNewRecentCommands);

        newRecentCommands.push_back(command);
        uniqueCommands.insert(command);

        for (const auto& c : recentCommands)
        {
            if (newRecentCommands.size() >= CommandLineHistoryLength)
            {
                // Don't store more than CommandLineHistoryLength commands
                break;
            }

            if (uniqueCommands.emplace(c).second)
            {
                newRecentCommands.push_back(c);
            }
        }

        ApplicationState::SharedInstance().RecentCommands(single_threaded_vector(std::move(newRecentCommands)));
    }
}
