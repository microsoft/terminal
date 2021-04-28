// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "CommandPalette.g.h"
#include "AppCommandlineArgs.h"
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
        void SetTabs(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase> const& tabs, Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase> const& mruTabs);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        void ScrollPageUp();
        void ScrollPageDown();
        void ScrollToTop();
        void ScrollToBottom();

        void EnableCommandPaletteMode(Microsoft::Terminal::Settings::Model::CommandPaletteLaunchMode const launchMode);
        void EnableTabSwitcherMode(const uint32_t startIdx, Microsoft::Terminal::Settings::Model::TabSwitcherMode tabSwitcherMode);
        void EnableTabSearchMode();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, NoMatchesText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, SearchBoxPlaceholderText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, PrefixCharacter, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParentCommandName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParsedCommandLineText, _PropertyChangedHandlers);

        TYPED_EVENT(SwitchToTabRequested, winrt::TerminalApp::CommandPalette, winrt::TerminalApp::TabBase);
        TYPED_EVENT(CommandLineExecutionRequested, winrt::TerminalApp::CommandPalette, winrt::hstring);
        TYPED_EVENT(DispatchCommandRequested, winrt::TerminalApp::CommandPalette, Microsoft::Terminal::Settings::Model::Command);
        TYPED_EVENT(PreviewAction, Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _nestedActionStack{ nullptr };

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandsToFilter();

        bool _lastFilterTextWasEmpty{ true };

        void _filterTextChanged(Windows::Foundation::IInspectable const& sender,
                                Windows::UI::Xaml::RoutedEventArgs const& args);
        void _previewKeyDownHandler(Windows::Foundation::IInspectable const& sender,
                                    Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _keyUpHandler(Windows::Foundation::IInspectable const& sender,
                           Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        void _selectedCommandChanged(Windows::Foundation::IInspectable const& sender,
                                     Windows::UI::Xaml::RoutedEventArgs const& args);

        void _updateUIForStackChange();

        void _rootPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _lostFocusHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        void _backdropPointerPressed(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _listItemClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::ItemClickEventArgs const& e);

        void _moveBackButtonClicked(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const&);

        void _updateFilteredActions();

        std::vector<winrt::TerminalApp::FilteredCommand> _collectFilteredActions();

        void _close();

        CommandPaletteMode _currentMode;
        void _switchToMode(CommandPaletteMode mode);

        std::wstring _getTrimmedInput();
        void _evaluatePrefix();

        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };

        // Tab Switcher
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _tabActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _mruTabActions{ nullptr };
        Microsoft::Terminal::Settings::Model::TabSwitcherMode _tabSwitcherMode;
        uint32_t _switcherStartIdx;

        void _bindTabs(Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase> const& source, Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> const& target);
        void _anchorKeyUpHandler();

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(winrt::TerminalApp::FilteredCommand const& command);
        void _dispatchCommandline(winrt::TerminalApp::FilteredCommand const& command);
        void _switchToTab(winrt::TerminalApp::FilteredCommand const& command);
        std::optional<winrt::TerminalApp::FilteredCommand> _buildCommandLineCommand(std::wstring const& commandLine);

        void _dismissPalette();

        void _scrollToIndex(uint32_t index);
        uint32_t _getNumVisibleItems();

        static constexpr int CommandLineHistoryLength = 10;
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandLineHistory{ nullptr };
        ::TerminalApp::AppCommandlineArgs _appArgs;

        void _choosingItemContainer(Windows::UI::Xaml::Controls::ListViewBase const& sender, Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs const& args);
        void _containerContentChanging(Windows::UI::Xaml::Controls::ListViewBase const& sender, Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs const& args);
        winrt::TerminalApp::PaletteItemTemplateSelector _itemTemplateSelector{ nullptr };
        std::unordered_map<Windows::UI::Xaml::DataTemplate, std::unordered_set<Windows::UI::Xaml::Controls::Primitives::SelectorItem>> _listViewItemsCache;
        Windows::UI::Xaml::DataTemplate _listItemTemplate;

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
