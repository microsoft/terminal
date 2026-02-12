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
    EditAction::EditAction()
    {
    }

    void EditAction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToCommandArgs>();
        _ViewModel = args.Command();
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
                        if (const auto& container = page->KeyChordListView().ContainerFromItem(*kcVM))
                        {
                            container.as<Controls::ListViewItem>().Focus(FocusState::Programmatic);
                        }
                    }
                }
            });
        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            CommandNameTextBox().Focus(FocusState::Programmatic);
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

    void EditAction::ShortcutActionBox_SuggestionChosen(const AutoSuggestBox& sender, const AutoSuggestBoxSuggestionChosenEventArgs& args)
    {
        if (const auto selectedAction = args.SelectedItem().try_as<winrt::hstring>())
        {
            sender.Text(*selectedAction);
        }
    }

    void EditAction::ShortcutActionBox_QuerySubmitted(const AutoSuggestBox& sender, const AutoSuggestBoxQuerySubmittedEventArgs& args)
    {
        const auto submittedText = args.QueryText();

        // Validate that this is a valid shortcut action
        bool isValid = false;
        for (const auto& action : _ViewModel.AvailableShortcutActions())
        {
            if (action == submittedText)
            {
                isValid = true;
                break;
            }
        }

        if (isValid)
        {
            _ViewModel.ProposedShortcutActionName(winrt::box_value(submittedText));
            _lastValidAction = submittedText;
        }
        else
        {
            // Revert to the last valid action
            sender.Text(_lastValidAction);
        }
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
