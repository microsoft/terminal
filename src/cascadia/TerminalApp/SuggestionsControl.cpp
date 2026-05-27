// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SuggestionsControl.h"

#include "CommandPaletteItems.h"

#include "SuggestionsControl.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

using namespace std::chrono_literals;

namespace winrt::TerminalApp::implementation
{
    SuggestionsControl::SuggestionsControl()
    {
        InitializeComponent();

        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        // _characterReceivedHandler is wired in SuggestionsControl.xaml on the
        // UserControl root (`CharacterReceived="_characterReceivedHandler"`).
        // _previewKeyDownHandler / _keyUpHandler / _lostFocusHandler likewise.
        // No C++-side AddHandler call exists; XAML codegen does the binding
        // during InitializeComponent() above.
        OutputDebugStringW(L"[SnippetParams] SuggestionsControl ctor: XAML wires CharacterReceived/PreviewKeyDown/PreviewKeyUp on UserControl root\n");

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

        _sizeChangedRevoker = _filteredActionsView().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // When we're in BottomUp mode, we need to adjust our own position
            // so that our bottom is aligned with our origin. This will ensure
            // that as the menu changes in size (as we filter results), the menu
            // stays "attached" to the cursor.
            if (Visibility() == Visibility::Visible && _direction == TerminalApp::SuggestionsDirection::BottomUp)
            {
                this->_recalculateTopMargin();
            }
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
        // Selection is fixed on the active snippet during parameter-filling — the list
        // is collapsed to a single row representing the snippet being filled. Don't
        // re-raise PreviewAction here; _previewResolvedInput() owns preview during fill.
        if (_paramFilling.has_value())
        {
            return;
        }

        const auto selectedCommand = _filteredActionsView().SelectedItem();
        const auto filteredCommand{ selectedCommand.try_as<winrt::TerminalApp::FilteredCommand>() };

        _filteredActionsView().ScrollIntoView(selectedCommand);

        PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"SelectedItem" });

        // Make sure to not send the preview if we're collapsed. This can
        // sometimes fire after we've been closed, which can trigger us to
        // preview the action for the empty text (as we've cleared the search
        // text as a part of closing).
        const bool isVisible{ this->Visibility() == Visibility::Visible };

        if (filteredCommand != nullptr &&
            isVisible)
        {
            const auto item{ filteredCommand.Item() };
            if (item.Type() == PaletteItemType::Action)
            {
                const auto actionPaletteItem{ winrt::get_self<ActionPaletteItem>(item) };
                const auto& cmd = actionPaletteItem->Command();
                PreviewAction.raise(*this, cmd);

                const auto description{ cmd.Description() };

                if (const auto& selected{ SelectedItem() })
                {
                    selected.SetValue(Automation::AutomationProperties::FullDescriptionProperty(), winrt::box_value(description));
                }

                if (!description.empty())
                {
                    _openTooltip(cmd);
                }
                else
                {
                    // If there's no description, then just close the tooltip.
                    _descriptionsView().Visibility(Visibility::Collapsed);
                    _descriptionsBackdrop().Visibility(Visibility::Collapsed);
                    _recalculateTopMargin();
                }
            }
        }
    }

    void SuggestionsControl::_openTooltip(Command cmd)
    {
        const auto description{ cmd.Description() };
        if (description.empty())
        {
            return;
        }

        // Build the contents of the "tooltip" based on the description
        //
        // First, the title. This is just the name of the command.
        _descriptionTitle().Inlines().Clear();
        Documents::Run titleRun;
        titleRun.Text(cmd.Name());
        _descriptionTitle().Inlines().Append(titleRun);

        // Now fill up the "subtitle" part of the "tooltip" with the actual
        // description itself.
        const auto& inlines{ _descriptionComment().Inlines() };
        inlines.Clear();

        // Split the filtered description on '\n`
        const auto lines = ::Microsoft::Console::Utils::SplitString(description, L'\n');
        // build a Run + LineBreak, and add them to the text block
        for (const auto& line : lines)
        {
            // Trim off any `\r`'s in the string. Pwsh completions will
            // frequently have these embedded.
            std::wstring trimmed{ line };
            trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), L'\r'), trimmed.end());
            if (trimmed.empty())
            {
                continue;
            }

            Documents::Run textRun;
            textRun.Text(trimmed);
            inlines.Append(textRun);
            inlines.Append(Documents::LineBreak{});
        }

        // Now, make ourselves visible.
        _descriptionsView().Visibility(Visibility::Visible);
        _descriptionsBackdrop().Visibility(Visibility::Visible);
        // and update the padding to account for our new contents.
        _recalculateTopMargin();
        return;
    }

    void SuggestionsControl::_previewKeyDownHandler(const IInspectable& /*sender*/,
                                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        const auto coreWindow = CoreWindow::GetForCurrentThread();
        const auto ctrlDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Control), CoreVirtualKeyStates::Down);
        const auto shiftDown = WI_IsFlagSet(coreWindow.GetKeyState(winrt::Windows::System::VirtualKey::Shift), CoreVirtualKeyStates::Down);

        // WALK-TIER FILL MODE: navigation + editing keys are owned by the
        // UserControl. Printable chars come through _characterReceivedHandler.
        // Handle this branch BEFORE the general key dispatch so the dropdown's
        // arrow-nav / list selection doesn't intercept Tab/Enter/Esc.
        if (_paramFilling.has_value())
        {
            auto& state = *_paramFilling;
            const auto total = state.args.Parameters().Size();

            // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
            {
                wchar_t buf[160];
                swprintf_s(buf, L"[SnippetParams] _previewKeyDownHandler (fill) key=0x%04X shift=%d ctrl=%d currentIndex=%u total=%u handled-before=%d\n",
                           static_cast<unsigned>(key), shiftDown ? 1 : 0, ctrlDown ? 1 : 0, state.currentIndex, total, e.Handled() ? 1 : 0);
                OutputDebugStringW(buf);
            }

            if (key == VirtualKey::Tab && !shiftDown)
            {
                // Tab → advance, or COMMIT if at last slot.
                if (state.currentIndex + 1 < total)
                {
                    OutputDebugStringW(L"[SnippetParams]   Tab: advance\n");
                    _advanceParameterSlot();
                }
                else
                {
                    OutputDebugStringW(L"[SnippetParams]   Tab: COMMIT (at last slot)\n");
                    _dispatchResolvedSnippet();
                }
                e.Handled(true);
                return;
            }
            if (key == VirtualKey::Tab && shiftDown)
            {
                // Shift+Tab → retreat, CLAMP at first (no wrap, no-op).
                if (state.currentIndex > 0)
                {
                    OutputDebugStringW(L"[SnippetParams]   Shift+Tab: retreat\n");
                    _retreatParameterSlot();
                }
                else
                {
                    OutputDebugStringW(L"[SnippetParams]   Shift+Tab: CLAMP at slot 0 (no-op)\n");
                }
                e.Handled(true);
                return;
            }
            if (key == VirtualKey::Enter)
            {
                // Enter → COMMIT regardless of which slot is active.
                OutputDebugStringW(L"[SnippetParams]   Enter: COMMIT\n");
                _dispatchResolvedSnippet();
                e.Handled(true);
                return;
            }
            if (key == VirtualKey::Escape)
            {
                // Esc → cancel fill, return to dropdown. Does NOT dismiss palette.
                OutputDebugStringW(L"[SnippetParams]   Esc: cancel fill\n");
                _exitParameterFilling();
                e.Handled(true);
                return;
            }
            if (key == VirtualKey::Back)
            {
                // Backspace → pop last char from active slot, refresh spans.
                if (state.currentIndex < total && !state.filledValues[state.currentIndex].empty())
                {
                    state.filledValues[state.currentIndex].pop_back();
                    {
                        wchar_t buf[256];
                        swprintf_s(buf, L"[SnippetParams]   Backspace: popped char, filledValues[%u]=\"%.80s\" (len=%zu)\n",
                                   state.currentIndex, state.filledValues[state.currentIndex].c_str(), state.filledValues[state.currentIndex].size());
                        OutputDebugStringW(buf);
                    }
                    _updatePreviewSpans();
                }
                else
                {
                    OutputDebugStringW(L"[SnippetParams]   Backspace: slot empty, no-op\n");
                }
                e.Handled(true);
                return;
            }
            // Any other navigation key (arrows, PgUp/PgDn, Home/End) is ignored
            // in fill mode — typed chars come through CharacterReceived. Mark
            // Handled so the dropdown's list-nav doesn't fire while collapsed.
            if (key == VirtualKey::Up || key == VirtualKey::Down ||
                key == VirtualKey::PageUp || key == VirtualKey::PageDown ||
                key == VirtualKey::Home || key == VirtualKey::End)
            {
                e.Handled(true);
                return;
            }
            // Fall through for everything else (modifiers, etc.) — let
            // CharacterReceived do the actual text input work.
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
            // Dismiss the palette if the text is empty; otherwise, clear the
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
    // - WALK-TIER FILL MODE: ingest printable characters typed by the user into
    //   the active parameter slot. Control chars (Tab/Enter/Esc/Backspace/etc.)
    //   are handled in _previewKeyDownHandler before this fires; we filter them
    //   out here as belt-and-suspenders. Outside fill mode, do nothing and let
    //   the event propagate normally (the dropdown TextBox is the keystroke
    //   sink in Browsing mode).
    void SuggestionsControl::_characterReceivedHandler(const IInspectable& /*sender*/,
                                                       const Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs& args)
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        const auto ch = args.Character();
        {
            wchar_t buf[200];
            const uint32_t idx = _paramFilling.has_value() ? _paramFilling->currentIndex : 0xFFFFFFFFu;
            swprintf_s(buf, L"[SnippetParams] _characterReceivedHandler char=0x%04X currentIndex=%u handled-before=%d paramFilling=%d\n",
                       static_cast<unsigned>(ch), idx, args.Handled() ? 1 : 0, _paramFilling.has_value() ? 1 : 0);
            OutputDebugStringW(buf);
        }

        if (!_paramFilling.has_value())
        {
            OutputDebugStringW(L"[SnippetParams]   bail: not in fill mode\n");
            return;
        }

        auto& state = *_paramFilling;
        const auto total = state.args.Parameters().Size();
        if (state.currentIndex >= total)
        {
            OutputDebugStringW(L"[SnippetParams]   bail: currentIndex out of range\n");
            return;
        }

        // Skip control chars (< 0x20) and DEL (0x7f). Backspace, Tab, Enter,
        // Esc all live in this range and are owned by _previewKeyDownHandler.
        if (ch < 0x20 || ch == 0x7f)
        {
            OutputDebugStringW(L"[SnippetParams]   bail: filtered control char (owned by _previewKeyDownHandler)\n");
            return;
        }

        state.filledValues[state.currentIndex].push_back(static_cast<wchar_t>(ch));
        {
            wchar_t buf[300];
            swprintf_s(buf, L"[SnippetParams]   appended char='%lc' filledValues[%u]=\"%.80s\" (len=%zu) — calling _updatePreviewSpans\n",
                       static_cast<wchar_t>(ch), state.currentIndex, state.filledValues[state.currentIndex].c_str(), state.filledValues[state.currentIndex].size());
            OutputDebugStringW(buf);
        }
        _updatePreviewSpans();
        args.Handled(true);
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
                    if (const auto paletteItem = filteredCmd.Item())
                    {
                        automationPeer.RaiseNotificationEvent(
                            Automation::Peers::AutomationNotificationKind::ItemAdded,
                            Automation::Peers::AutomationNotificationProcessing::MostRecent,
                            paletteItem.Name(),
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
        // Walk-tier: the back button only appears inside the dropdown card,
        // which is collapsed during fill mode. No fill-mode branch needed.
        PreviewAction.raise(*this, nullptr);
        _searchBox().Focus(FocusState::Programmatic);

        const auto previousAction{ _nestedActionStack.GetAt(_nestedActionStack.Size() - 1) };
        _nestedActionStack.RemoveAtEnd();

        // Repopulate nested commands when the root has not been reached yet
        if (_nestedActionStack.Size() > 0)
        {
            const auto newPreviousAction{ _nestedActionStack.GetAt(_nestedActionStack.Size() - 1) };
            const auto item{ newPreviousAction.Item() };
            if (item.Type() == PaletteItemType::Action)
            {
                const auto actionPaletteItem{ winrt::get_self<ActionPaletteItem>(item) };

                ParentCommandName(actionPaletteItem->Command().Name());
                _updateCurrentNestedCommands(actionPaletteItem->Command());
            }
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
                RS_fmt(L"SuggestionsControl_NestedCommandAnnouncement", ParentCommandName()),
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
    // - Helper method to retrieve the action from the user selected command,
    //   and dispatch that command. Also fires a tracelogging event
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
            // If we're already in parameter-filling mode, this dispatch is the user
            // accepting the current slot's value. Advance — or, if we're on the
            // last slot, fall through to the dispatch path via
            // _dispatchResolvedSnippet(). state.filledValues is already current
            // (the UserControl-level CharacterReceived / Backspace handlers keep
            // it live as the user types).
            if (_paramFilling.has_value())
            {
                const auto total = _paramFilling->args.Parameters().Size();
                // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
                {
                    wchar_t buf[200];
                    swprintf_s(buf, L"[SnippetParams] _dispatchCommand (fill branch) currentIndex=%u total=%u — %s\n",
                               _paramFilling->currentIndex, total,
                               (_paramFilling->currentIndex + 1 < total) ? L"advance" : L"COMMIT");
                    OutputDebugStringW(buf);
                }
                if (_paramFilling->currentIndex + 1 < total)
                {
                    _advanceParameterSlot();
                }
                else
                {
                    _dispatchResolvedSnippet();
                }
                return;
            }

            const auto item{ filteredCommand.Item() };
            if (item.Type() == PaletteItemType::Action)
            {
                const auto actionPaletteItem{ winrt::get_self<ActionPaletteItem>(item) };
                const auto command{ actionPaletteItem->Command() };

                // If this is a parameterized sendInput snippet, transition into
                // Filling[0] instead of dispatching. (Walk-tier of the Snippet
                // Parameters spec.)
                const auto sendInputCmd = command.ActionAndArgs().Args().try_as<SendInputArgs>();
                if (sendInputCmd)
                {
                    if (const auto parameters = sendInputCmd.Parameters();
                        parameters && parameters.Size() > 0)
                    {
                        _enterParameterFilling(filteredCommand, sendInputCmd);
                        return;
                    }
                }

                if (command.HasNestedCommands())
                {
                    // If this Command had subcommands, then don't dispatch the
                    // action. Instead, display a new list of commands for the user
                    // to pick from.
                    _nestedActionStack.Append(filteredCommand);
                    ParentCommandName(command.Name());
                    _updateCurrentNestedCommands(command);

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
                    DispatchCommandRequested.raise(*this, command);

                    if (const auto& sendInputCmd = command.ActionAndArgs().Args().try_as<SendInputArgs>())
                    {
                        if (til::starts_with(sendInputCmd.Input(), L"winget"))
                        {
                            TraceLoggingWrite(
                                g_hTerminalAppProvider,
                                "QuickFixSuggestionUsed",
                                TraceLoggingDescription("Event emitted when a winget suggestion from is used"),
                                TraceLoggingValue("SuggestionsUI", "Source"),
                                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
                        }
                    }

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
    // - Transition from Browsing to Filling[0] for a parameterized snippet. Collapses
    //   the filtered list to a single row representing the active snippet, surfaces
    //   the first parameter's description as placeholder text, and primes the live
    //   preview path.
    // Arguments:
    // - filteredCommand: the snippet FilteredCommand that was accepted from the list.
    // - args: that snippet's SendInputArgs (already known by the caller to have a
    //   non-empty Parameters collection).
    // Return Value:
    // - <none>
    void SuggestionsControl::_enterParameterFilling(const winrt::TerminalApp::FilteredCommand& filteredCommand,
                                                    const winrt::Microsoft::Terminal::Settings::Model::SendInputArgs& args)
    {
        ParameterFillingState state;
        state.snippet = filteredCommand;
        state.args = args;
        state.currentIndex = 0;
        state.filledValues.resize(args.Parameters().Size());

        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            std::wstring_view inputView{ args.Input() };
            if (inputView.size() > 80)
            {
                inputView = inputView.substr(0, 80);
            }
            wchar_t buf[300];
            swprintf_s(buf, L"[SnippetParams] _enterParameterFilling: entering param-fill mode, paramCount=%u currentIndex=0 input=\"%.80s\"\n",
                       args.Parameters().Size(), inputView.data());
            OutputDebugStringW(buf);
        }

        _paramFilling.emplace(std::move(state));

        // WALK-TIER UX: the terminal buffer itself shows the template (via
        // TermControl::PreviewInputSpans). Collapse the dropdown card entirely
        // so only the parameter-description tooltip floats near the cursor.
        // The whole snippet-picker UI (search box, parent row + back button,
        // list view) lives inside `_backdrop`; collapsing the parent hides
        // them all in one shot with no per-element bookkeeping. The
        // `_descriptionsBackdrop` sibling stays in `_listAndDescriptionStack`
        // and becomes the only visible chrome.
        _backdrop().Visibility(Visibility::Collapsed);

        // Keystroke capture: the UserControl itself receives PreviewKeyDown /
        // CharacterReceived during fill (no TextBox). Programmatic focus on
        // *this so events arrive even though the search box is now hidden.
        const auto focusOk = Focus(Windows::UI::Xaml::FocusState::Programmatic);
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams] _enterParameterFilling: Focus(Programmatic) on UserControl returned %d, backdrop collapsed\n", focusOk ? 1 : 0);
            OutputDebugStringW(buf);
        }

        _updateUIForParameterSlot();

        // The outer UserControl was positioned by Open() using the full
        // backdrop height. Now that the backdrop is collapsed and only the
        // description tooltip is visible, re-anchor so it sits next to the
        // cursor instead of floating where the full list used to be. Must
        // happen AFTER _updateUIForParameterSlot() so the description has
        // its text populated and a meaningful DesiredSize.
        _recalculateTopMargin();
    }

    // Method Description:
    // - Tear down Filling[i] state and restore the snippet-picker dropdown.
    //   WALK-TIER semantics: Esc returns the user to the dropdown (does NOT
    //   dismiss the palette). Clears the buffer-side preview.
    void SuggestionsControl::_exitParameterFilling()
    {
        _clearPreviewSpans();

        _paramFilling.reset();
        ParentCommandName(L"");
        SearchBoxPlaceholderText(RS_(L"SuggestionsControl_SearchBox/PlaceholderText"));

        // Restore the dropdown card and tear down the parameter tooltip.
        _backdrop().Visibility(Visibility::Visible);
        _descriptionsView().Visibility(Visibility::Collapsed);
        _descriptionsBackdrop().Visibility(Visibility::Collapsed);

        // The backdrop is back; re-anchor so the (now full-height) control
        // sits where Open() originally placed it instead of where the
        // shrunken description tooltip left it.
        _recalculateTopMargin();

        // Restore focus to the appropriate element for the current mode so the
        // user can keep navigating the snippet list immediately after Esc.
        if (_mode == SuggestionsMode::Palette)
        {
            _searchBox().Visibility(Visibility::Visible);
            _searchBox().Focus(FocusState::Programmatic);
        }
        else if (_mode == SuggestionsMode::Menu)
        {
            _searchBox().Visibility(Visibility::Collapsed);
            if (const auto& dependencyObj = SelectedItem().try_as<winrt::Windows::UI::Xaml::DependencyObject>())
            {
                Input::FocusManager::TryFocusAsync(dependencyObj, FocusState::Programmatic);
            }
        }

        _recalculateTopMargin();
    }

    // Method Description:
    // - Move from Filling[i] to Filling[i+1]. Caller must have already committed
    //   the current slot's value into filledValues before calling.
    void SuggestionsControl::_advanceParameterSlot()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            const uint32_t before = _paramFilling->currentIndex;
            const uint32_t after = before + 1;
            const uint32_t total = _paramFilling->args.Parameters().Size();
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams] _advanceParameterSlot: index %u -> %u (count=%u)\n", before, after, total);
            OutputDebugStringW(buf);
        }
        _paramFilling->currentIndex++;
        _updateUIForParameterSlot();
    }

    // Method Description:
    // - Move from Filling[i] back to Filling[i-1]. CLAMPS at i==0 (no wrap).
    //   filledValues persists across slot transitions, so re-entry restores
    //   the prior text automatically.
    void SuggestionsControl::_retreatParameterSlot()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            const uint32_t before = _paramFilling->currentIndex;
            const uint32_t total = _paramFilling->args.Parameters().Size();
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams] _retreatParameterSlot: index %u -> %u (count=%u)%s\n",
                       before, (before == 0 ? 0u : before - 1u), total, (before == 0 ? L" [CLAMP no-op]" : L""));
            OutputDebugStringW(buf);
        }
        if (_paramFilling->currentIndex == 0)
        {
            return;
        }
        _paramFilling->currentIndex--;
        _updateUIForParameterSlot();
    }

    // Method Description:
    // - Refresh the parameter-filling tooltip for the current slot, then push
    //   a fresh PreviewInputSpans batch so the terminal buffer mirrors the new
    //   active-tabstop. The dropdown card is already collapsed; we own only
    //   the `_descriptionsBackdrop` card here.
    void SuggestionsControl::_updateUIForParameterSlot()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        OutputDebugStringW(L"[SnippetParams] _updateUIForParameterSlot: entry\n");
        if (!_paramFilling.has_value())
        {
            OutputDebugStringW(L"[SnippetParams]   bail: not in fill mode\n");
            return;
        }
        const auto& state = *_paramFilling;
        const auto parameters = state.args.Parameters();
        const auto total = parameters.Size();
        if (state.currentIndex >= total)
        {
            OutputDebugStringW(L"[SnippetParams]   bail: currentIndex out of range\n");
            return;
        }
        const auto current = parameters.GetAt(state.currentIndex);
        const auto name = current.Name();
        const auto description = current.Description();
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            wchar_t buf[300];
            swprintf_s(buf, L"[SnippetParams]   slot %u/%u name=\"%.40s\" restoredValue=\"%.80s\"\n",
                       state.currentIndex, total, name.c_str(), state.filledValues[state.currentIndex].c_str());
            OutputDebugStringW(buf);
        }

        // Tooltip title = parameter name; subtitle = description. Reuses the
        // existing tooltip composition pattern (_descriptionTitle as 14pt
        // header, _descriptionComment as multi-line body via Documents::Run +
        // LineBreak). See _openTooltip for the original template.
        _descriptionTitle().Inlines().Clear();
        {
            Documents::Run titleRun;
            titleRun.Text(name);
            _descriptionTitle().Inlines().Append(titleRun);
        }
        {
            const auto& inlines{ _descriptionComment().Inlines() };
            inlines.Clear();
            if (!description.empty())
            {
                const auto lines = ::Microsoft::Console::Utils::SplitString(description, L'\n');
                for (const auto& line : lines)
                {
                    std::wstring trimmed{ line };
                    trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), L'\r'), trimmed.end());
                    if (trimmed.empty())
                    {
                        continue;
                    }
                    Documents::Run textRun;
                    textRun.Text(trimmed);
                    inlines.Append(textRun);
                    inlines.Append(Documents::LineBreak{});
                }
            }
        }
        _descriptionsView().Visibility(Visibility::Visible);
        _descriptionsBackdrop().Visibility(Visibility::Visible);
        _recalculateTopMargin();

        // Accessibility: LiveRegion-style announcement on every slot transition.
        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(*this) })
        {
            const auto announcement = description.empty() ?
                                          winrt::hstring{ RS_fmt(L"SuggestionsControl_ParameterAnnouncementShort", name) } :
                                          winrt::hstring{ RS_fmt(L"SuggestionsControl_ParameterAnnouncement", name, description) };
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                announcement,
                L"SuggestionsControlParameterSlotChanged" /* unique name for this notification category */);
        }

        _updatePreviewSpans();
    }

    // Method Description:
    // - Build the spans IVector for the current snippet + accumulated values +
    //   active tabstop, and raise PreviewInputSpansRequested. TerminalPage
    //   forwards to the active TermControl.
    //
    // The span-build is delegated to `SendInputArgs::BuildPreviewSpans`, a
    // projected method on Settings.Model. Cross-project call goes through the
    // WinRT projection — no TerminalSettingsModel internal headers reach into
    // this TU. The returned IVector<SnippetPreviewRun> carries the same
    // (text, kind) pairs the implementation-side free function used to return
    // via vector<pair<wstring, SnippetPreviewSpanKind>>; iteration is via
    // run.Text() / run.Kind() on the projected runtimeclass.
    void SuggestionsControl::_updatePreviewSpans()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        OutputDebugStringW(L"[SnippetParams] _updatePreviewSpans: entry\n");
        if (!_paramFilling.has_value())
        {
            OutputDebugStringW(L"[SnippetParams]   bail: not in fill mode\n");
            return;
        }
        const auto& state = *_paramFilling;
        const auto valuesMap = _buildParameterMap();

        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams]   valuesMap.Size=%u currentIndex=%u — calling SendInputArgs::BuildPreviewSpans\n",
                       valuesMap.Size(), state.currentIndex);
            OutputDebugStringW(buf);
        }

        winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::SnippetPreviewRun> modelSpans{ nullptr };
        try
        {
            modelSpans = state.args.BuildPreviewSpans(valuesMap, state.currentIndex);
        }
        catch (...)
        {
            OutputDebugStringW(L"[SnippetParams]   EXCEPTION: BuildPreviewSpans threw\n");
            return;
        }

        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            const uint32_t runCount = modelSpans ? modelSpans.Size() : 0u;
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams]   BuildPreviewSpans returned %u run(s)\n", runCount);
            OutputDebugStringW(buf);
        }

        auto outSpans = winrt::single_threaded_vector<winrt::Microsoft::Terminal::Control::PreviewInputSpan>();
        uint32_t i = 0;
        for (const auto& run : modelSpans)
        {
            // Model::SnippetPreviewSpanKind ↔ Control::PreviewInputSpanKind:
            // both enums share the same integral values per IDL. The identity
            // cast is well-defined; an explicit switch would just add line
            // noise. Renderer-side attribute mapping (Active = inverse video,
            // Placeholder = dim) lives in ControlCore.
            const auto controlKind = static_cast<winrt::Microsoft::Terminal::Control::PreviewInputSpanKind>(static_cast<int32_t>(run.Kind()));
            // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
            {
                wchar_t buf[300];
                swprintf_s(buf, L"[SnippetParams]     run[%u] kind=%d text=\"%.80s\"\n",
                           i, static_cast<int>(run.Kind()), run.Text().c_str());
                OutputDebugStringW(buf);
            }
            outSpans.Append(winrt::Microsoft::Terminal::Control::PreviewInputSpan{ run.Text(), controlKind });
            ++i;
        }

        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            wchar_t buf[160];
            swprintf_s(buf, L"[SnippetParams]   raising PreviewInputSpansRequested with %u span(s)\n", outSpans.Size());
            OutputDebugStringW(buf);
        }
        try
        {
            PreviewInputSpansRequested.raise(*this, outSpans);
            OutputDebugStringW(L"[SnippetParams]   PreviewInputSpansRequested.raise returned OK\n");
        }
        catch (...)
        {
            OutputDebugStringW(L"[SnippetParams]   EXCEPTION: PreviewInputSpansRequested.raise threw\n");
        }
    }

    // Method Description:
    // - Clear the terminal-side preview by raising PreviewInputSpansRequested
    //   with an empty IVector (semantic equivalent of the legacy
    //   PreviewInput(L"") dismiss).
    void SuggestionsControl::_clearPreviewSpans()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        OutputDebugStringW(L"[SnippetParams] _clearPreviewSpans: raising PreviewInputSpansRequested with empty IVector\n");
        auto emptySpans = winrt::single_threaded_vector<winrt::Microsoft::Terminal::Control::PreviewInputSpan>();
        PreviewInputSpansRequested.raise(*this, emptySpans);
    }

    // Method Description:
    // - End-of-fill dispatch path. Builds a Command whose SendInputArgs.Input is
    //   the fully-resolved string (with every ${<name>} token substituted), then
    //   tears down fill state, closes the palette, and raises DispatchCommandRequested
    //   with the transient resolved Command. The original snippet args are NOT mutated.
    void SuggestionsControl::_dispatchResolvedSnippet()
    {
        // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
        {
            const uint32_t idx = _paramFilling.has_value() ? _paramFilling->currentIndex : 0xFFFFFFFFu;
            const uint32_t total = _paramFilling.has_value() ? _paramFilling->args.Parameters().Size() : 0u;
            wchar_t buf[200];
            swprintf_s(buf, L"[SnippetParams] _dispatchResolvedSnippet: COMMIT path entry, currentIndex=%u total=%u\n", idx, total);
            OutputDebugStringW(buf);
        }
        const auto cmd = _buildResolvedCommand();

        // Clear preview FIRST so the buffer is empty when the resolved input
        // gets sent for real (avoids a one-frame double-write).
        _clearPreviewSpans();

        const auto searchTextLength = (_paramFilling.has_value() && !_paramFilling->filledValues.empty()) ?
                                          _paramFilling->filledValues.back().size() :
                                          size_t{ 0 };

        // Clear fill state FIRST so _close() doesn't try to do anything fill-aware.
        _paramFilling.reset();
        ParentCommandName(L"");
        SearchBoxPlaceholderText(RS_(L"SuggestionsControl_SearchBox/PlaceholderText"));

        // Restore the dropdown card and tear down the parameter tooltip so the
        // next Open() doesn't inherit a collapsed backdrop.
        _backdrop().Visibility(Visibility::Visible);
        _descriptionsView().Visibility(Visibility::Collapsed);
        _descriptionsBackdrop().Visibility(Visibility::Collapsed);

        _close();

        DispatchCommandRequested.raise(*this, cmd);

        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "SuggestionsControlDispatchedParameterizedSnippet",
            TraceLoggingDescription("Event emitted when the user dispatches a parameterized sendInput snippet via the parameter-filling UI"),
            TraceLoggingUInt32(static_cast<uint32_t>(searchTextLength), "SearchTextLength", "Number of characters in the last parameter slot"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    // Method Description:
    // - Build the parameter map handed to SendInputArgs.Resolve() and to
    //   BuildSnippetPreviewSpans. Reads straight from `state.filledValues`
    //   (the UserControl-level keystroke handler keeps it current — there is
    //   no separate "live" buffer to merge).
    Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>
    SuggestionsControl::_buildParameterMap()
    {
        auto map = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        if (!_paramFilling.has_value())
        {
            // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
            OutputDebugStringW(L"[SnippetParams] _buildParameterMap: not in fill mode, returning empty map\n");
            return map;
        }
        const auto& state = *_paramFilling;
        const auto parameters = state.args.Parameters();
        const auto total = parameters.Size();
        for (uint32_t i = 0; i < total; ++i)
        {
            const auto pName = parameters.GetAt(i).Name();
            map.Insert(pName, winrt::hstring{ state.filledValues[i] });
            // [SnippetParams] THROWAWAY DEBUG LOGGING — remove before commit.
            {
                wchar_t buf[300];
                swprintf_s(buf, L"[SnippetParams] _buildParameterMap[%u] name=\"%.40s\" value=\"%.80s\"\n",
                           i, pName.c_str(), state.filledValues[i].c_str());
                OutputDebugStringW(buf);
            }
        }
        return map;
    }

    // Method Description:
    // - Construct a transient Command whose SendInputArgs.Input is the resolved
    //   (substituted) string, preserving the original snippet's name. The
    //   original snippet's args are NOT mutated.
    winrt::Microsoft::Terminal::Settings::Model::Command SuggestionsControl::_buildResolvedCommand()
    {
        const auto& state = *_paramFilling;
        const auto map = _buildParameterMap();
        const auto resolvedInput = state.args.Resolve(map);

        winrt::hstring originalName;
        if (const auto item = state.snippet.Item())
        {
            if (item.Type() == PaletteItemType::Action)
            {
                const auto actionPaletteItem{ winrt::get_self<ActionPaletteItem>(item) };
                originalName = actionPaletteItem->Command().Name();
            }
        }

        ActionAndArgs resolvedActionAndArgs{ ShortcutAction::SendInput, SendInputArgs{ resolvedInput } };
        Command resolvedCommand;
        if (!originalName.empty())
        {
            resolvedCommand.Name(originalName);
        }
        resolvedCommand.ActionAndArgs(resolvedActionAndArgs);
        return resolvedCommand;
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
        // Walk-tier: in fill mode the search box is hidden inside the
        // collapsed `_backdrop` and plays no role; keystrokes are captured
        // at the UserControl level. Nothing to filter here — just bail.
        if (_paramFilling.has_value())
        {
            return;
        }

        // We're setting _lastFilterTextWasEmpty here, because if the user tries
        // to backspace the last character in the input, the Backspace KeyDown
        // event will fire _before_ _filterTextChanged does. Updating the value
        // here will ensure that we can check this case appropriately.
        _lastFilterTextWasEmpty = _searchBox().Text().empty();

        const auto lastSelectedIndex = std::max(0, _filteredActionsView().SelectedIndex()); // SelectedIndex will return -1 for "nothing"

        _updateFilteredActions();

        if (const auto newSelectedIndex = _filteredActionsView().SelectedIndex();
            newSelectedIndex == -1)
        {
            // Make sure something stays selected
            _scrollToIndex(lastSelectedIndex);
        }
        else
        {
            // BODGY: Calling ScrollIntoView on a ListView doesn't always work
            // immediately after a change to the items. See:
            // https://stackoverflow.com/questions/16942580/why-doesnt-listview-scrollintoview-ever-work
            // The SelectionChanged thing we do (in _selectedCommandChanged),
            // but because we're also not changing the actual selected item when
            // the size of the list grows (it _stays_ selected, so it never
            // _changes_), we never get a SelectionChanged.
            //
            // To mitigate, only in the case of totally clearing out the filter
            // (like hitting `esc`), we want to briefly select the 0th item,
            // then immediately select the one we want to make visible. That
            // will make sure we get a SelectionChanged when the ListView is
            // ready, and we can use that to scroll to the right item.
            //
            // If we do this on _every_ change, then the preview text flickers
            // between the 0th item and the correct one.
            if (_lastFilterTextWasEmpty)
            {
                _filteredActionsView().SelectedIndex(0);
            }
            _scrollToIndex(newSelectedIndex);
        }

        const auto currentNeedleHasResults{ _filteredActions.Size() > 0 };
        if (!currentNeedleHasResults)
        {
            PreviewAction.raise(*this, nullptr);
        }
        _noMatchesText().Visibility(currentNeedleHasResults ? Visibility::Collapsed : Visibility::Visible);
        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(_searchBox()) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                currentNeedleHasResults ?
                    winrt::hstring{ RS_fmt(L"SuggestionsControl_MatchesAvailable", _filteredActions.Size()) } :
                    NoMatchesText(), // what to announce if results were found
                L"SuggestionsControlResultAnnouncement" /* unique name for this group of notifications */);
        }
    }

    Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> SuggestionsControl::FilteredActions()
    {
        return _filteredActions;
    }

    void SuggestionsControl::SetCommands(const Collections::IVector<Command>& actions)
    {
        _allCommands.Clear();
        for (const auto& action : actions)
        {
            // key chords aren't relevant in the suggestions control, so make the palette item with just the command and no keys
            auto actionPaletteItem{ winrt::make<winrt::TerminalApp::implementation::ActionPaletteItem>(action, winrt::hstring{}) };
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
        ParsedCommandLineText(L"");
        _searchBox().Text(L"");
        _searchBox().Select(_searchBox().Text().size(), 0);

        _nestedActionStack.Clear();
        ParentCommandName(L"");
        _currentNestedCommands.Clear();
        // Leaving this block of code outside the above if-statement
        // guarantees that the correct text is shown for the mode
        // whenever _switchToMode is called.

        SearchBoxPlaceholderText(RS_(L"SuggestionsControl_SearchBox/PlaceholderText"));
        NoMatchesText(RS_(L"SuggestionsControl_NoMatchesText/Text"));
        ControlName(RS_(L"SuggestionsControlName"));

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
            auto pattern = std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(searchText));

            for (const auto& action : commandsToFilter)
            {
                // Update filter for all commands
                // This will modify the highlighting but will also lead to re-computation of weight (and consequently sorting).
                // Pay attention that it already updates the highlighting in the UI
                auto impl = winrt::get_self<implementation::FilteredCommand>(action);
                impl->UpdateFilter(pattern);

                // if there is active search we skip commands with 0 weight
                if (searchText.empty() || action.Weight() > 0)
                {
                    actions.push_back(action);
                }
            }
        }

        // No sorting in palette mode, so results are still filtered, but in the
        // original order. This feels more right for something like
        // recentCommands.
        //
        // This is in contrast to the Command Palette, which always sorts its
        // actions.

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
    void SuggestionsControl::_close()
    {
        Visibility(Visibility::Collapsed);

        // Clear the text box each time we close the dialog. This is consistent with VsCode.
        _searchBox().Text(L"");

        _nestedActionStack.Clear();

        // If we were dismissed mid-fill (LostFocus, light-dismiss, etc.) clear
        // the buffer-side preview and restore the dropdown card so the next
        // Open() doesn't inherit a collapsed `_backdrop` or stuck-visible
        // parameter tooltip. (_dispatchResolvedSnippet also performs this
        // restoration on the commit path; this is the defensive backstop for
        // every other exit path — Esc/light-dismiss/LostFocus.)
        if (_paramFilling.has_value())
        {
            _clearPreviewSpans();
            _backdrop().Visibility(Visibility::Visible);
            _descriptionsView().Visibility(Visibility::Collapsed);
            _descriptionsBackdrop().Visibility(Visibility::Collapsed);
        }

        // Clear any in-flight parameter-filling state — otherwise the next open
        // would inherit a stale Filling[i] mode.
        _paramFilling.reset();

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
                    const auto helpText = winrt::box_value(RS_(L"SuggestionsControl_MoreOptions/[using:Windows.UI.Xaml.Automation]AutomationProperties/HelpText"));
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

        // We need to move either the list of suggestions, or the tooltip, to
        // the top of the stack panel (depending on the layout).
        Grid controlToMoveToTop = nullptr;

        if (_direction == TerminalApp::SuggestionsDirection::TopDown)
        {
            Controls::Grid::SetRow(_searchBox(), 0);
            controlToMoveToTop = _backdrop();
        }
        else // BottomUp
        {
            Controls::Grid::SetRow(_searchBox(), 4);
            controlToMoveToTop = _descriptionsBackdrop();
        }

        assert(controlToMoveToTop);
        const auto& children{ _listAndDescriptionStack().Children() };
        uint32_t index;
        if (children.IndexOf(controlToMoveToTop, index))
        {
            children.Move(index, 0);
        }
    }

    void SuggestionsControl::_recalculateTopMargin()
    {
        auto currentMargin = Margin();
        // Call Measure() on the descriptions backdrop, so that it gets it's new
        // DesiredSize for this new description text.
        //
        // If you forget this, then we _probably_ weren't laid out since
        // updating that text, and the ActualHeight will be the _last_
        // description's height.
        _descriptionsBackdrop().Measure({
            static_cast<float>(ActualWidth()),
            static_cast<float>(ActualHeight()),
        });

        // Now, position vertically.
        if (_direction == TerminalApp::SuggestionsDirection::TopDown)
        {
            // The control should open right below the cursor, with the list
            // extending below. This is easy, we can just use the cursor as the
            // origin (more or less)
            currentMargin.Top = (_anchor.Y);
        }
        else
        {
            // Bottom Up.

            // This is wackier, because we need to calculate the offset upwards
            // from our anchor. So we need to get the size of our elements:
            //
            // NOTE: when `_backdrop` is collapsed (param-fill mode), its
            // `ActualHeight()` may still report the prior (visible) value
            // until the next layout pass — so we can't rely on it being 0
            // just because we set Visibility::Collapsed. Check Visibility()
            // directly instead, which is the authoritative state.
            const auto backdropHeight = _backdrop().Visibility() == Visibility::Visible ?
                                            _backdrop().ActualHeight() :
                                            0;
            const auto descriptionDesiredHeight = _descriptionsBackdrop().Visibility() == Visibility::Visible ?
                                                      _descriptionsBackdrop().DesiredSize().Height :
                                                      0;

            const auto marginTop = (_anchor.Y - backdropHeight - descriptionDesiredHeight);

            currentMargin.Top = marginTop;
        }
        Margin(currentMargin);
    }

    void SuggestionsControl::Open(TerminalApp::SuggestionsMode mode,
                                  const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command>& commands,
                                  winrt::hstring filter,
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

        // Is there space in the window below the cursor to open the menu downwards?
        const bool canOpenDownwards = (_anchor.Y + characterHeight + ActualHeight()) < space.Height;
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
        const auto maxX = gsl::narrow_cast<int>(space.Width - ActualWidth());
        const auto clampedX = std::clamp(proposedX, 0, maxX);

        // Create a thickness for the new margins. This will set the left, then
        // we'll go update the top separately
        Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(clampedX, 0, 0, 0));
        _recalculateTopMargin();

        _searchBox().Text(filter);

        // If we're in bottom-up mode, make sure to re-select the _last_ item in
        // the list, so that it's like we're starting with the most recent one
        // selected.
        if (_direction == TerminalApp::SuggestionsDirection::BottomUp)
        {
            const auto last = _filteredActionsView().Items().Size() - 1;
            _scrollToIndex(last);
        }
        // Move the cursor to the very last position, so it starts immediately
        // after the text. This is apparently done by starting a 0-wide
        // selection starting at the end of the string.
        _searchBox().Select(filter.size(), 0);
    }
}
