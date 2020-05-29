// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandPalette.h"

#include "CommandPalette.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;

namespace winrt::TerminalApp::implementation
{
    CommandPalette::CommandPalette()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Command>();
        _allActions = winrt::single_threaded_vector<winrt::TerminalApp::Command>();
    }

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
            _close();
        }
    }

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

    void CommandPalette::_keyDownHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                         Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.OriginalKey();
        if (key == VirtualKey::Up)
        {
            _selectNextItem(false);
            e.Handled(true);
        }
        else if (key == VirtualKey::Down)
        {
            _selectNextItem(true);
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter)
        {
            auto selectedItem = _FilteredActionsView().SelectedItem();
            if (selectedItem)
            {
                auto data = selectedItem.try_as<Command>();
                if (data)
                {
                    auto actionAndArgs = data.Action();
                    _dispatch.DoAction(actionAndArgs);
                    _close();
                }
            }
            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
            _close();
        }
    }

    void CommandPalette::_filterTextChanged(Windows::Foundation::IInspectable const& /*sender*/,
                                            Windows::UI::Xaml::RoutedEventArgs const& /*args*/)
    {
        _updateFilteredActions();
        _FilteredActionsView().SelectedIndex(0);
    }

    Windows::Foundation::Collections::IObservableVector<Command> CommandPalette::FilteredActions()
    {
        return _filteredActions;
    }

    void CommandPalette::SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Command> const& actions)
    {
        _allActions = actions;
        _updateFilteredActions();
    }

    void CommandPalette::_updateFilteredActions()
    {
        _filteredActions.Clear();
        auto searchText = _SearchBox().Text();
        const bool addAll = searchText.empty();

        for (auto action : _allActions)
        {
            if (addAll || CommandPalette::_filterMatchesName(searchText, action.Name()))
            {
                _filteredActions.Append(action);
            }
        }
    }

    bool CommandPalette::_filterMatchesName(winrt::hstring searchText, winrt::hstring name)
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

    void CommandPalette::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }

    void CommandPalette::_close()
    {
        Visibility(Visibility::Collapsed);
        // TODO: Do we want to clear the text box each time we close the dialog? Or leave it?
        // I think if we decide to leave it, we should auto-select all the text
        // in it, so a user can start typing right away, or continue with the
        // current selection.

        // _SearchBox().Text(L"");
        _closeHandlers(*this, RoutedEventArgs{});
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(CommandPalette, Closed, _closeHandlers, TerminalApp::CommandPalette, winrt::Windows::UI::Xaml::RoutedEventArgs);
}
