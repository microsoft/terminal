// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionList.h"

#include "ActionList.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace Windows::UI::Xaml;
using namespace Windows::System;

namespace winrt::TerminalApp::implementation
{
    ActionList::ActionList()
    {
        InitializeComponent();

        _filteredActions = winrt::single_threaded_observable_vector<winrt::TerminalApp::Action>();
        _allActions = winrt::single_threaded_vector<winrt::TerminalApp::Action>();

        _SearchBox().TextChanged({ this, &ActionList::_FilterTextChanged });
        _SearchBox().KeyDown({ this, &ActionList::_KeyDownHandler });
    }

    void ActionList::ToggleVisibility()
    {
        const bool isVisible = Visibility() == Visibility::Visible;
        Visibility(isVisible ? Visibility::Collapsed : Visibility::Visible);
        if (!isVisible)
        {
            // We just became visible
            _SearchBox().Focus(FocusState::Programmatic);
        }
        _FilteredActionsView().SelectedIndex(0);
        //_FilteredActionsView().Items().GetAt(0).
    }

    void ActionList::_KeyDownHandler(Windows::Foundation::IInspectable const& sender,
                                     Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        auto key = e.OriginalKey();
        if (key == VirtualKey::Up)
        {
            auto selected = _FilteredActionsView().SelectedIndex();
            selected = (selected - 1) % _FilteredActionsView().Items().Size();
            _FilteredActionsView().SelectedIndex(selected);
            e.Handled(true);
        }
        else if (key == VirtualKey::Down)
        {
            auto selected = _FilteredActionsView().SelectedIndex();
            selected = (selected + 1) % _FilteredActionsView().Items().Size();
            _FilteredActionsView().SelectedIndex(selected);
            e.Handled(true);
        }
        else if (key == VirtualKey::Enter)
        {
            auto selectedItem = _FilteredActionsView().SelectedItem();
            if (selectedItem)
            {
                auto data = selectedItem.try_as<Action>();
                if (data)
                {
                    auto command = data.Command();
                    _dispatch.DoAction(command);
                }
            }
            e.Handled(true);
        }
        else if (key == VirtualKey::Escape)
        {
        }
    }

    void ActionList::_FilterTextChanged(Windows::Foundation::IInspectable const& sender,
                                        Windows::UI::Xaml::RoutedEventArgs const& args)
    {
        _UpdateFilteredActions();
        _FilteredActionsView().SelectedIndex(0);
    }

    Windows::Foundation::Collections::IObservableVector<Action> ActionList::FilteredActions()
    {
        return _filteredActions;
    }

    void ActionList::SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Action> const& actions)
    {
        _allActions = actions;
        _UpdateFilteredActions();
    }

    void ActionList::_UpdateFilteredActions()
    {
        _filteredActions.Clear();
        auto searchText = _SearchBox().Text();
        const bool addAll = searchText.empty();

        for (auto action : _allActions)
        {
            if (addAll || ActionList::_FilterMatchesName(searchText, action.Name()))
            {
                _filteredActions.Append(action);
            }
        }
    }

    bool ActionList::_FilterMatchesName(winrt::hstring searchText, winrt::hstring name)
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
        // return searchText == name;
    }

    void ActionList::SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch)
    {
        _dispatch = dispatch;
    }
}
