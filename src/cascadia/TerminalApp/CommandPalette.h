// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "CommandPalette.g.h"
#include "AppCommandlineArgs.h"

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

        WFC::IObservableVector<MTApp::FilteredCommand> FilteredActions();

        void SetCommands(const WFC::IVector<MTSM::Command>& actions);
        void SetTabs(const WFC::IObservableVector<MTApp::TabBase>& tabs, const WFC::IObservableVector<MTApp::TabBase>& mruTabs);
        void SetActionMap(const MTSM::IActionMapView& actionMap);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        void ScrollPageUp();
        void ScrollPageDown();
        void ScrollToTop();
        void ScrollToBottom();

        void EnableCommandPaletteMode(MTSM::CommandPaletteLaunchMode const launchMode);
        void EnableTabSwitcherMode(const uint32_t startIdx, Microsoft::Terminal::Settings::Model::TabSwitcherMode tabSwitcherMode);
        void EnableTabSearchMode();

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, NoMatchesText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, SearchBoxPlaceholderText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, PrefixCharacter, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParentCommandName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParsedCommandLineText, _PropertyChangedHandlers);

        TYPED_EVENT(SwitchToTabRequested, MTApp::CommandPalette, MTApp::TabBase);
        TYPED_EVENT(CommandLineExecutionRequested, MTApp::CommandPalette, winrt::hstring);
        TYPED_EVENT(DispatchCommandRequested, MTApp::CommandPalette, MTSM::Command);
        TYPED_EVENT(PreviewAction, WF::IInspectable, MTSM::Command);

    private:
        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        WFC::IVector<MTApp::FilteredCommand> _allCommands{ nullptr };
        WFC::IVector<MTApp::FilteredCommand> _currentNestedCommands{ nullptr };
        WFC::IObservableVector<MTApp::FilteredCommand> _filteredActions{ nullptr };
        WFC::IVector<MTApp::FilteredCommand> _nestedActionStack{ nullptr };

        WFC::IVector<MTApp::FilteredCommand> _commandsToFilter();

        bool _lastFilterTextWasEmpty{ true };

        void _filterTextChanged(const WF::IInspectable& sender,
                                const WUX::RoutedEventArgs& args);
        void _previewKeyDownHandler(const WF::IInspectable& sender,
                                    const WUX::Input::KeyRoutedEventArgs& e);

        void _keyUpHandler(const WF::IInspectable& sender,
                           const WUX::Input::KeyRoutedEventArgs& e);

        void _selectedCommandChanged(const WF::IInspectable& sender,
                                     const WUX::RoutedEventArgs& args);

        void _updateUIForStackChange();

        void _rootPointerPressed(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);

        void _lostFocusHandler(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);

        void _backdropPointerPressed(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);

        void _listItemClicked(const WF::IInspectable& sender, const WUXC::ItemClickEventArgs& e);

        void _listItemSelectionChanged(const WF::IInspectable& sender, const WUXC::SelectionChangedEventArgs& e);

        void _moveBackButtonClicked(const WF::IInspectable& sender, const WUX::RoutedEventArgs&);

        void _updateFilteredActions();

        void _updateCurrentNestedCommands(const MTSM::Command& parentCommand);

        std::vector<MTApp::FilteredCommand> _collectFilteredActions();

        void _close();

        CommandPaletteMode _currentMode;
        void _switchToMode(CommandPaletteMode mode);

        std::wstring _getTrimmedInput();
        void _evaluatePrefix();

        MTSM::IActionMapView _actionMap{ nullptr };

        // Tab Switcher
        WFC::IVector<MTApp::FilteredCommand> _tabActions{ nullptr };
        WFC::IVector<MTApp::FilteredCommand> _mruTabActions{ nullptr };
        Microsoft::Terminal::Settings::Model::TabSwitcherMode _tabSwitcherMode;
        uint32_t _switcherStartIdx;

        void _bindTabs(const WFC::IObservableVector<MTApp::TabBase>& source, const WFC::IVector<MTApp::FilteredCommand>& target);
        void _anchorKeyUpHandler();

        WUXC::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(const MTApp::FilteredCommand& command);
        void _dispatchCommandline(const MTApp::FilteredCommand& command);
        void _switchToTab(const MTApp::FilteredCommand& command);
        static std::optional<MTApp::FilteredCommand> _buildCommandLineCommand(const winrt::hstring& commandLine);

        void _dismissPalette();

        void _scrollToIndex(uint32_t index);
        uint32_t _getNumVisibleItems();

        static constexpr uint32_t CommandLineHistoryLength = 20;
        static WFC::IVector<MTApp::FilteredCommand> _loadRecentCommands();
        static void _updateRecentCommands(const winrt::hstring& command);
        ::MTApp::AppCommandlineArgs _appArgs;

        void _choosingItemContainer(const WUXC::ListViewBase& sender, const WUXC::ChoosingItemContainerEventArgs& args);
        void _containerContentChanging(const WUXC::ListViewBase& sender, const WUXC::ContainerContentChangingEventArgs& args);
        MTApp::PaletteItemTemplateSelector _itemTemplateSelector{ nullptr };
        std::unordered_map<WUX::DataTemplate, std::unordered_set<WUXC::Primitives::SelectorItem>> _listViewItemsCache;
        WUX::DataTemplate _listItemTemplate;

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
