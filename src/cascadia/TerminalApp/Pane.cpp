// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

Pane::Pane(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control) :
    _control{ control },
    _focused{ false },
    _profile{ profile },
    _splitState{ SplitState::None },
    _firstChild{ nullptr },
    _secondChild{ nullptr }
{
    // _MakeTabViewItem();
    _root = Controls::Grid{};
    _root.Children().Append(_control.GetControl());
}

Pane::~Pane()
{
}

// void Pane::_MakeTabViewItem()
// {
//     _tabViewItem = ::winrt::Microsoft::UI::Xaml::Controls::TabViewItem{};
//     const auto title = _control.Title();

//     _tabViewItem.Header(title);

//     _control.TitleChanged([=](auto newTitle){
//         _tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
//             _tabViewItem.Header(newTitle);
//         });
//     });
// }

winrt::Windows::UI::Xaml::UIElement Pane::GetRootElement()
{
    return _root;
}

winrt::Microsoft::Terminal::TerminalControl::TermControl Pane::GetFocusedTerminalControl()
{
    if (_IsLeaf())
    {
        return _control;
    }
    else
    {
        // TODO determine a way of tracking the actually focused terminal control
        return _firstChild->GetFocusedTerminalControl();
    }
}

bool Pane::IsFocused() const noexcept
{
    return _focused;
}

bool Pane::_IsLeaf() const noexcept
{
    return _splitState == SplitState::None;
}

void Pane::SetFocused(bool focused)
{
    _focused = focused;

    if (_focused)
    {
        _Focus();
    }
}

// GUID Pane::GetProfile() const noexcept
// {
//     return _profile;
// }

void Pane::_Focus()
{
    _focused = true;
    _control.GetControl().Focus(FocusState::Programmatic);
}

// // Method Description:
// // - Move the viewport of the terminal up or down a number of lines. Negative
// //      values of `delta` will move the view up, and positive values will move
// //      the viewport down.
// // Arguments:
// // - delta: a number of lines to move the viewport relative to the current viewport.
// // Return Value:
// // - <none>
// void Pane::Scroll(int delta)
// {
//     _control.GetControl().Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
//         const auto currentOffset = _control.GetScrollOffset();
//         _control.ScrollViewport(currentOffset + delta);
//     });
// }
