// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "CommandPalette.g.h"
#include "AppCommandlineArgs.h"

#include <til/hash.h>

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

        void SetCommands(const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command>& actions);
        void SetTabs(const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase>& tabs, const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase>& mruTabs);
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
        struct winrt_object_hash
        {
            size_t operator()(const auto& value) const noexcept
            {
                return til::hash(winrt::get_abi(value));
            }
        };

        friend struct CommandPaletteT<CommandPalette>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _nestedActionStack{ nullptr };

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandsToFilter();

        bool _lastFilterTextWasEmpty{ true };

        void _filterTextChanged(const Windows::Foundation::IInspectable& sender,
                                const Windows::UI::Xaml::RoutedEventArgs& args);
        void _previewKeyDownHandler(const Windows::Foundation::IInspectable& sender,
                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        void _keyUpHandler(const Windows::Foundation::IInspectable& sender,
                           const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        void _selectedCommandChanged(const Windows::Foundation::IInspectable& sender,
                                     const Windows::UI::Xaml::RoutedEventArgs& args);

        void _updateUIForStackChange();

        void _rootPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _lostFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);

        void _backdropPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _listItemClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::ItemClickEventArgs& e);

        void _listItemSelectionChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e);

        void _moveBackButtonClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);

        void _updateFilteredActions();

        void _updateCurrentNestedCommands(const winrt::Microsoft::Terminal::Settings::Model::Command& parentCommand);

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

        void _bindTabs(const Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::TabBase>& source, const Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand>& target);
        void _anchorKeyUpHandler();

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(const winrt::TerminalApp::FilteredCommand& command);
        void _dispatchCommandline(const winrt::TerminalApp::FilteredCommand& command);
        void _switchToTab(const winrt::TerminalApp::FilteredCommand& command);
        static std::optional<winrt::TerminalApp::FilteredCommand> _buildCommandLineCommand(const winrt::hstring& commandLine);

        void _dismissPalette();

        void _scrollToIndex(uint32_t index);
        uint32_t _getNumVisibleItems();

        static constexpr uint32_t CommandLineHistoryLength = 20;
        static Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _loadRecentCommands();
        static void _updateRecentCommands(const winrt::hstring& command);
        ::TerminalApp::AppCommandlineArgs _appArgs;

        void _choosingItemContainer(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args);
        void _containerContentChanging(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args);
        winrt::TerminalApp::PaletteItemTemplateSelector _itemTemplateSelector{ nullptr };
        std::unordered_map<Windows::UI::Xaml::DataTemplate, std::unordered_set<Windows::UI::Xaml::Controls::Primitives::SelectorItem, winrt_object_hash>, winrt_object_hash> _listViewItemsCache;
        Windows::UI::Xaml::DataTemplate _listItemTemplate;

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(CommandPalette);
}
