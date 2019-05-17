// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - Pane.h
//
// Abstract:
// - Panes are an abstraction by which the terminal can dislay multiple terminal
//   instances simultaneously in a single terminal window. While tabs allow for
//   a single terminal window to have many terminal sessions running
//   simultaneously within a single window, only one tab can be visible at a
//   time. Panes, on the other hand, allow a user to have many different
//   terminal sessions visible to the user within the context of a single window
//   at the same time. This can enable greater productivity from the user, as
//   they can see the output of one terminal window while working in another.
// - See doc/cascadia/Panes.md for a detailed description.
//
// Author:
// - Mike Griese (zadjii-msft) 16-May-2019


#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

class Pane : public std::enable_shared_from_this<Pane>
{

public:

    enum class SplitState : int
    {
        None = 0,
        Vertical = 1,
        Horizontal = 2
    };

    Pane(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control, const bool lastFocused = false);
    ~Pane() = default;

    std::shared_ptr<Pane> GetLastFocusedPane();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetLastFocusedTerminalControl();
    std::optional<GUID> GetLastFocusedProfile();

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    bool WasLastFocused() const noexcept;
    void UpdateFocus();

    void CheckUpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings, const GUID& profile);

    void SplitHorizontal(const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);
    void SplitVertical(const GUID& profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);

    DECLARE_EVENT(Closed, _closedHandlers, winrt::Microsoft::Terminal::TerminalControl::ConnectionClosedEventArgs);

private:
    winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Grid _separatorRoot{ nullptr };
    winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };

    std::shared_ptr<Pane> _firstChild;
    std::shared_ptr<Pane> _secondChild;
    SplitState _splitState;

    bool _lastFocused;
    std::optional<GUID> _profile;
    winrt::event_token _connectionClosedToken;
    winrt::event_token _firstClosedToken;
    winrt::event_token _secondClosedToken;

    bool _IsLeaf() const noexcept;
    bool _HasFocusedChild() const noexcept;
    void _SetupChildCloseHandlers();
    void _AddControlToRoot(winrt::Microsoft::Terminal::TerminalControl::TermControl control);

    void _CloseChild(const bool closeFirst);

    void _FocusFirstChild();
};
