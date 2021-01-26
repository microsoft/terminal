// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LeafPane.h"

#include "LeafPane.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

namespace winrt::TerminalApp::implementation
{

    winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_focusedBorderBrush = { nullptr };
    winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_unfocusedBorderBrush = { nullptr };

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

        _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &LeafPane::_ControlConnectionStateChangedHandler });
        _warningBellToken = _control.WarningBell({ this, &LeafPane::_ControlWarningBellHandler });

        // On the first Pane's creation, lookup resources we'll use to theme the
        // Pane, including the brushed to use for the focused/unfocused border
        // color.
        if (s_focusedBorderBrush == nullptr || s_unfocusedBorderBrush == nullptr)
        {
            _SetupResources();
        }

        // Use the unfocused border color as the pane background, so an actual color
        // appears behind panes as we animate them sliding in.
        Root().Background(s_unfocusedBorderBrush);

        // Register an event with the control to have it inform us when it gains focus.
        _gotFocusRevoker = control.GotFocus(winrt::auto_revoke, { this, &LeafPane::_ControlGotFocusHandler });

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

    void LeafPane::UpdateSettings(const TerminalSettings& /*settings*/, const GUID& /*profile*/)
    {
    }

    void LeafPane::ResizeContent(const Size& /*newSize*/)
    {
    }

    void LeafPane::FocusPane(uint32_t /*id*/)
    {
    }

    void LeafPane::_ControlConnectionStateChangedHandler(const TermControl& /*sender*/,
                                                         const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
    }

    void LeafPane::_ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                              const winrt::Windows::Foundation::IInspectable& /*eventArgs*/)
    {
    }

    void LeafPane::_ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                           RoutedEventArgs const& /* args */)
    {
    }

    void LeafPane::_SetupResources()
    {
        const auto res = Application::Current().Resources();
        const auto accentColorKey = winrt::box_value(L"SystemAccentColor");
        if (res.HasKey(accentColorKey))
        {
            const auto colorFromResources = res.Lookup(accentColorKey);
            // If SystemAccentColor is _not_ a Color for some reason, use
            // Transparent as the color, so we don't do this process again on
            // the next pane (by leaving s_focusedBorderBrush nullptr)
            auto actualColor = winrt::unbox_value_or<Color>(colorFromResources, Colors::Black());
            s_focusedBorderBrush = SolidColorBrush(actualColor);
        }
        else
        {
            // DON'T use Transparent here - if it's "Transparent", then it won't
            // be able to hittest for clicks, and then clicking on the border
            // will eat focus.
            s_focusedBorderBrush = SolidColorBrush{ Colors::Black() };
        }

        const auto tabViewBackgroundKey = winrt::box_value(L"TabViewBackground");
        if (res.HasKey(tabViewBackgroundKey))
        {
            winrt::Windows::Foundation::IInspectable obj = res.Lookup(tabViewBackgroundKey);
            s_unfocusedBorderBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            // DON'T use Transparent here - if it's "Transparent", then it won't
            // be able to hittest for clicks, and then clicking on the border
            // will eat focus.
            s_unfocusedBorderBrush = SolidColorBrush{ Colors::Black() };
        }
    }
}
