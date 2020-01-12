// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ColorPickupFlyout.h"

class Tab : public std::enable_shared_from_this<Tab>
{
public:
    Tab(const GUID& profile, const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    // Called after construction to perform the necessary setup, which relies on weak_ptr
    void Initialize(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
    winrt::Windows::UI::Xaml::UIElement GetRootElement();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetActiveTerminalControl() const;
    std::optional<GUID> GetFocusedProfile() const noexcept;

    bool IsFocused() const noexcept;
    void SetFocused(const bool focused);

    winrt::fire_and_forget Scroll(const int delta);

    bool CanSplitPane(winrt::TerminalApp::SplitState splitType);
    void SplitPane(winrt::TerminalApp::SplitState splitType, const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    winrt::fire_and_forget UpdateIcon(const winrt::hstring iconPath);

    float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

    void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
    void ResizePane(const winrt::TerminalApp::Direction& direction);
    void NavigateFocus(const winrt::TerminalApp::Direction& direction);

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings, const GUID& profile);
    winrt::hstring GetActiveTitle() const;
    winrt::fire_and_forget SetTabText(const winrt::hstring text);

    void ClosePane();

    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
    DECLARE_EVENT(ActivePaneChanged, _ActivePaneChangedHandlers, winrt::delegate<>);

private:
    std::shared_ptr<Pane> _rootPane{ nullptr };
    std::shared_ptr<Pane> _activePane{ nullptr };
    winrt::hstring _lastIconPath{};
    winrt::TerminalApp::ColorPickupFlyout _tabColorPickup{};

    bool _focused{ false };
    winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };

    void _MakeTabViewItem();
    void _CreateContextMenu();
    void _Focus();
    void _SetTabColor(const winrt::Windows::UI::Color& color);
    void _ResetTabColor();
    void _RefreshVisualState();

    void _BindEventHandlers(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control) noexcept;

    void _AttachEventHandlersToControl(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
    void _AttachEventHandlersToPane(std::shared_ptr<Pane> pane);
};
