// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.Terminal.TerminalControl.h>


class Pane
{

public:

    enum class SplitState : int
    {
        None = 0,
        Vertical = 1,
        Horizontal = 2
    };

    Pane(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control, const bool lastFocused = false);
    ~Pane();

    winrt::Microsoft::UI::Xaml::Controls::TabViewItem GetTabViewItem();
    // winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetFocusedTerminalControl();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetLastFocusedTerminalControl();
    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    bool WasLastFocused() const noexcept;
    void CheckFocus();

    // GUID GetProfile() const noexcept;

    void SplitHorizontal(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);
    void SplitVertical(GUID profile, winrt::Microsoft::Terminal::TerminalControl::TermControl control);


private:
    winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
    winrt::Windows::UI::Xaml::Controls::Grid _separatorRoot{ nullptr };
    winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };

    std::shared_ptr<Pane> _firstChild;
    std::shared_ptr<Pane> _secondChild;
    SplitState _splitState;

    bool _lastFocused;
    std::optional<GUID> _profile;

    bool _IsLeaf() const noexcept;
    bool _HasFocusedChild() const noexcept;
};
