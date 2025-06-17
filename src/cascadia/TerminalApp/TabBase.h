// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TabBase.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct TabBase : TabBaseT<TabBase>
    {
    public:
        virtual void Focus(winrt::Windows::UI::Xaml::FocusState focusState) = 0;

        virtual void Shutdown();
        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        void UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);
        virtual std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions(BuildStartupKind kind) const = 0;

        virtual std::optional<winrt::Windows::UI::Color> GetTabColor();
        void ThemeColor(const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& focused,
                        const winrt::Microsoft::Terminal::Settings::Model::ThemeColor& unfocused,
                        const til::color& tabRowColor);

        Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility CloseButtonVisibility();
        void CloseButtonVisibility(Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility visible);

        til::event<winrt::delegate<void()>> RequestFocusActiveControl;

        til::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> Closed;
        til::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> CloseRequested;
        til::property_changed_event PropertyChanged;

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewIndex, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewNumTabs, 0);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Icon, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(bool, ReadOnly, PropertyChanged.raise, false);
        WINRT_PROPERTY(winrt::Microsoft::UI::Xaml::Controls::TabViewItem, TabViewItem, nullptr);

        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::FrameworkElement, Content, PropertyChanged.raise, nullptr);

    protected:
        winrt::Windows::UI::Xaml::FocusState _focusState{ winrt::Windows::UI::Xaml::FocusState::Unfocused };
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeOtherTabsMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeTabsAfterMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _moveToNewWindowMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _moveRightMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _moveLeftMenuItem{};
        winrt::TerminalApp::ShortcutActionDispatch _dispatch;
        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };
        winrt::hstring _keyChord{};

        winrt::Microsoft::Terminal::Settings::Model::ThemeColor _themeColor{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::ThemeColor _unfocusedThemeColor{ nullptr };
        til::color _tabRowColor;

        Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility _closeButtonVisibility{ Microsoft::Terminal::Settings::Model::TabCloseButtonVisibility::Always };

        virtual void _CreateContextMenu();
        virtual winrt::hstring _CreateToolTipTitle();

        virtual void _MakeTabViewItem();

        void _AppendMoveMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout);
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem _AppendCloseMenuItems(winrt::Windows::UI::Xaml::Controls::MenuFlyout flyout);
        void _EnableMenuItems();
        void _UpdateSwitchToTabKeyChord();
        void _UpdateToolTip();

        void _RecalculateAndApplyTabColor();
        void _ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& color);
        void _ClearTabBackgroundColor();
        void _RefreshVisualState();
        virtual winrt::Windows::UI::Xaml::Media::Brush _BackgroundBrush() = 0;

        bool _focused() const noexcept;
        void _updateIsClosable();

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
