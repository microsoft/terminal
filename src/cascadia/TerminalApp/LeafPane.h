// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

#include "LeafPane.g.h"

namespace winrt::TerminalApp::implementation
{
    enum class Borders : int
    {
        None = 0x0,
        Top = 0x1,
        Bottom = 0x2,
        Left = 0x4,
        Right = 0x8
    };
    DEFINE_ENUM_FLAG_OPERATORS(Borders);

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
        winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
        GUID GetProfile();
        void FocusPane(uint32_t id);
        void FocusFirstChild();
        bool HasFocusedChild();

        bool WasLastFocused() const noexcept;
        void UpdateVisuals();
        void ClearActive();
        void SetActive();

        void UpdateSettings(const winrt::TerminalApp::TerminalSettings& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& /*newSize*/) {};

        std::pair<IPane, IPane> Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                      const float splitSize,
                                      const GUID& profile,
                                      const winrt::Microsoft::Terminal::TerminalControl::TermControl& control);

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
        void Shutdown();
        void Close();
        int GetLeafPaneCount() const noexcept;

        uint16_t Id() noexcept;
        void Id(uint16_t) noexcept;

        winrt::Windows::Foundation::Size GetMinSize() const;

        winrt::Windows::Foundation::IReference<winrt::Microsoft::Terminal::Settings::Model::SplitState> PreCalculateAutoSplit(const IPane target,
                                                                                                                              const winrt::Windows::Foundation::Size parentSize) const;
        winrt::Windows::Foundation::IReference<bool> PreCalculateCanSplit(const IPane target,
                                                                          winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                          const float splitSize,
                                                                          const winrt::Windows::Foundation::Size availableSpace) const;

        SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        void BorderTappedHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::Input::TappedRoutedEventArgs const& e);
        WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
        DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<LeafPane>);

    private:
        winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
        GUID _profile;
        bool _lastActive{ false };
        uint16_t _id;
        Borders _borders{ Borders::None };

        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
        static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

        winrt::event_token _connectionStateChangedToken{ 0 };
        winrt::event_token _warningBellToken{ 0 };

        winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;

        void _ControlConnectionStateChangedHandler(const winrt::Microsoft::Terminal::TerminalControl::TermControl& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
        void _ControlWarningBellHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Windows::Foundation::IInspectable const& e);
        void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                     winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        void _UpdateBorders();

        static void _SetupResources();

        winrt::Microsoft::Terminal::Settings::Model::SplitState _convertAutomaticSplitState(const winrt::Microsoft::Terminal::Settings::Model::SplitState& splitType);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(LeafPane);
}
