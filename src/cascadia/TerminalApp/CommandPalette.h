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
        void ToggleVisibility();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        TYPED_EVENT(Closed, TerminalApp::CommandPalette, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _allActions{ nullptr };
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _keyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _selectNextItem(const bool moveDown);

        void _updateFilteredActions();
        static bool _filterMatchesName(const winrt::hstring& searchText, const winrt::hstring& name);
        static int _getWeight(const winrt::hstring& searchText, const winrt::hstring& name);
        void _close();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
