// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - LeafPane.h
//
// Abstract:
// - Panes are an abstraction by which the terminal can display multiple terminal
//   instances simultaneously in a single terminal window. While tabs allow for
//   a single terminal window to have many terminal sessions running
//   simultaneously within a single window, only one tab can be visible at a
//   time. Panes, on the other hand, allow a user to have many different
//   terminal sessions visible to the user within the context of a single window
//   at the same time. This can enable greater productivity from the user, as
//   they can see the output of one terminal window while working in another.
// - See doc/cascadia/Panes.md for a detailed description.
// - Panes can be one of 2 types, parent or leaf. A parent pane contains 2 other panes
//   (each of which could itself be a parent or could be a leaf). A leaf pane contains
//   a terminal control.
//
// Author(s):
// - Mike Griese (zadjii-msft) 16-May-2019
// - Pankaj Bhojwani February-2021

#pragma once

#include "../../cascadia/inc/cppwinrt_utils.h"

#include "LeafPane.g.h"

namespace winrt::TerminalApp::implementation
{
    DEFINE_ENUM_FLAG_OPERATORS(BordersEnum);

    struct LeafPane : LeafPaneT<LeafPane>
    {
    public:
        LeafPane();
        LeafPane(const GUID& profile,
                 const winrt::Microsoft::Terminal::Control::TermControl& control,
                 const bool lastFocused = false);

        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

        IPane GetActivePane();
        IPane FindFirstLeaf();
        winrt::Microsoft::Terminal::Control::TermControl TerminalControl();
        GUID Profile();
        void FocusPane(uint32_t id);
        void FocusFirstChild();
        bool HasFocusedChild();

        bool ContainsReadOnly();

        bool WasLastFocused() const noexcept;
        void UpdateVisuals();
        void ClearActive();
        void SetActive();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& /*newSize*/){};

        TerminalApp::LeafPane Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                    const float splitSize,
                                    const GUID& profile,
                                    const winrt::Microsoft::Terminal::Control::TermControl& control);

        float CalcSnappedDimensionSingle(const bool widthOrHeight, const float dimension) const;
        void Shutdown();
        void Close();
        uint32_t GetLeafPaneCount() const noexcept;

        void Maximize(IPane paneToZoom);
        void Restore(IPane paneToUnZoom);

        winrt::Windows::Foundation::Size GetMinSize() const;

        winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Settings::Model::SplitState> PreCalculateAutoSplit(const IPane target,
                                                                                                                              const winrt::Windows::Foundation::Size parentSize) const;
        winrt::Windows::Foundation::IReference<bool> PreCalculateCanSplit(const IPane target,
                                                                          winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                          const float splitSize,
                                                                          const winrt::Windows::Foundation::Size availableSpace) const;

        void UpdateBorderWithClosedNeighbor(TerminalApp::LeafPane closedNeighbor, const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& neighborDirection);

        SnapSizeResult CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        void BorderTappedHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::Input::TappedRoutedEventArgs const& e);

        void UpdateBorders();

        DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<LeafPane>);
        DECLARE_EVENT(LostFocus, _LostFocusHandlers, winrt::delegate<LeafPane>);
        DECLARE_EVENT(PaneRaiseBell, _PaneRaiseBellHandlers, winrt::Windows::Foundation::EventHandler<bool>);
        TYPED_EVENT(Closed, IPane, IPane);
        TYPED_EVENT(PaneTypeChanged, IPane, IPane);

        WINRT_PROPERTY(uint32_t, Id);
        WINRT_PROPERTY(BordersEnum, Borders, BordersEnum::None);

    private:
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState _connectionState{ winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };
        GUID _profile;
        bool _lastActive{ false };
        bool _zoomed{ false };

        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

        winrt::event_token _connectionStateChangedToken{ 0 };
        winrt::event_token _warningBellToken{ 0 };

        winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;
        winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;

        void _ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
        void _ControlWarningBellHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Windows::Foundation::IInspectable const& e);
        void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                     winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _ControlLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                      winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        static void _SetupResources();

        winrt::Microsoft::Terminal::Settings::Model::SplitState _convertAutomaticSplitState(const winrt::Microsoft::Terminal::Settings::Model::SplitState& splitType);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(LeafPane);
}
