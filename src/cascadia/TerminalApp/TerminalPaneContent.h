// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TerminalPaneContent.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct TerminalPaneContent : TerminalPaneContentT<TerminalPaneContent>
    {
        TerminalPaneContent(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                            const winrt::Microsoft::Terminal::Control::TermControl& control);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();
        winrt::Microsoft::Terminal::Control::TermControl GetTerminal();
        winrt::Windows::Foundation::Size MinSize();
        void Focus();
        void Close();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                            const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);

        winrt::Microsoft::Terminal::Settings::Model::Profile GetProfile() const
        {
            return _profile;
        };

    private:
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState _connectionState{ winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };

        winrt::event_token _connectionStateChangedToken{ 0 };
        winrt::event_token _warningBellToken{ 0 };

        winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;
        winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;

        void _ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                                   const winrt::Windows::Foundation::IInspectable& /*args*/);
        void _ControlWarningBellHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                        winrt::Windows::Foundation::IInspectable const& e);
    };
}
