// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include "Pane.h"

class Tab
{
public:
    Tab(const GUID& profile, const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
    winrt::Windows::UI::Xaml::UIElement GetRootElement();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetFocusedTerminalControl();
    std::optional<GUID> GetFocusedProfile() const noexcept;

    bool IsFocused() const noexcept;
    void SetFocused(const bool focused);

    void Scroll(const int delta);
    void AddVerticalSplit(const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl& control);
    void AddHorizontalSplit(const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    void UpdateFocus();

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings, const GUID& profile);
    winrt::hstring GetFocusedTitle() const;
    void SetTabText(const winrt::hstring& text);

    DECLARE_EVENT(Closed, _closedHandlers, winrt::Microsoft::Terminal::TerminalControl::ConnectionClosedEventArgs);

private:
    std::shared_ptr<Pane> _rootPane{ nullptr };

    bool _focused{ false };
    winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem{ nullptr };

    void _MakeTabViewItem();
    void _Focus();
};
