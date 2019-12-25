// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "LeafPane.h"
#include "ParentPane.h"

class Tab : public std::enable_shared_from_this<Tab>
{
public:
    Tab(const GUID& profile, const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
    ~Tab();

    // Called after construction to setup events with weak_ptr
    void BindEventHandlers() noexcept;

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
    DECLARE_EVENT(RootPaneChanged, _RootPaneChangedHandlers, winrt::delegate<>);

private:
    std::shared_ptr<Pane> _rootPane{ nullptr };
    winrt::hstring _lastIconPath{};

    bool _focused{ false };
    winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };

    winrt::event_token _rootPaneClosedToken{ 0 };
    winrt::event_token _rootPaneTypeChangedToken{ 0 };

    void _MakeTabViewItem();
    void _SetupRootPaneEventHandlers();
    void _RemoveAllRootPaneEventHandlers();
    void _Focus();

    void _AttachEventHandlersToControl(const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
    void _AttachEventHandlersToLeafPane(std::shared_ptr<LeafPane> pane);
};
