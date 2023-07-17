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
        _setupControlEvents();
    }

    void TerminalPaneContent::_setupControlEvents()
    {
        _controlEvents._ConnectionStateChanged = _control.ConnectionStateChanged(winrt::auto_revoke, { this, &TerminalPaneContent::_ControlConnectionStateChangedHandler });
        _controlEvents._WarningBell = _control.WarningBell(winrt::auto_revoke, { this, &TerminalPaneContent::_ControlWarningBellHandler });
        _controlEvents._CloseTerminalRequested = _control.CloseTerminalRequested(winrt::auto_revoke, { this, &TerminalPaneContent::_CloseTerminalRequestedHandler });
        _controlEvents._RestartTerminalRequested = _control.RestartTerminalRequested(winrt::auto_revoke, { this, &TerminalPaneContent::_RestartTerminalRequestedHandler });
    }
    void TerminalPaneContent::_removeControlEvents()
    {
        _controlEvents = {};
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
        _removeControlEvents();

        // Clear out our media player callbacks, and stop any playing media. This
        // will prevent the callback from being triggered after we've closed, and
        // also make sure that our sound stops when we're closed.
        if (_bellPlayer)
        {
            _bellPlayer.Pause();
            _bellPlayer.Source(nullptr);
            _bellPlayer.Close();
            _bellPlayer = nullptr;
            _bellPlayerCreated = false;
        }
    }

    NewTerminalArgs TerminalPaneContent::GetNewTerminalArgs(const bool asContent) const
    {
        NewTerminalArgs args{};
        auto controlSettings = _control.Settings();

        args.Profile(controlSettings.ProfileName());
        // If we know the user's working directory use it instead of the profile.
        if (const auto dir = _control.WorkingDirectory(); !dir.empty())
        {
            args.StartingDirectory(dir);
        }
        else
        {
            args.StartingDirectory(controlSettings.StartingDirectory());
        }
        args.TabTitle(controlSettings.StartingTitle());
        args.Commandline(controlSettings.Commandline());
        args.SuppressApplicationTitle(controlSettings.SuppressApplicationTitle());
        if (controlSettings.TabColor() || controlSettings.StartingTabColor())
        {
            til::color c;
            // StartingTabColor is prioritized over other colors
            if (const auto color = controlSettings.StartingTabColor())
            {
                c = til::color(color.Value());
            }
            else
            {
                c = til::color(controlSettings.TabColor().Value());
            }

            args.TabColor(winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color>{ static_cast<winrt::Windows::UI::Color>(c) });
        }

        // TODO:GH#9800 - we used to be able to persist the color scheme that a
        // TermControl was initialized with, by name. With the change to having the
        // control own its own copy of its settings, this isn't possible anymore.
        //
        // We may be able to get around this by storing the Name in the Core::Scheme
        // object. That would work for schemes set by the Terminal, but not ones set
        // by VT, but that seems good enough.

        // Only fill in the ContentId if absolutely needed. If you fill in a number
        // here (even 0), we'll serialize that number, AND treat that action as an
        // "attach existing" rather than a "create"
        if (asContent)
        {
            args.ContentId(_control.ContentId());
        }

        return args;
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

        if (previousConnectionState < ConnectionState::Connected && newConnectionState >= ConnectionState::Failed)
        {
            // A failure to complete the connection (before it has _connected_) is not covered by "closeOnExit".
            // This is to prevent a misconfiguration (closeOnExit: always, startingDirectory: garbage) resulting
            // in Terminal flashing open and immediately closed.
            return;
        }

        if (_profile)
        {
            if (_isDefTermSession && _profile.CloseOnExit() == CloseOnExitMode::Automatic)
            {
                // For 'automatic', we only care about the connection state if we were launched by Terminal
                // Since we were launched via defterm, ignore the connection state (i.e. we treat the
                // close on exit mode as 'always', see GH #13325 for discussion)
                Close();
            }

            const auto mode = _profile.CloseOnExit();
            if ((mode == CloseOnExitMode::Always) ||
                ((mode == CloseOnExitMode::Graceful || mode == CloseOnExitMode::Automatic) && newConnectionState == ConnectionState::Closed))
            {
                // TODO! ask the Pane to close
                // Close();
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
                    auto sounds{ _profile.BellSound() };
                    if (sounds && sounds.Size() > 0)
                    {
                        winrt::hstring soundPath{ wil::ExpandEnvironmentStringsW<std::wstring>(sounds.GetAt(rand() % sounds.Size()).c_str()) };
                        winrt::Windows::Foundation::Uri uri{ soundPath };
                        _playBellSound(uri);
                    }
                    else
                    {
                        const auto soundAlias = reinterpret_cast<LPCTSTR>(SND_ALIAS_SYSTEMHAND);
                        PlaySound(soundAlias, NULL, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
                    }
                }

                if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Window))
                {
                    _control.BellLightOn();
                }

                // TODO!
                // // raise the event with the bool value corresponding to the taskbar flag
                // _PaneRaiseBellHandlers(nullptr, WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Taskbar));
            }
        }
    }

    winrt::fire_and_forget TerminalPaneContent::_playBellSound(winrt::Windows::Foundation::Uri uri)
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(_control.Dispatcher());
        if (auto pane{ weakThis.get() })
        {
            if (!_bellPlayerCreated)
            {
                // The MediaPlayer might not exist on Windows N SKU.
                try
                {
                    _bellPlayerCreated = true;
                    _bellPlayer = winrt::Windows::Media::Playback::MediaPlayer();
                    // GH#12258: The media keys (like play/pause) should have no effect on our bell sound.
                    _bellPlayer.CommandManager().IsEnabled(false);
                }
                CATCH_LOG();
            }
            if (_bellPlayer)
            {
                const auto source{ winrt::Windows::Media::Core::MediaSource::CreateFromUri(uri) };
                const auto item{ winrt::Windows::Media::Playback::MediaPlaybackItem(source) };
                _bellPlayer.Source(item);
                _bellPlayer.Play();
            }
        }
    }
    void TerminalPaneContent::_CloseTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                             const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        Close();
    }

    void TerminalPaneContent::_RestartTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                               const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        RestartTerminalRequested.raise(*this, nullptr);
    }

    void TerminalPaneContent::UpdateSettings(const TerminalSettingsCreateResult& settings,
                                             const Profile& profile)
    {
        _profile = profile;
        _control.UpdateControlSettings(settings.DefaultSettings(), settings.UnfocusedSettings());
    }

    // Method Description:
    // - Should be called when this pane is created via a default terminal handoff
    // - Finalizes our configuration given the information that we have been
    //   created via default handoff
    void TerminalPaneContent::MarkAsDefterm()
    {
        _isDefTermSession = true;
    }
}
