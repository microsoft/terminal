// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FilteredCommand.h"
#include "SuggestionsControl.g.h"
#include "AppCommandlineArgs.h"

#include <til/hash.h>

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

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        void SelectNextItem(const bool moveDown);

        void ScrollPageUp();
        void ScrollPageDown();
        void ScrollToTop();
        void ScrollToBottom();

        Windows::UI::Xaml::FrameworkElement SelectedItem();

        TerminalApp::SuggestionsMode Mode() const;
        void Mode(TerminalApp::SuggestionsMode mode);

        void Open(TerminalApp::SuggestionsMode mode,
                  const Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::Command>& commands,
                  winrt::hstring filterText,
                  Windows::Foundation::Point anchor,
                  Windows::Foundation::Size space,
                  float characterHeight);

        til::typed_event<winrt::TerminalApp::SuggestionsControl, Microsoft::Terminal::Settings::Model::Command> DispatchCommandRequested;
        til::typed_event<Windows::Foundation::IInspectable, Microsoft::Terminal::Settings::Model::Command> PreviewAction;

        // Walk-tier inline snippet-parameter fill: raised on every keystroke /
        // tabstop change inside fill mode. Empty IVector clears the preview.
        til::typed_event<Windows::Foundation::IInspectable,
                         Windows::Foundation::Collections::IVector<Microsoft::Terminal::Control::PreviewInputSpan>>
            PreviewInputSpansRequested;

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, NoMatchesText, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, SearchBoxPlaceholderText, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ControlName, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParentCommandName, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, ParsedCommandLineText, PropertyChanged.raise);

    private:
        struct winrt_object_hash
        {
            size_t operator()(const auto& value) const noexcept
            {
                return til::hash(winrt::get_abi(value));
            }
        };
        friend struct SuggestionsControlT<SuggestionsControl>; // for Xaml to bind events

        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _allCommands{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _currentNestedCommands{ nullptr };
        Windows::Foundation::Collections::IObservableVector<winrt::TerminalApp::FilteredCommand> _filteredActions{ nullptr };
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _nestedActionStack{ nullptr };

        // Parameter-filling state. When set, the control is in `Filling[i]` mode (per
        // doc/specs/#1595 - Suggestions UI/Snippet Parameters.md). When empty, the
        // control is in normal `Browsing` mode. Mirrors the `_currentNestedCommands` /
        // `_nestedActionStack` "I am in a substate" pattern.
        struct ParameterFillingState
        {
            winrt::TerminalApp::FilteredCommand snippet{ nullptr };
            winrt::Microsoft::Terminal::Settings::Model::SendInputArgs args{ nullptr };
            uint32_t currentIndex{ 0 };
            std::vector<std::wstring> filledValues; // one slot per Parameter, in order
        };
        std::optional<ParameterFillingState> _paramFilling;

        TerminalApp::SuggestionsMode _mode{ TerminalApp::SuggestionsMode::Palette };
        TerminalApp::SuggestionsDirection _direction{ TerminalApp::SuggestionsDirection::TopDown };

        bool _lastFilterTextWasEmpty{ true };
        Windows::Foundation::Point _anchor;
        Windows::Foundation::Size _space;

        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };

        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;

        winrt::TerminalApp::PaletteItemTemplateSelector _itemTemplateSelector{ nullptr };
        std::unordered_map<Windows::UI::Xaml::DataTemplate, std::unordered_set<Windows::UI::Xaml::Controls::Primitives::SelectorItem, winrt_object_hash>, winrt_object_hash> _listViewItemsCache;
        Windows::UI::Xaml::DataTemplate _listItemTemplate;

        void _switchToMode();
        void _setDirection(TerminalApp::SuggestionsDirection direction);

        void _scrollToIndex(uint32_t index);

        void _updateUIForStackChange();
        void _updateFilteredActions();

        void _dispatchCommand(const winrt::TerminalApp::FilteredCommand& command);
        void _close();
        void _dismissPalette();

        void _recalculateTopMargin();

        void _filterTextChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _previewKeyDownHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _keyUpHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _characterReceivedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs& args);

        void _rootPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _lostFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _backdropPointerPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _listItemClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::ItemClickEventArgs& e);
        void _listItemSelectionChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& e);
        void _selectedCommandChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _openTooltip(Microsoft::Terminal::Settings::Model::Command cmd);

        void _moveBackButtonClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs&);
        void _updateCurrentNestedCommands(const winrt::Microsoft::Terminal::Settings::Model::Command& parentCommand);

        void _choosingItemContainer(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args);
        void _containerContentChanging(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args);

        std::vector<winrt::TerminalApp::FilteredCommand> _collectFilteredActions();
        Windows::Foundation::Collections::IVector<winrt::TerminalApp::FilteredCommand> _commandsToFilter();
        std::wstring _getTrimmedInput();
        uint32_t _getNumVisibleItems();

        // Parameter-filling state-machine helpers (see ParameterFillingState above and
        // doc/specs/#1595 - Suggestions UI/Snippet Parameters.md). These manage the
        // transition Browsing -> Filling[0] -> ... -> Filling[last] -> dispatch.
        void _enterParameterFilling(const winrt::TerminalApp::FilteredCommand& filteredCommand,
                                    const winrt::Microsoft::Terminal::Settings::Model::SendInputArgs& args);
        void _exitParameterFilling();
        void _advanceParameterSlot();
        void _retreatParameterSlot();
        void _updateUIForParameterSlot();
        void _updatePreviewSpans();
        void _clearPreviewSpans();
        void _dispatchResolvedSnippet();
        Windows::Foundation::Collections::IMap<winrt::hstring, winrt::hstring>
            _buildParameterMap();
        winrt::Microsoft::Terminal::Settings::Model::Command
            _buildResolvedCommand();

        friend class TerminalAppLocalTests::TabTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(SuggestionsControl);
}
