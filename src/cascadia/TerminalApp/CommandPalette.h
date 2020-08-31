// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CommandPalette.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    enum class CommandPaletteMode
    {
        ActionMode = 0,
        TabSearchMode,
        TabSwitchMode,
        CommandlineMode
    };

    struct CommandPalette : CommandPaletteT<CommandPalette>
    {
        CommandPalette();

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> FilteredActions();

        void SetCommands(Windows::Foundation::Collections::IVector<TerminalApp::Command> const& actions);
        void SetKeyBindings(Microsoft::Terminal::TerminalControl::IKeyBindings bindings);

        void EnableCommandPaletteMode();

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        // Tab Switcher
        void EnableTabSwitcherMode(const bool searchMode, const uint32_t startIdx);
        void OnTabsChanged(const Windows::Foundation::IInspectable& s, const Windows::Foundation::Collections::IVectorChangedEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, NoMatchesText, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, SearchBoxText, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, ParentCommandName, _PropertyChangedHandlers);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<TerminalApp::Command> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<TerminalApp::Command> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _nestedActionStack{ nullptr };

        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        Windows::Foundation::Collections::IVector<TerminalApp::Command> _commandsToFilter();

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _previewKeyDownHandler(Windows::Foundation::IInspectable const& sender,
                                    Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _keyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _keyUpHandler(Windows::Foundation::IInspectable const& sender,
                           Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _updateUIForStackChange();

        void _rootPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _backdropPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _listItemClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::ItemClickEventArgs const& e);

        void _updateFilteredActions();

        std::vector<winrt::TerminalApp::Command> _collectFilteredActions();

        static int _getWeight(const winrt::hstring& searchText, const winrt::hstring& name);
        void _close();

        CommandPaletteMode _currentMode;
        void _switchToMode(CommandPaletteMode mode);

        void _evaluatePrefix();
        std::wstring _getPostPrefixInput();

        Microsoft::Terminal::TerminalControl::IKeyBindings _bindings;

        // Tab Switcher
        void GenerateCommandForTab(const uint32_t idx, bool inserted, winrt::TerminalApp::Tab& tab);
        void UpdateTabIndices(const uint32_t startIdx);
        Windows::Foundation::Collections::IVector<TerminalApp::Command> _allTabActions{ nullptr };
        uint32_t _switcherStartIdx;
        void _anchorKeyUpHandler();

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(const TerminalApp::Command& command);
        void _dispatchCommandline();
        void _dismissPalette();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
