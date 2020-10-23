// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ColorPickupFlyout.h"
#include "Tab.g.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct Tab : public TabT<Tab>
    {
    public:
        Tab() = delete;
        Tab(const GUID& profile, const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

        // Called after construction to perform the necessary setup, which relies on weak_ptr
        void Initialize(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

        winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
        winrt::Microsoft::Terminal::TerminalControl::TermControl GetActiveTerminalControl() const;
        std::optional<GUID> GetFocusedProfile() const noexcept;

        bool IsFocused() const noexcept;
        void SetFocused(const bool focused);

        winrt::fire_and_forget Scroll(const int delta);

        bool CanSplitPane(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType);
        void SplitPane(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType, const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

        winrt::fire_and_forget UpdateIcon(const winrt::hstring iconPath);

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        winrt::Microsoft::Terminal::Settings::Model::SplitState PreCalculateAutoSplit(winrt::Windows::Foundation::Size rootSize) const;
        bool PreCalculateCanSplit(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType, winrt::Windows::Foundation::Size availableSpace) const;

        void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
        void ResizePane(const winrt::Microsoft::Terminal::Settings::Model::Direction& direction);
        void NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::Direction& direction);

        void UpdateSettings(const winrt::TerminalApp::TerminalSettings& settings, const GUID& profile);
        winrt::hstring GetActiveTitle() const;

        void Shutdown();
        void ClosePane();

        void SetTabText(winrt::hstring title);
        void ResetTabText();

        std::optional<winrt::Windows::UI::Color> GetTabColor();

        void SetRuntimeTabColor(const winrt::Windows::UI::Color& color);
        void ResetRuntimeTabColor();
        void ActivateColorPicker();

        void ToggleZoom();
        bool IsZoomed();
        void EnterZoom();
        void ExitZoom();

        int GetLeafPaneCount() const noexcept;

        void UpdateTabViewIndex(const uint32_t idx, const uint32_t numTabs);

        void SetDispatch(const winrt::TerminalApp::ShortcutActionDispatch& dispatch);

        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        DECLARE_EVENT(ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);
        DECLARE_EVENT(ColorSelected, _colorSelected, winrt::delegate<winrt::Windows::UI::Color>);
        DECLARE_EVENT(ColorCleared, _colorCleared, winrt::delegate<>);

        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::UI::Xaml::Controls::IconSource, IconSource, _PropertyChangedHandlers, nullptr);
        OBSERVABLE_GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::Command, SwitchToTabCommand, _PropertyChangedHandlers, nullptr);

        // The TabViewIndex is the index this Tab object resides in TerminalPage's _tabs vector.
        // This is needed since Tab is going to be managing its own SwitchToTab command.
        OBSERVABLE_GETSET_PROPERTY(uint32_t, TabViewIndex, _PropertyChangedHandlers, 0);
        // The TabViewNumTabs is the number of Tab objects in TerminalPage's _tabs vector.
        OBSERVABLE_GETSET_PROPERTY(uint32_t, TabViewNumTabs, _PropertyChangedHandlers, 0);

        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::UI::Xaml::UIElement, Content, _PropertyChangedHandlers, nullptr);

    private:
        std::shared_ptr<Pane> _rootPane{ nullptr };
        std::shared_ptr<Pane> _activePane{ nullptr };
        std::shared_ptr<Pane> _zoomedPane{ nullptr };
        winrt::hstring _lastIconPath{};
        winrt::TerminalApp::ColorPickupFlyout _tabColorPickup{};
        std::optional<winrt::Windows::UI::Color> _themeTabColor{};
        std::optional<winrt::Windows::UI::Color> _runtimeTabColor{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeOtherTabsMenuItem{};
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItem _closeTabsAfterMenuItem{};

        bool _focused{ false };
        winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };

        winrt::hstring _runtimeTabText{};
        bool _inRename{ false };
        winrt::Windows::UI::Xaml::Controls::TextBox::LayoutUpdated_revoker _tabRenameBoxLayoutUpdatedRevoker;

        winrt::TerminalApp::ShortcutActionDispatch _dispatch;

        void _MakeTabViewItem();
        void _Focus();

        void _CreateContextMenu();
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutSubItem _CreateCloseSubMenu();
        void _EnableCloseMenuItems();

        void _RefreshVisualState();

        void _BindEventHandlers(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control) noexcept;

        void _AttachEventHandlersToControl(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
        void _AttachEventHandlersToPane(std::shared_ptr<Pane> pane);

        void _UpdateActivePane(std::shared_ptr<Pane> pane);

        void _UpdateTabHeader();
        winrt::fire_and_forget _UpdateTitle();
        void _ConstructTabRenameBox(const winrt::hstring& tabText);

        void _RecalculateAndApplyTabColor();
        void _ApplyTabColor(const winrt::Windows::UI::Color& color);
        void _ClearTabBackgroundColor();

        void _MakeSwitchToTabCommand();

        void _CloseTabsAfter();
        void _CloseOtherTabs();

        friend class ::TerminalAppLocalTests::TabTests;
    };
}
