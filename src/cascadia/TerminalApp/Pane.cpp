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
    _root = Controls::Grid{};
    _root.Children().Append(_control.GetControl());
}

Pane::~Pane()
{
}

winrt::Windows::UI::Xaml::Controls::Grid Pane::GetRootElement()
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

void Pane::_Focus()
{
    // _focused = true;
    // _control.GetControl().Focus(FocusState::Programmatic);
}

void Pane::SplitVertical(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    if (!_IsLeaf())
    {
        return;
    }
    _splitState = SplitState::Horizontal;

    // Create two columns in this grid.
    auto separatorColDef = Controls::ColumnDefinition();
    separatorColDef.Width(GridLengthHelper::Auto());

    _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});
    _root.ColumnDefinitions().Append(separatorColDef);
    _root.ColumnDefinitions().Append(Controls::ColumnDefinition{});

    _root.Children().Clear();

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control);

    // add the first pane to row 0
    _root.Children().Append(_firstChild->GetRootElement());
    Controls::Grid::SetColumn(_firstChild->GetRootElement(), 0);

    _separatorRoot = Controls::Grid{};
    _separatorRoot.Width(8);
    // NaN is the special value XAML uses for "Auto" sizing.
    _separatorRoot.Height(NAN);
    _root.Children().Append(_separatorRoot);
    Controls::Grid::SetColumn(_separatorRoot, 1);

    // add the second pane to row 1
    _root.Children().Append(_secondChild->GetRootElement());
    Controls::Grid::SetColumn(_secondChild->GetRootElement(), 2);
}

void Pane::SplitHorizontal(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    if (!_IsLeaf())
    {
        return;
    }
    _splitState = SplitState::Vertical;

    // Create two rows in this grid.

    auto separatorRowDef = Controls::RowDefinition();
    separatorRowDef.Height(GridLengthHelper::Auto());

    _root.RowDefinitions().Append(Controls::RowDefinition{});
    _root.RowDefinitions().Append(separatorRowDef);
    _root.RowDefinitions().Append(Controls::RowDefinition{});

    _root.Children().Clear();

    // Create two new Panes
    //   Move our control, guid into the first one.
    //   Move the new guid, control into the second.
    _firstChild = std::make_shared<Pane>(_profile.value(), _control);
    _profile = std::nullopt;
    _control = { nullptr };
    _secondChild = std::make_shared<Pane>(profile, control);


    // add the first pane to row 0
    _root.Children().Append(_firstChild->GetRootElement());
    Controls::Grid::SetRow(_firstChild->GetRootElement(), 0);

    _separatorRoot = Controls::Grid{};
    _separatorRoot.Height(8);
    // NaN is the special value XAML uses for "Auto" sizing.
    _separatorRoot.Width(NAN);
    _root.Children().Append(_separatorRoot);
    Controls::Grid::SetRow(_separatorRoot, 1);

    // add the second pane to row 1
    _root.Children().Append(_secondChild->GetRootElement());
    Controls::Grid::SetRow(_secondChild->GetRootElement(), 2);

}
