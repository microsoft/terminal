// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "CommandPalette.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

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

        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> FilteredActions();

        void SetCommands(Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command> const& actions);
        void SetTabActions(Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command> const& tabs, const bool clearList);
        void SetKeyBindings(Microsoft::Terminal::TerminalControl::IKeyBindings bindings);

        void EnableCommandPaletteMode(Microsoft::Terminal::Settings::Model::CommandPaletteLaunchMode const launchMode);

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        void ScrollPageUp();
        void ScrollPageDown();
        void ScrollToTop();
        void ScrollToBottom();

        // Tab Switcher
        void EnableTabSwitcherMode(const bool searchMode, const uint32_t startIdx);
        void SetTabSwitchOrder(const Microsoft::Terminal::Settings::Model::TabSwitcherMode order);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, NoMatchesText, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, SearchBoxPlaceholderText, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, PrefixCharacter, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, ParentCommandName, _PropertyChangedHandlers);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _nestedActionStack{ nullptr };

        winrt::TerminalApp::ShortcutActionDispatch _dispatch;
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandsToFilter();

        bool _lastFilterTextWasEmpty{ true };

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _previewKeyDownHandler(Windows::Foundation::IInspectable const& sender,
                                    Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _keyDownHandler(Windows::Foundation::IInspectable const& sender,
                             Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _keyUpHandler(Windows::Foundation::IInspectable const& sender,
                           Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _selectedCommandChanged(Windows::Foundation::IInspectable const& sender,
                                     Windows::UI::Xaml::RoutedEventArgs const& args);

        void _updateUIForStackChange();

        void _rootPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _backdropPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _listItemClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::ItemClickEventArgs const& e);

        void _moveBackButtonClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const&);

        void _updateFilteredActions();

        void _populateFilteredActions(Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> const& vectorToPopulate,
                                      Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command> const& actions);

        std::vector<winrt::TerminalApp::FilteredCommand> _collectFilteredActions();

        static int _getWeight(const winrt::hstring& searchText, const winrt::hstring& name);
        void _close();

        CommandPaletteMode _currentMode;
        void _switchToMode(CommandPaletteMode mode);

        std::wstring _getTrimmedInput();
        void _evaluatePrefix();

        Microsoft::Terminal::TerminalControl::IKeyBindings _bindings;

        // Tab Switcher
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _tabActions{ nullptr };
        uint32_t _switcherStartIdx;
        void _anchorKeyUpHandler();

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(winrt::TerminalApp::FilteredCommand const& command);
        void _dispatchCommandline(winrt::TerminalApp::FilteredCommand const& command);
        std::optional<winrt::TerminalApp::FilteredCommand> _buildCommandLineCommand(std::wstring const& commandLine);

        void _dismissPalette();

        void _scrollToIndex(uint32_t index);
        uint32_t _getNumVisibleItems();

        static constexpr int CommandLineHistoryLength = 10;
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandLineHistory{ nullptr };

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
