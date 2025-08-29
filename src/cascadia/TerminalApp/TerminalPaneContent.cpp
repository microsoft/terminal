// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPaneContent.h"

#include <mmsystem.h>

#include "TerminalSettingsCache.h"
#include "../../types/inc/utils.hpp"

#include "BellEventArgs.g.cpp"
#include "TerminalPaneContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

namespace winrt::TerminalApp::implementation
{
    TerminalPaneContent::TerminalPaneContent(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                                             const std::shared_ptr<TerminalSettingsCache>& cache,
                                             const winrt::Microsoft::Terminal::Control::TermControl& control) :
        _control{ control },
        _cache{ cache },
        _profile{ profile }
    {
        _setupControlEvents();
    }

    void TerminalPaneContent::_setupControlEvents()
    {
        _controlEvents._ConnectionStateChanged = _control.ConnectionStateChanged(winrt::auto_revoke, { this, &TerminalPaneContent::_controlConnectionStateChangedHandler });
        _controlEvents._WarningBell = _control.WarningBell(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlWarningBellHandler });
        _controlEvents._CloseTerminalRequested = _control.CloseTerminalRequested(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_closeTerminalRequestedHandler });
        _controlEvents._RestartTerminalRequested = _control.RestartTerminalRequested(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_restartTerminalRequestedHandler });

        _controlEvents._TitleChanged = _control.TitleChanged(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlTitleChanged });
        _controlEvents._TabColorChanged = _control.TabColorChanged(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlTabColorChanged });
        _controlEvents._SetTaskbarProgress = _control.SetTaskbarProgress(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlSetTaskbarProgress });
        _controlEvents._ReadOnlyChanged = _control.ReadOnlyChanged(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlReadOnlyChanged });
        _controlEvents._FocusFollowMouseRequested = _control.FocusFollowMouseRequested(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlFocusFollowMouseRequested });
    }
    void TerminalPaneContent::_removeControlEvents()
    {
        _controlEvents = {};
    }

    winrt::Windows::UI::Xaml::FrameworkElement TerminalPaneContent::GetRoot()
    {
        return _control;
    }
    winrt::Microsoft::Terminal::Control::TermControl TerminalPaneContent::GetTermControl()
    {
        return _control;
    }
    winrt::Windows::Foundation::Size TerminalPaneContent::MinimumSize()
    {
        return _control.MinimumSize();
    }
    void TerminalPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _control.Focus(reason);
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

    winrt::hstring TerminalPaneContent::Icon() const
    {
        return _profile.Icon().Resolved();
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> TerminalPaneContent::TabColor() const noexcept
    {
        return _control.TabColor();
    }

    INewContentArgs TerminalPaneContent::GetNewTerminalArgs(const BuildStartupKind kind) const
    {
        NewTerminalArgs args{};
        const auto& controlSettings = _control.Settings();

        args.Profile(::Microsoft::Console::Utils::GuidToString(_profile.Guid()));
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
        // control own its own copy of its settings, this wasn't possible anymore.
        // It probably is once again possible, but Dustin doesn't know how to undo
        // the damage done in the ControlSettings migration.

        switch (kind)
        {
        case BuildStartupKind::Content:
        case BuildStartupKind::MovePane:
            // Only fill in the ContentId if absolutely needed. If you fill in a number
            // here (even 0), we'll serialize that number, AND treat that action as an
            // "attach existing" rather than a "create"
            args.ContentId(_control.ContentId());
            break;
        case BuildStartupKind::PersistAll:
        {
            const auto connection = _control.Connection();
            const auto id = connection ? connection.SessionId() : winrt::guid{};

            if (id != winrt::guid{})
            {
                const auto settingsDir = CascadiaSettings::SettingsDirectory();
                const auto idStr = ::Microsoft::Console::Utils::GuidToPlainString(id);
                const auto path = fmt::format(FMT_COMPILE(L"{}\\buffer_{}.txt"), settingsDir, idStr);
                _control.PersistToPath(path);
                args.SessionId(id);
            }
            break;
        }
        case BuildStartupKind::PersistLayout:
        default:
            break;
        }

        return args;
    }

    void TerminalPaneContent::_controlTitleChanged(const IInspectable&, const IInspectable&)
    {
        TitleChanged.raise(*this, nullptr);
    }
    void TerminalPaneContent::_controlTabColorChanged(const IInspectable&, const IInspectable&)
    {
        TabColorChanged.raise(*this, nullptr);
    }
    void TerminalPaneContent::_controlSetTaskbarProgress(const IInspectable&, const IInspectable&)
    {
        TaskbarProgressChanged.raise(*this, nullptr);
    }
    void TerminalPaneContent::_controlReadOnlyChanged(const IInspectable&, const IInspectable&)
    {
        ReadOnlyChanged.raise(*this, nullptr);
    }
    void TerminalPaneContent::_controlFocusFollowMouseRequested(const IInspectable&, const IInspectable&)
    {
        FocusRequested.raise(*this, nullptr);
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
    safe_void_coroutine TerminalPaneContent::_controlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                                                                   const winrt::Windows::Foundation::IInspectable& args)
    {
        ConnectionStateChanged.raise(sender, args);
        auto newConnectionState = ConnectionState::Closed;
        if (const auto coreState = sender.try_as<ICoreState>())
        {
            newConnectionState = coreState.ConnectionState();
        }

        const auto previousConnectionState = std::exchange(_connectionState, newConnectionState);
        if (newConnectionState < ConnectionState::Closed)
        {
            // Pane doesn't care if the connection isn't entering a terminal state.
            co_return;
        }

        const auto weakThis = get_weak();
        co_await wil::resume_foreground(_control.Dispatcher());
        const auto strongThis = weakThis.get();
        if (!strongThis)
        {
            co_return;
        }

        // It's possible that this event handler started being executed, scheduled
        // on the UI thread, another child got created. So our control is
        // actually no longer _our_ control, and instead could be a descendant.
        //
        // When the control's new Pane takes ownership of the control, the new
        // parent will register its own event handler. That event handler will get
        // fired after this handler returns, and will properly cleanup state.

        if (previousConnectionState < ConnectionState::Connected && newConnectionState >= ConnectionState::Failed)
        {
            // A failure to complete the connection (before it has _connected_) is not covered by "closeOnExit".
            // This is to prevent a misconfiguration (closeOnExit: always, startingDirectory: garbage) resulting
            // in Terminal flashing open and immediately closed.
            co_return;
        }

        if (_profile)
        {
            const auto mode = _profile.CloseOnExit();

            if (
                // This one is obvious: If the user asked for "always" we do just that.
                (mode == CloseOnExitMode::Always) ||
                // Otherwise, and unless the user asked for the opposite of "always",
                // close the pane when the connection closed gracefully (not failed).
                (mode != CloseOnExitMode::Never && newConnectionState == ConnectionState::Closed) ||
                // However, defterm handoff can result in Windows Terminal randomly opening which may be annoying,
                // so by default we should at least always close the pane, even if the command failed.
                // See GH #13325 for discussion.
                (mode == CloseOnExitMode::Automatic && _isDefTermSession))
            {
                CloseRequested.raise(nullptr, nullptr);
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
    void TerminalPaneContent::_controlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
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
                        winrt::hstring soundPath{ sounds.GetAt(rand() % sounds.Size()).Resolved() };
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

                // raise the event with the bool value corresponding to the taskbar flag
                BellRequested.raise(*this,
                                    *winrt::make_self<TerminalApp::implementation::BellEventArgs>(WI_IsFlagSet(_profile.BellStyle(), BellStyle::Taskbar)));
            }
        }
    }

    safe_void_coroutine TerminalPaneContent::_playBellSound(winrt::Windows::Foundation::Uri uri)
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
    void TerminalPaneContent::_closeTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                             const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        CloseRequested.raise(nullptr, nullptr);
    }

    void TerminalPaneContent::_restartTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                               const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        RestartTerminalRequested.raise(*this, nullptr);
    }

    void TerminalPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        // Reload our profile from the settings model to propagate bell mode, icon, and close on exit mode (anything that uses _profile).
        const auto profile{ settings.FindProfile(_profile.Guid()) };
        _profile = profile ? profile : settings.ProfileDefaults();

        if (const auto settings{ _cache->TryLookup(_profile) })
        {
            _control.UpdateControlSettings(settings->DefaultSettings(), settings->UnfocusedSettings());
        }
    }

    // Method Description:
    // - Should be called when this pane is created via a default terminal handoff
    // - Finalizes our configuration given the information that we have been
    //   created via default handoff
    void TerminalPaneContent::MarkAsDefterm()
    {
        _isDefTermSession = true;
    }

    winrt::Windows::UI::Xaml::Media::Brush TerminalPaneContent::BackgroundBrush()
    {
        return _control.BackgroundBrush();
    }

    float TerminalPaneContent::SnapDownToGrid(const TerminalApp::PaneSnapDirection direction, const float sizeToSnap)
    {
        return _control.SnapDimensionToGrid(direction == PaneSnapDirection::Width, sizeToSnap);
    }
    Windows::Foundation::Size TerminalPaneContent::GridUnitSize()
    {
        return _control.CharacterDimensions();
    }
}
