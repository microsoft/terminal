// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include "Pane.h"

class Tab
{

public:
    Tab(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);
    ~Tab();

    winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
    winrt::Windows::UI::Xaml::UIElement GetRootElement();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetLastFocusedTerminalControl();
    std::optional<GUID> GetLastFocusedProfile() const noexcept;

    bool IsFocused();
    void SetFocused(const bool focused);

    void Scroll(const int delta);
    void SplitVertical(const GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);
    void SplitHorizontal(const GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);

    void CheckFocus();

    void CheckUpdateSettings(winrt::Microsoft::Terminal::Settings::TerminalSettings settings, GUID profile);
    winrt::hstring GetLastFocusedTitle();
    void SetTabText(const winrt::hstring& text);

    DECLARE_EVENT(Closed, _closedHandlers, winrt::Microsoft::Terminal::TerminalControl::ConnectionClosedEventArgs);

private:

    std::shared_ptr<Pane> _rootPane;

    bool _focused;
    winrt::Microsoft::UI::Xaml::Controls::TabViewItem _tabViewItem;

    void _MakeTabViewItem();
    void _Focus();
};
