// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

#include "LeafPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct LeafPane : LeafPaneT<LeafPane>
    {
    public:
        LeafPane();
        LeafPane(const GUID& profile,
                 const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
                 const bool lastFocused = false);

        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

        LeafPane* GetActivePane();
        winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
        GUID GetProfile();
        void FocusPane(uint32_t id);

        bool WasLastFocused() const noexcept;
        void UpdateVisuals();
        void ClearActive();
        void SetActive(const bool active);

        void UpdateSettings(const winrt::TerminalApp::TerminalSettings& settings,
                            const GUID& profile);
        void ResizeContent(const winrt::Windows::Foundation::Size& newSize);

    private:
        winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
        GUID _profile;
        bool _lastActive{ false };

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

        static void _SetupResources();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(LeafPane);
}
