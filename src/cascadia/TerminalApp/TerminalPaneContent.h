// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TerminalPaneContent.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"
#include <til/winrt.h>

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

        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                            const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);

        void MarkAsDefterm();

        winrt::Microsoft::Terminal::Settings::Model::Profile GetProfile() const
        {
            return _profile;
        };

        winrt::hstring Title() { return _control.Title(); }
        uint64_t TaskbarState() { return _control.TaskbarState(); }
        uint64_t TaskbarProgress() { return _control.TaskbarProgress(); }
        bool ReadOnly() { return _control.ReadOnly(); }

        float SnapDownToGrid(const TerminalApp::PaneSnapDirection direction, const float sizeToSnap);
        Windows::Foundation::Size GridSize();

        til::typed_event<TerminalApp::TerminalPaneContent, winrt::Windows::Foundation::IInspectable> RestartTerminalRequested;
        til::typed_event<> CloseRequested;

    private:
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState _connectionState{ winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };
        winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
        bool _isDefTermSession{ false };

        winrt::Windows::Media::Playback::MediaPlayer _bellPlayer{ nullptr };
        bool _bellPlayerCreated{ false };

        struct ControlEventTokens
        {
            winrt::Microsoft::Terminal::Control::TermControl::ConnectionStateChanged_revoker _ConnectionStateChanged;
            winrt::Microsoft::Terminal::Control::TermControl::WarningBell_revoker _WarningBell;
            winrt::Microsoft::Terminal::Control::TermControl::CloseTerminalRequested_revoker _CloseTerminalRequested;
            winrt::Microsoft::Terminal::Control::TermControl::RestartTerminalRequested_revoker _RestartTerminalRequested;
        } _controlEvents;
        void _setupControlEvents();
        void _removeControlEvents();

        winrt::fire_and_forget _playBellSound(winrt::Windows::Foundation::Uri uri);

        void _ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
        void _ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                        const winrt::Windows::Foundation::IInspectable& e);
        // void _ControlGotFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
        //                              const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        // void _ControlLostFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
        //                               const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _CloseTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
        void _RestartTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
    };
}
