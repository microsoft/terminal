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
    // Depth-first search of the visual tree under 'root' for the first KeyChordListener.
    static Editor::KeyChordListener _findKeyChordListener(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto listener = root.try_as<Editor::KeyChordListener>())
        {
            return listener;
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = _findKeyChordListener(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    // Depth-first search of the visual tree under 'root' for the first focusable, visible
    // control (e.g. a key chord row's edit pencil), used to restore focus to a row after it
    // leaves edit mode.
    static Controls::Control _findFirstFocusable(const Windows::UI::Xaml::DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto control = root.try_as<Controls::Control>())
        {
            if (control.IsTabStop() && control.IsEnabled() && control.Visibility() == Visibility::Visible)
            {
                return control;
            }
        }
        const auto count = Windows::UI::Xaml::Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Windows::UI::Xaml::Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = _findFirstFocusable(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    EditAction::EditAction()
    {
    }

    void EditAction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        Automation::AutomationProperties::SetName(KeyBindingsContainer(), RS_(L"EditAction_KeyBindings/Text"));
        Automation::AutomationProperties::SetName(AdditionalCustomizationsContainer(), RS_(L"EditAction_AdditionalCustomizations/Text"));
        Automation::AutomationProperties::SetName(NewKeyBinding(), RS_(L"EditAction_NewKeyBinding/Header"));

        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _ViewModel = args.ViewModel().as<Editor::CommandViewModel>();
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
                                if (const auto listener = _findKeyChordListener(root))
                                {
                                    listener.FocusInput();
                                    return;
                                }
                            }
                            // Otherwise (left edit mode) return focus to the row's first
                            // focusable control (the edit pencil).
                            if (const auto focusable = _findFirstFocusable(root))
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

            // If we navigated here to edit a specific key chord (its row arrives already in
            // edit mode), focus that row's KeyChordListener so the user can immediately type a
            // new chord. Otherwise, focus the command name text box.
            Editor::KeyChordViewModel chordToEdit{ nullptr };
            if (const auto& chords = _ViewModel.KeyChordList())
            {
                for (const auto& chord : chords)
                {
                    if (chord.IsInEditMode())
                    {
                        chordToEdit = chord;
                        break;
                    }
                }
            }

            if (chordToEdit)
            {
                KeyChordItems().UpdateLayout();
                if (const auto& container = KeyChordItems().ContainerFromItem(chordToEdit))
                {
                    if (const auto listener = _findKeyChordListener(container.try_as<DependencyObject>()))
                    {
                        listener.FocusInput();
                        return;
                    }
                }
            }

            // Default page-entry focus goes to "Shortcut type"
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

    void EditAction::ShortcutActionBox_GotFocus(const IInspectable& sender, const RoutedEventArgs&)
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
            sender.as<AutoSuggestBox>().ItemsSource(_filteredActions);
        }
        sender.as<AutoSuggestBox>().IsSuggestionListOpen(true);
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
