// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;

Tab::Tab(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control) :
    _focused{ false },
    _tabViewItem{ nullptr },
    _rootPane{ nullptr }
{
    _rootPane = std::make_shared<Pane>(profile, control, true);
    _MakeTabViewItem();
}

Tab::~Tab()
{
    // When we're destructed, winrt will automatically decrement the refcount
    // of our terminalcontrol.
    // Assuming that refcount hits 0, it'll destruct it on it's own, including
    //      calling Close on the terminal and connection.
}

void Tab::_MakeTabViewItem()
{
    _tabViewItem = ::winrt::Microsoft::UI::Xaml::Controls::TabViewItem{};
    _tabViewItem.FontSize(12);
}

winrt::Windows::UI::Xaml::UIElement Tab::GetRootElement()
{
    return _rootPane->GetRootElement();
}

winrt::Microsoft::Terminal::TerminalControl::TermControl Tab::GetFocusedTerminalControl()
{
    return _rootPane->GetFocusedTerminalControl();
}

winrt::Microsoft::UI::Xaml::Controls::TabViewItem Tab::GetTabViewItem()
{
    return _tabViewItem;
}

bool Tab::IsFocused()
{
    return _focused;
}

void Tab::SetFocused(bool focused)
{
    _focused = focused;

    if (_focused)
    {
        _Focus();
    }
}

std::optional<GUID> Tab::GetLastFocusedProfile() const noexcept
{
    return _rootPane->GetLastFocusedProfile();
}

void Tab::CheckUpdateSettings(TerminalSettings settings, GUID profile)
{
    _rootPane->CheckUpdateSettings(settings, profile);
}

void Tab::_Focus()
{
    _focused = true;

    auto lastFocusedControl = _rootPane->GetLastFocusedTerminalControl();
    if (lastFocusedControl)
    {
        lastFocusedControl.GetControl().Focus(FocusState::Programmatic);
    }
}

void Tab::CheckFocus()
{
    _rootPane->CheckFocus();
}

winrt::hstring Tab::CheckTitleUpdate()
{
    auto lastFocusedControl = _rootPane->GetLastFocusedTerminalControl();
    return lastFocusedControl ? lastFocusedControl.Title() : L"";
}

void Tab::SetTabText(const winrt::hstring& text)
{
    _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
        _tabViewItem.Header(text);
    });
}

// Method Description:
// - Move the viewport of the terminal up or down a number of lines. Negative
//      values of `delta` will move the view up, and positive values will move
//      the viewport down.
// Arguments:
// - delta: a number of lines to move the viewport relative to the current viewport.
// Return Value:
// - <none>
void Tab::Scroll(int delta)
{
    GetFocusedTerminalControl().GetControl().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
        auto control = GetFocusedTerminalControl();
        const auto currentOffset = control.GetScrollOffset();
        control.ScrollViewport(currentOffset + delta);
    });
}

void Tab::SplitVertical(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    _rootPane->SplitVertical(profile, control);
}

void Tab::SplitHorizontal(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    _rootPane->SplitHorizontal(profile, control);
}
