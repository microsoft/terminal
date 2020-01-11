// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "ParentPane.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <winrt/Microsoft.Terminal.TerminalControl.h>

enum class Borders : int
{
    None = 0x0,
    Top = 0x1,
    Bottom = 0x2,
    Left = 0x4,
    Right = 0x8
};
DEFINE_ENUM_FLAG_OPERATORS(Borders);

class LeafPane : public Pane, public std::enable_shared_from_this<LeafPane>
{
public:
    LeafPane(const GUID& profile,
             const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
             const bool lastFocused = false);
    ~LeafPane() override = default;
    LeafPane(const LeafPane&) = delete;
    LeafPane(LeafPane&&) = delete;
    LeafPane& operator=(const LeafPane&) = delete;
    LeafPane& operator=(LeafPane&&) = delete;

    std::shared_ptr<LeafPane> FindActivePane() override;
    void PropagateToLeaves(std::function<void(LeafPane&)> action) override;
    void PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& edge,
                                 std::function<void(LeafPane&)> action) override;

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings,
                        const GUID& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& /* newSize */) override{};
    void Relayout() override{};

    winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl() const noexcept;
    GUID GetProfile() const noexcept;
    bool CanSplit(winrt::TerminalApp::SplitState splitType);
    std::shared_ptr<LeafPane> Split(winrt::TerminalApp::SplitState splitType,
                      const GUID& profile,
                      const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

    void SetActive(const bool focusControl);
    void ClearActive();
    bool WasLastActive() const noexcept;
    void UpdateBorderWithClosedNeightbour(std::shared_ptr<LeafPane> closedNeightbour,
                                          const winrt::TerminalApp::Direction& neightbourDirection);
    void Close();

    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
    DECLARE_EVENT(Splitted, _SplittedHandlers, winrt::delegate<std::shared_ptr<ParentPane>>);
    DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<LeafPane>>);

private:
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

    winrt::Windows::UI::Xaml::Controls::Border _border{};
    winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };

    GUID _profile;
    bool _lastActive{ false };
    Borders _borders{ Borders::None };

    winrt::event_token _connectionStateChangedToken{ 0 };
    winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;

    winrt::fire_and_forget _ControlConnectionStateChangedHandler(
        const winrt::Microsoft::Terminal::TerminalControl::TermControl& sender,
        const winrt::Windows::Foundation::IInspectable& /*args*/);
    void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    std::shared_ptr<LeafPane> _FindFirstLeaf() override;
    void _UpdateBorders();
    void _UpdateVisuals();
    winrt::TerminalApp::SplitState _ConvertAutomaticSplitState(const winrt::TerminalApp::SplitState& splitType) const;
    SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const override;
    void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const override;
    winrt::Windows::Foundation::Size _GetMinSize() const override;

    static void _SetupResources();
};
