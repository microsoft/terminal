// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LeafPane.h"

#include "LeafPane.g.cpp"

using namespace winrt;
//using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::TerminalControl;

namespace winrt::TerminalApp::implementation
{
    LeafPane::LeafPane()
    {
        InitializeComponent();
    }

    LeafPane::LeafPane(const GUID& profile, const TermControl& control, const bool lastFocused) :
        _control{ control },
        _lastActive{ lastFocused },
        _profile{ profile }
    {
        InitializeComponent();
        //GridBorder().Child(_control);
        //_root.Children().Append(_border);
        //_border.Child(_control);

        //_connectionStateChangedToken = _control.ConnectionStateChanged({ this, &Pane::_ControlConnectionStateChangedHandler });
        //_warningBellToken = _control.WarningBell({ this, &Pane::_ControlWarningBellHandler });

        //// On the first Pane's creation, lookup resources we'll use to theme the
        //// Pane, including the brushed to use for the focused/unfocused border
        //// color.
        //if (s_focusedBorderBrush == nullptr || s_unfocusedBorderBrush == nullptr)
        //{
        //    _SetupResources();
        //}

        //// Use the unfocused border color as the pane background, so an actual color
        //// appears behind panes as we animate them sliding in.
        //_root.Background(s_unfocusedBorderBrush);

        //// Register an event with the control to have it inform us when it gains focus.
        //_gotFocusRevoker = control.GotFocus(winrt::auto_revoke, { this, &Pane::_ControlGotFocusHandler });

        //// When our border is tapped, make sure to transfer focus to our control.
        //// LOAD-BEARING: This will NOT work if the border's BorderBrush is set to
        //// Colors::Transparent! The border won't get Tapped events, and they'll fall
        //// through to something else.
        //_border.Tapped([this](auto&, auto& e) {
        //    _FocusFirstChild();
        //    e.Handled(true);
        //});
    }

    LeafPane* LeafPane::GetActivePane()
    {
        return _lastActive ? this : nullptr;
    }

    TermControl LeafPane::GetTerminalControl()
    {
        return _control;
    }

    GUID LeafPane::GetProfile()
    {
        return _profile;
    }

    Controls::Grid LeafPane::GetRootElement()
    {
        return Root();
    }

    bool LeafPane::WasLastFocused() const noexcept
    {
        return _lastActive;
    }

    void LeafPane::UpdateVisuals()
    {
    }

    void LeafPane::ClearActive()
    {
        _lastActive = false;
    }

    void LeafPane::SetActive(const bool active)
    {
        _lastActive = active;
    }

    void LeafPane::FocusPane(uint32_t /*id*/)
    {
    }
}
