// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EditAction.h"
#include "EditAction.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // Depth-first search of the visual tree under 'root' for the first TextBox (the AutoSuggestBox's
    // inner editable TextBox), so we can select its text on focus.
    static Controls::TextBox _findChildTextBox(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto textBox = root.try_as<Controls::TextBox>())
        {
            return textBox;
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = _findChildTextBox(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    EditAction::EditAction()
    {
        InitializeComponent();

        ActionType().Header(box_value(RS_(L"Actions_ShortcutAction/Text")));
        ActionName().Header(box_value(RS_(L"Actions_Name/Text")));
        Automation::AutomationProperties::SetName(KeyBindingsContainer(), RS_(L"EditAction_KeyBindings/Text"));
        Automation::AutomationProperties::SetName(AdditionalCustomizationsContainer(), RS_(L"EditAction_AdditionalCustomizations/Text"));
        Automation::AutomationProperties::SetName(NewKeyBinding(), RS_(L"EditAction_NewKeyBinding/Header"));
    }

    void EditAction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::CommandViewModel>();

        // Suppress opening the suggestion list for the whole page-entry window; see LostFocus.
        _isPageEntryFocus = true;
        _propagateWindowRootRevoker = _ViewModel.PropagateWindowRootRequested(
            winrt::auto_revoke,
            [windowRoot = args.WindowRoot()](const IInspectable&, const Editor::ArgWrapper& wrapper) {
                if (wrapper)
                {
                    wrapper.WindowRoot(windowRoot);
                }
            });
        auto weakThis = get_weak();
        _focusContainerRevoker = _ViewModel.FocusContainer(
            winrt::auto_revoke,
            [weakThis](const auto&, const auto& args) {
                if (auto page{ weakThis.get() })
                {
                    if (auto kcVM{ args.try_as<KeyChordViewModel>() })
                    {
                        // Realize the containers in case this key chord was newly added.
                        page->KeyChordItems().UpdateLayout();
                        if (const auto& container = page->KeyChordItems().ContainerFromItem(*kcVM))
                        {
                            const auto root = container.try_as<DependencyObject>();
                            if (kcVM->IsInEditMode())
                            {
                                // Focus the editable listener so the user can type a chord.
                                if (const auto listener = FindKeyChordListener(root))
                                {
                                    listener.FocusInput();
                                    return;
                                }
                            }
                            // Otherwise (left edit mode) return focus to the row's first
                            // focusable control (the edit pencil).
                            if (const auto focusable = FindFirstFocusable(root))
                            {
                                focusable.Focus(FocusState::Programmatic);
                            }
                        }
                    }
                }
            });
        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            // Page-entry focus goes to "Shortcut type". A key chord row is only ever put into
            // edit mode from this page (via "Add key binding"), which focuses its listener
            // through the FocusContainer handler above.
            ShortcutActionBox().Focus(FocusState::Programmatic);
        });

        // Initialize AutoSuggestBox with current action and store last valid action
        if (_ViewModel.ProposedShortcutActionName())
        {
            const auto currentAction = winrt::unbox_value<winrt::hstring>(_ViewModel.ProposedShortcutActionName());
            ShortcutActionBox().Text(currentAction);
            _lastValidAction = currentAction;
        }
    }

    void EditAction::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _focusContainerRevoker.revoke();
        _propagateWindowRootRevoker.revoke();
        _layoutUpdatedRevoker.revoke();

        if (_ViewModel)
        {
            // A key chord the user started but never accepted was never written to the settings
            // model. Drop it, so an empty row doesn't linger on the Actions page.
            get_self<CommandViewModel>(_ViewModel)->CancelPendingKeyChordEdit();
        }
    }

    void EditAction::ShortcutActionBox_GettingFocus(const IInspectable& /*sender*/, const Windows::UI::Xaml::Input::GettingFocusEventArgs& args)
    {
        // Open on Tab, but not on page entry.
        // FocusState is unreliable, so use InputDevice: "Keyboard" means we tabbed to focus.
        _openSuggestionsOnFocus = args.InputDevice() == Windows::UI::Xaml::Input::FocusInputDeviceKind::Keyboard && !_isPageEntryFocus;
    }

    void EditAction::ShortcutActionBox_GotFocus(const IInspectable& sender, const RoutedEventArgs&)
    {
        const auto box = sender.as<AutoSuggestBox>();

        // Seeding ItemsSource inside this branch is intentional: assigning it on a focused
        // AutoSuggestBox opens the popup on its own. Typing filters via ShortcutActionBox_TextChanged.
        if (_openSuggestionsOnFocus)
        {
            // Only rebuild the list if we don't have a cached list or if the cached list is filtered
            if (!_filteredActions || !_currentActionFilter.empty())
            {
                // Open the suggestions list with all available actions
                std::vector<winrt::hstring> allActions;
                for (const auto& action : _ViewModel.AvailableShortcutActions())
                {
                    allActions.push_back(action);
                }

                _filteredActions = winrt::single_threaded_observable_vector(std::move(allActions));
                _currentActionFilter = L"";
                box.ItemsSource(_filteredActions);
            }
            box.IsSuggestionListOpen(true);
        }

        // Select all current text so the user can immediately overwrite it. AutoSuggestBox has no
        // SelectAll, so use the inner TextBox.
        if (const auto textBox = _findChildTextBox(box.as<Windows::UI::Xaml::DependencyObject>()))
        {
            textBox.SelectAll();
        }
    }

    void EditAction::ShortcutActionBox_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs& args)
    {
        if (args.Reason() == AutoSuggestionBoxTextChangeReason::UserInput)
        {
            const auto searchText = sender.Text();
            std::vector<winrt::hstring> filtered;

            for (const auto& action : _ViewModel.AvailableShortcutActions())
            {
                // TODO: Update this to use fzf later
                if (til::contains_linguistic_insensitive(action, searchText))
                {
                    filtered.push_back(action);
                }
            }

            _filteredActions = winrt::single_threaded_observable_vector(std::move(filtered));
            _currentActionFilter = searchText;
            sender.ItemsSource(_filteredActions);
        }
    }

    void EditAction::ShortcutActionBox_QuerySubmitted(const AutoSuggestBox& sender, const AutoSuggestBoxQuerySubmittedEventArgs& args)
    {
        const auto submittedText = args.QueryText();

        for (const auto& action : _ViewModel.AvailableShortcutActions())
        {
            if (action == submittedText)
            {
                _ViewModel.ProposedShortcutActionName(winrt::box_value(submittedText));
                _lastValidAction = submittedText;
                return;
            }
        }

        // If we get here, we never found a match.
        // Revert to the last valid action
        sender.Text(_lastValidAction);
    }

    void EditAction::ShortcutActionBox_LostFocus(const IInspectable& sender, const RoutedEventArgs&)
    {
        _isPageEntryFocus = false;

        // The auto suggest box does a weird thing where it reverts to the last query text when you
        // keyboard navigate out of it. Intercept it here and keep the correct text.
        const auto box = sender.as<AutoSuggestBox>();
        const auto currentText = box.Text();

        if (currentText != _lastValidAction && !_lastValidAction.empty())
        {
            box.Text(_lastValidAction);
        }
    }
}
