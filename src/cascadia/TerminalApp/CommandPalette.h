// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "CommandPalette.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    enum class PaletteMode : uint32_t
    {
        ActionMode = 0,
        CommandlineMode
    };

    struct CommandPalette : CommandPaletteT<CommandPalette>
    {
        CommandPalette();

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> FilteredActions();
        void SetActions(Windows::Foundation::Collections::IVector<TerminalApp::Command> const& actions);
        void ToggleVisibility();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(Closed, _closeHandlers, TerminalApp::CommandPalette, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _allActions{ nullptr };
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        PaletteMode _mode{ PaletteMode::ActionMode };

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _keyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _selectNextItem(const bool moveDown);

        void _checkMode();

        void _updateFilteredActions();
        static bool _filterMatchesName(winrt::hstring searchText, winrt::hstring name);
        void _close();

        void _dispatchAction();
        void _dispatchCommandline();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct CommandPalette : CommandPaletteT<CommandPalette, implementation::CommandPalette>
    {
    };
}
