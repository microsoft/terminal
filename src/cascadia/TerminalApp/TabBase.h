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
        virtual void Focus(WUX::FocusState focusState) = 0;
        WUX::FocusState FocusState() const noexcept;

        virtual void Shutdown();
        void SetDispatch(const MTApp::ShortcutActionDispatch& dispatch);

        void UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs);
        void SetActionMap(const Microsoft::Terminal::Settings::Model::IActionMapView& actionMap);
        virtual std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> BuildStartupActions() const = 0;

        virtual std::optional<winrt::Windows::UI::Color> GetTabColor();
        void ThemeColor(const MTSM::ThemeColor& focused,
                        const MTSM::ThemeColor& unfocused,
                        const til::color& tabRowColor);

        WINRT_CALLBACK(RequestFocusActiveControl, winrt::delegate<void()>);

        WINRT_CALLBACK(Closed, WF::EventHandler<WF::IInspectable>);
        WINRT_CALLBACK(CloseRequested, WF::EventHandler<WF::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewIndex, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        WINRT_PROPERTY(uint32_t, TabViewNumTabs, 0);

        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Icon, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, ReadOnly, _PropertyChangedHandlers, false);
        WINRT_PROPERTY(MUXC::TabViewItem, TabViewItem, nullptr);

        WINRT_OBSERVABLE_PROPERTY(WUX::FrameworkElement, Content, _PropertyChangedHandlers, nullptr);

    protected:
        WUX::FocusState _focusState{ WUX::FocusState::Unfocused };
        WUXC::MenuFlyoutItem _closeOtherTabsMenuItem{};
        WUXC::MenuFlyoutItem _closeTabsAfterMenuItem{};
        MTApp::ShortcutActionDispatch _dispatch;
        Microsoft::Terminal::Settings::Model::IActionMapView _actionMap{ nullptr };
        winrt::hstring _keyChord{};

        MTSM::ThemeColor _themeColor{ nullptr };
        MTSM::ThemeColor _unfocusedThemeColor{ nullptr };
        til::color _tabRowColor;

        virtual void _CreateContextMenu();
        virtual winrt::hstring _CreateToolTipTitle();

        virtual void _MakeTabViewItem();

        void _AppendCloseMenuItems(WUXC::MenuFlyout flyout);
        void _EnableCloseMenuItems();
        void _CloseTabsAfter();
        void _CloseOtherTabs();
        winrt::fire_and_forget _UpdateSwitchToTabKeyChord();
        void _UpdateToolTip();

        void _RecalculateAndApplyTabColor();
        void _ApplyTabColorOnUIThread(const winrt::Windows::UI::Color& color);
        void _ClearTabBackgroundColor();
        void _RefreshVisualState();
        virtual WUXMedia::Brush _BackgroundBrush() = 0;

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
