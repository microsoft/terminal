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
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

enum class Borders : int
{
    None = 0x0,
    Top = 0x1,
    Bottom = 0x2,
    Left = 0x4,
    Right = 0x8
};
DEFINE_ENUM_FLAG_OPERATORS(Borders);

class Pane : public std::enable_shared_from_this<Pane>
{
public:
    enum class SplitState : int
    {
        None = 0,
        Vertical = 1,
        Horizontal = 2
    };

    Pane(const GUID& profile,
         const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
         const bool lastFocused = false);

    std::shared_ptr<Pane> GetActivePane();
    winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
    std::optional<GUID> GetFocusedProfile();

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    bool WasLastFocused() const noexcept;
    void UpdateVisuals();
    void ClearActive();
    void SetActive();

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings,
                        const GUID& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
    bool ResizePane(const winrt::TerminalApp::Direction& direction);
    bool NavigateFocus(const winrt::TerminalApp::Direction& direction);

    bool CanSplit(SplitState splitType);
    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Split(SplitState splitType,
                                                                  const GUID& profile,
                                                                  const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    void Close();

    DECLARE_EVENT(Closed, _closedHandlers, winrt::Microsoft::Terminal::TerminalControl::ConnectionClosedEventArgs);
    DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);

private:
    winrt::Windows::UI::Xaml::Controls::Grid _root{};
    winrt::Windows::UI::Xaml::Controls::Border _border{};
    winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

    std::shared_ptr<Pane> _firstChild{ nullptr };
    std::shared_ptr<Pane> _secondChild{ nullptr };
    SplitState _splitState{ SplitState::None };
    std::optional<float> _firstPercent{ std::nullopt };
    std::optional<float> _secondPercent{ std::nullopt };

    bool _lastActive{ false };
    std::optional<GUID> _profile{ std::nullopt };
    winrt::event_token _connectionClosedToken{ 0 };
    winrt::event_token _firstClosedToken{ 0 };
    winrt::event_token _secondClosedToken{ 0 };

    winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;

    std::shared_mutex _createCloseLock{};

    Borders _borders{ Borders::None };

    bool _IsLeaf() const noexcept;
    bool _HasFocusedChild() const noexcept;
    void _SetupChildCloseHandlers();

    bool _CanSplit(SplitState splitType);
    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> _Split(SplitState splitType,
                                                                   const GUID& profile,
                                                                   const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    void _CreateRowColDefinitions(const winrt::Windows::Foundation::Size& rootSize);
    void _CreateSplitContent();
    void _ApplySplitDefinitions();
    void _UpdateBorders();

    bool _Resize(const winrt::TerminalApp::Direction& direction);
    bool _NavigateFocus(const winrt::TerminalApp::Direction& direction);

    void _CloseChild(const bool closeFirst);

    void _FocusFirstChild();
    void _ControlClosedHandler();

    std::pair<float, float> _GetPaneSizes(const float& fullSize);

    winrt::Windows::Foundation::Size _GetMinSize() const;
    void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    // Function Description:
    // - Returns true if the given direction can be used with the given split
    //   type.
    // - This is used for pane resizing (which will need a pane separator
    //   that's perpendicular to the direction to be able to move the separator
    //   in that direction).
    // - Additionally, it will be used for moving focus between panes, which
    //   again happens _across_ a separator.
    // Arguments:
    // - direction: The Direction to compare
    // - splitType: The SplitState to compare
    // Return Value:
    // - true iff the direction is perpendicular to the splitType. False for
    //   SplitState::None.
    static constexpr bool DirectionMatchesSplit(const winrt::TerminalApp::Direction& direction,
                                                const SplitState& splitType)
    {
        if (splitType == SplitState::None)
        {
            return false;
        }
        else if (splitType == SplitState::Horizontal)
        {
            return direction == winrt::TerminalApp::Direction::Up ||
                   direction == winrt::TerminalApp::Direction::Down;
        }
        else if (splitType == SplitState::Vertical)
        {
            return direction == winrt::TerminalApp::Direction::Left ||
                   direction == winrt::TerminalApp::Direction::Right;
        }
        return false;
    }

    static void _SetupResources();
};
