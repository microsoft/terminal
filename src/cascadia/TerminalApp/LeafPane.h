// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>
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
                 const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
                 const bool lastFocused = false);

        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

        IPane GetActivePane();
        IPane FindFirstLeaf();
        winrt::Microsoft::Terminal::TerminalControl::TermControl TerminalControl();
        GUID Profile();
        void FocusPane(uint32_t id);
        void FocusFirstChild();
        bool HasFocusedChild();

        bool ContainsReadOnly();

        bool WasLastFocused() const noexcept;
        void UpdateVisuals();
        void ClearActive();
        void SetActive();

        void UpdateSettings(const winrt::TerminalApp::TerminalSettings& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& /*newSize*/){};

        TerminalApp::LeafPane Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                    const float splitSize,
                                    const GUID& profile,
                                    const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

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

        DECLARE_EVENT(Closed, _ClosedHandlers, winrt::delegate<LeafPane>);
        DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<LeafPane>);
        DECLARE_EVENT(LostFocus, _LostFocusHandlers, winrt::delegate<LeafPane>);
        DECLARE_EVENT(PaneRaiseVisualBell, _PaneRaiseVisualBellHandlers, winrt::delegate<LeafPane>);
        TYPED_EVENT(PaneTypeChanged, IPane, IPane);

        GETSET_PROPERTY(uint16_t, Id);
        GETSET_PROPERTY(BordersEnum, Borders, BordersEnum::None);

    private:
        winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
        GUID _profile;
        bool _lastActive{ false };
        bool _zoomed{ false };

        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

        winrt::event_token _connectionStateChangedToken{ 0 };
        winrt::event_token _warningBellToken{ 0 };

        winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;
        winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;

        void _ControlConnectionStateChangedHandler(const winrt::Microsoft::Terminal::TerminalControl::TermControl& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
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
