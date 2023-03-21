// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "SuggestionsControl.g.h"
#include "AppCommandlineArgs.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct SuggestionsControl : SuggestionsControlT<SuggestionsControl>
    {
        SuggestionsControl();

        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> FilteredActions();

        void SetCommands(const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command>& actions);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        void ScrollPageUp();
        void ScrollPageDown();
        void ScrollToTop();
        void ScrollToBottom();

        Windows::UI::Xaml::FrameworkElement SelectedItem();

        TerminalApp::SuggestionsMode Mode() const;
        void Mode(TerminalApp::SuggestionsMode mode);
        void Anchor(Windows::Foundation::Point anchor, Windows::Foundation::Size space, float characterHeight);

        winrt::hstring FilterText();
        void FilterText(winrt::hstring mode);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, NoMatchesText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, SearchBoxPlaceholderText, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParentCommandName, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParsedCommandLineText, _PropertyChangedHandlers);

        TYPED_EVENT(SwitchToTabRequested, winrt::TerminalApp::SuggestionsControl, winrt::TerminalApp::TabBase);
        TYPED_EVENT(CommandLineExecutionRequested, winrt::TerminalApp::SuggestionsControl, winrt::hstring);
        TYPED_EVENT(DispatchCommandRequested, winrt::TerminalApp::SuggestionsControl, Microsoft::Terminal::Settings::Model::Command);
        TYPED_EVENT(PreviewAction, Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command);

    private:
        friend struct SuggestionsControlT<SuggestionsControl>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _nestedActionStack{ nullptr };

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandsToFilter();

        TerminalApp::SuggestionsMode _mode{ TerminalApp::SuggestionsMode::Palette };
        TerminalApp::SuggestionsDirection _direction{ TerminalApp::SuggestionsDirection::TopDown };

        bool _lastFilterTextWasEmpty{ true };
        Windows::Foundation::Point _anchor;
        Windows::Foundation::Size _space;

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

        void _switchToMode();

        void _setDirection(TerminalApp::SuggestionsDirection direction);

        std::wstring _getTrimmedInput();

        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        void _dispatchCommand(const winrt::TerminalApp::FilteredCommand& command);
        static std::optional<winrt::TerminalApp::FilteredCommand> _buildCommandLineCommand(const winrt::hstring& commandLine);

        void _dismissPalette();

        void _scrollToIndex(uint32_t index);
        uint32_t _getNumVisibleItems();

        void _choosingItemContainer(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args);
        void _containerContentChanging(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args);
        winrt::TerminalApp::PaletteItemTemplateSelector _itemTemplateSelector{ nullptr };
        std::unordered_map<Windows::UI::Xaml::DataTemplate, std::unordered_set<Windows::UI::Xaml::Controls::Primitives::SelectorItem>> _listViewItemsCache;
        Windows::UI::Xaml::DataTemplate _listItemTemplate;

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SuggestionsControl);
}
