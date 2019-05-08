// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Tab.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

Tab::Tab(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control) :
    _focused{ false },
    _tabViewItem{ nullptr },
    _rootPane{ nullptr }
{
    _rootPane = std::make_shared<Pane>(profile, control);
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
    // const auto title = _control.Title();

    // _tabViewItem.Header(title);

    // _control.TitleChanged([=](auto newTitle){
    //     _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
    //         _tabViewItem.Header(newTitle);
    //     });
    // });
}

winrt::Windows::UI::Xaml::UIElement Tab::GetRootElement()
{
    return _rootPane->GetRootElement();
}

winrt::Microsoft::Terminal::TerminalControl::TermControl Tab::GetFocusedTerminalControl()
{
    return _rootPane->GetFocusedTerminalControl();
    // return _control;
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

// TODO
// GUID Tab::GetProfile() const noexcept
// {
//     return _profile;
// }

void Tab::_Focus()
{
    _focused = true;
    // _control.GetControl().Focus(FocusState::Programmatic);
    _rootPane->SetFocused(true);
}

// TODO
// // Method Description:
// // - Move the viewport of the terminal up or down a number of lines. Negative
// //      values of `delta` will move the view up, and positive values will move
// //      the viewport down.
// // Arguments:
// // - delta: a number of lines to move the viewport relative to the current viewport.
// // Return Value:
// // - <none>
// void Tab::Scroll(int delta)
// {
//     _control.GetControl().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
//         const auto currentOffset = _control.GetScrollOffset();
//         _control.ScrollViewport(currentOffset + delta);
//     });
// }
