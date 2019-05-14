// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Pane.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;

Pane::Pane(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control, const bool lastFocused) :
    _control{ control },
    _lastFocused{ lastFocused },
    _profile{ profile },
    _splitState{ SplitState::None },
    _firstChild{ nullptr },
    _secondChild{ nullptr },
    _connectionClosedToken{}
{
    _root = Controls::Grid{};
    _AddControlToRoot(_control);
}

Pane::~Pane()
{
}

void Pane::_AddControlToRoot(TermControl control)
{
    _root.Children().Append(control.GetControl());

    _connectionClosedToken = control.ConnectionClosed([=]() {
        if (control.CloseOnExit())
        {
            // Fire our ConnectionClosed event
            _closedHandlers();
        }
    });
}

Controls::Grid Pane::GetRootElement()
{
    return _root;
}

TermControl Pane::GetFocusedTerminalControl()
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

winrt::Microsoft::Terminal::TerminalControl::TermControl Pane::GetLastFocusedTerminalControl()
{
    if (_IsLeaf())
    {
        return _lastFocused ? _control : nullptr;
    }
    else
    {
        auto firstFocused = _firstChild->GetLastFocusedTerminalControl();
        if (firstFocused != nullptr)
        {
            return firstFocused;
        }
        auto secondFocused = _secondChild->GetLastFocusedTerminalControl();
        return secondFocused;
    }
}

std::optional<GUID> Pane::GetLastFocusedProfile() const noexcept
{
    if (_IsLeaf())
    {
        return _lastFocused ? _profile : std::nullopt;
    }
    else
    {
        auto firstFocused = _firstChild->GetLastFocusedProfile();
        if (firstFocused.has_value())
        {
            return firstFocused;
        }
        auto secondFocused = _secondChild->GetLastFocusedProfile();
        return secondFocused;
    }
}

bool Pane::WasLastFocused() const noexcept
{
    return _lastFocused;
}

bool Pane::_IsLeaf() const noexcept
{
    return _splitState == SplitState::None;
}

bool Pane::_HasFocusedChild() const noexcept
{
    const bool controlFocused = _control != nullptr &&
                                _control.GetControl().FocusState() != FocusState::Unfocused;
    const bool firstFocused = _firstChild != nullptr && _firstChild->_HasFocusedChild();
    const bool secondFocused = _secondChild != nullptr && _secondChild->_HasFocusedChild();

    return controlFocused || firstFocused || secondFocused;
}

void Pane::CheckFocus()
{
    if (_IsLeaf())
    {
        const bool controlFocused = _control != nullptr &&
                                    _control.GetControl().FocusState() != FocusState::Unfocused;

        _lastFocused = controlFocused;
    }
    else
    {
        _lastFocused = false;
        _firstChild->CheckFocus();
        _secondChild->CheckFocus();
    }
}

void Pane::CheckUpdateSettings(TerminalSettings settings, GUID profile)
{
    if (!_IsLeaf())
    {
        _firstChild->CheckUpdateSettings(settings, profile);
        _secondChild->CheckUpdateSettings(settings, profile);
    }
    else
    {
        if (profile == _profile)
        {
            _control.UpdateSettings(settings);
        }
    }
}

void Pane::_SetupChildCloseHandlers()
{

    _firstChild->Closed([this](){

        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
            _control = _secondChild->_control;
            _profile = _secondChild->_profile;
            _secondChild->_root.Children().Clear();
            _lastFocused = _firstChild->_lastFocused || _secondChild->_lastFocused;
            _root.Children().Clear();
            _root.ColumnDefinitions().Clear();
            _root.RowDefinitions().Clear();
            _separatorRoot = { nullptr };
            _AddControlToRoot(_control);
            if (_lastFocused)
            {
                _control.GetControl().Focus(FocusState::Programmatic);
            }
            _splitState = SplitState::None;
            _firstChild = nullptr;
            _secondChild = nullptr;
        });
    });

    _secondChild->Closed([this](){
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [=](){
            _control = _firstChild->_control;
            _profile = _firstChild->_profile;
            _firstChild->_root.Children().Clear();
            _lastFocused = _firstChild->_lastFocused || _secondChild->_lastFocused;
            _root.Children().Clear();
            _root.ColumnDefinitions().Clear();
            _root.RowDefinitions().Clear();
            _separatorRoot = { nullptr };
            _AddControlToRoot(_control);
            if (_lastFocused)
            {
                _control.GetControl().Focus(FocusState::Programmatic);
            }
            _splitState = SplitState::None;
            _firstChild = nullptr;
            _secondChild = nullptr;
        });
    });
}

void Pane::SplitVertical(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            _firstChild->SplitVertical(profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            _secondChild->SplitVertical(profile, control);
        }

        return;
    }
    // revoke our handler - the child will take care of the control now.
    _control.ConnectionClosed(_connectionClosedToken);

    _splitState = SplitState::Vertical;

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

    _SetupChildCloseHandlers();

    _lastFocused = false;
}

void Pane::SplitHorizontal(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control)
{
    if (!_IsLeaf())
    {
        if (_firstChild->_HasFocusedChild())
        {
            _firstChild->SplitHorizontal(profile, control);
        }
        else if (_secondChild->_HasFocusedChild())
        {
            _secondChild->SplitHorizontal(profile, control);
        }

        return;
    }
    _splitState = SplitState::Horizontal;

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

    _SetupChildCloseHandlers();

    _lastFocused = false;
}

DEFINE_EVENT(Pane, Closed, _closedHandlers, ConnectionClosedEventArgs);
