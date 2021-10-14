// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPaneContent.h"
#include "TerminalPaneContent.g.cpp"

#include <Mmsystem.h>
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

namespace winrt::TerminalApp::implementation
{
    TerminalPaneContent::TerminalPaneContent(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                                             const winrt::Microsoft::Terminal::Control::TermControl& control) :
        _control{ control },
        _profile{ profile }
    {
        _connectionStateChangedToken = control.ConnectionStateChanged({ this, &TerminalPaneContent::_ControlConnectionStateChangedHandler });
        _warningBellToken = control.WarningBell({ this, &TerminalPaneContent::_ControlWarningBellHandler });
    }

    winrt::Windows::UI::Xaml::FrameworkElement TerminalPaneContent::GetRoot()
    {
        return _control;
    }
    winrt::Microsoft::Terminal::Control::TermControl TerminalPaneContent::GetTerminal()
    {
        return _control;
    }
    winrt::Windows::Foundation::Size TerminalPaneContent::MinSize()
    {
        return _control.MinimumSize();
    }
    void TerminalPaneContent::Focus()
    {
        _control.Focus(FocusState::Programmatic);
    }
    void TerminalPaneContent::Close()
    {
        _control.ConnectionStateChanged(_connectionStateChangedToken);
        _control.WarningBell(_warningBellToken);
    }

    // Method Description:
    // - Called when our attached control is closed. Triggers listeners to our close
    //   event, if we're a leaf pane.
    // - If this was called, and we became a parent pane (due to work on another
    //   thread), this function will do nothing (allowing the control's new parent
    //   to handle the event instead).
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPaneContent::_ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                                    const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        const auto newConnectionState = _control.ConnectionState();
        const auto previousConnectionState = std::exchange(_connectionState, newConnectionState);

        if (newConnectionState < ConnectionState::Closed)
        {
            // Pane doesn't care if the connection isn't entering a terminal state.
            return;
        }

        if (previousConnectionState < ConnectionState::Connected &&
            newConnectionState >= ConnectionState::Failed)
        {
            // A failure to complete the connection (before it has _connected_) is not covered by "closeOnExit".
            // This is to prevent a misconfiguration (closeOnExit: always, startingDirectory: garbage) resulting
            // in Terminal flashing open and immediately closed.
            return;
        }

        if (_profile)
        {
            const auto mode = _profile.CloseOnExit();
            if ((mode == CloseOnExitMode::Always) ||
                (mode == CloseOnExitMode::Graceful && newConnectionState == ConnectionState::Closed))
            {
                // TODO!
                // _CloseRequestedHandlers(*this, nullptr);
            }
        }
    }

    // Method Description:
    // - Plays a warning note when triggered by the BEL control character,
    //   using the sound configured for the "Critical Stop" system event.`
    //   This matches the behavior of the Windows Console host.
    // - Will also flash the taskbar if the bellStyle setting for this profile
    //   has the 'visual' flag set
    // Arguments:
    // - <unused>
    void TerminalPaneContent::_ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                         const winrt::Windows::Foundation::IInspectable& /*eventArgs*/)
    {
        if (_profile)
        {
            // We don't want to do anything if nothing is set, so check for that first
            if (static_cast<int>(_profile.BellStyle()) != 0)
            {
                if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Audible))
                {
                    // Audible is set, play the sound
                    const auto soundAlias = reinterpret_cast<LPCTSTR>(SND_ALIAS_SYSTEMHAND);
                    PlaySound(soundAlias, NULL, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
                }

                if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Window))
                {
                    _control.BellLightOn();
                }

                // raise the event with the bool value corresponding to the taskbar flag
                // TODO!
                // _PaneRaiseBellHandlers(nullptr, WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Taskbar));
            }
        }
    }

    void TerminalPaneContent::UpdateSettings(const TerminalSettingsCreateResult& settings,
                                             const Profile& profile)
    {
        _profile = profile;
        auto controlSettings = _control.Settings().as<TerminalSettings>();
        // Update the parent of the control's settings object (and not the object itself) so
        // that any overrides made by the control don't get affected by the reload
        controlSettings.SetParent(settings.DefaultSettings());
        auto unfocusedSettings{ settings.UnfocusedSettings() };
        if (unfocusedSettings)
        {
            // Note: the unfocused settings needs to be entirely unchanged _except_ we need to
            // set its parent to the settings object that lives in the control. This is because
            // the overrides made by the control live in that settings object, so we want to make
            // sure the unfocused settings inherit from that.
            unfocusedSettings.SetParent(controlSettings);
        }
        _control.UnfocusedAppearance(unfocusedSettings);
        _control.UpdateSettings();
    }
}
