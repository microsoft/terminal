// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CommandPalette.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct CommandPalette : CommandPaletteT<CommandPalette>
    {
        CommandPalette();

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> FilteredActions();
        void SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Command> const& actions);

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _allActions{ nullptr };
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _keyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _rootPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _backdropPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _listItemClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::ItemClickEventArgs const& e);

        void _selectNextItem(const bool moveDown);

        void _updateFilteredActions();
        std::vector<winrt::TerminalApp::Command> _collectFilteredActions();
        static int _getWeight(const winrt::hstring& searchText, const winrt::hstring& name);
        void _close();

        void _dispatchCommand(const TerminalApp::Command& command);

        void _dismissPalette();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
