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
                                             const winrt::Microsoft::Terminal::Control::TermControl& control,
                                             const bool showPaneTitlebar) :
        _control{ control },
        _cache{ cache },
        _profile{ profile }
    {
        _showPaneTitlebar = showPaneTitlebar;
        _initializePaneTitlebar();
        _setupControlEvents();
    }

    void TerminalPaneContent::_initializePaneTitlebar()
    {
        using namespace winrt::Windows::UI::Xaml::Controls;
        _root = Grid{};
        RowDefinition titleRow;
        titleRow.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Auto));
        _root.RowDefinitions().Append(titleRow);
        _root.RowDefinitions().Append(RowDefinition{});
        _titlebar = Border{};
        _titlebar.Height(PaneTitlebarHeight);
        _titlebar.Visibility(_showPaneTitlebar ? Visibility::Visible : Visibility::Collapsed);
        _titlebar.Background(_control.BackgroundBrush());
        _title = TextBlock{};
        _title.Text(_control.Title());
        _title.HorizontalAlignment(HorizontalAlignment::Center);
        _title.VerticalAlignment(VerticalAlignment::Center);
        _title.TextTrimming(TextTrimming::CharacterEllipsis);
        _title.Margin({ 4, 0, 4, 0 });
        _titlebar.Child(_title);
        _titlebar.DoubleTapped([weak = get_weak()](auto&&, auto&& args) {
            if (const auto self = weak.get())
            {
                self->_renamePane();
                args.Handled(true);
            }
        });
        MenuFlyout menu;
        MenuFlyoutItem rename;
        rename.Text(RS_(L"RenamePaneText"));
        rename.Click([weak = get_weak()](auto&&, auto&&) {
            if (const auto self = weak.get())
            {
                self->_renamePane();
            }
        });
        menu.Items().Append(rename);
        MenuFlyoutItem close;
        close.Text(RS_(L"ClosePaneText"));
        close.Click([weak = get_weak()](auto&&, auto&&) {
            if (const auto self = weak.get())
            {
                self->CloseRequested.raise(*self, nullptr);
            }
        });
        menu.Items().Append(close);
        _titlebar.ContextFlyout(menu);
        _root.Children().Append(_titlebar);
        Grid::SetRow(_control, 1);
        _root.Children().Append(_control);
    }

    void TerminalPaneContent::_renamePane()
    {
        using namespace winrt::Windows::UI::Xaml::Controls;
        TextBox editor;
        editor.Text(_title.Text());
        editor.FontSize(_title.FontSize());
        auto finish = [weak = get_weak()](const TextBox& input, const bool commit) {
            if (const auto self = weak.get(); self && self->_titlebar.Child() == input)
            {
                if (commit)
                {
                    self->_paneTitleOverride = input.Text();
                }
                self->_titlebar.Child(self->_title);
                self->_updatePaneTitlebar();
                self->Focus();
            }
        };
        editor.KeyDown([finish](auto&& sender, auto&& args) {
            if (args.Key() == winrt::Windows::System::VirtualKey::Enter || args.Key() == winrt::Windows::System::VirtualKey::Escape)
            {
                finish(sender.template as<TextBox>(), args.Key() == winrt::Windows::System::VirtualKey::Enter);
                args.Handled(true);
            }
        });
        editor.LostFocus([finish](auto&& sender, auto&&) { finish(sender.template as<TextBox>(), true); });
        _titlebar.Child(editor);
        editor.Focus(FocusState::Programmatic);
        editor.SelectAll();
    }

    safe_void_coroutine TerminalPaneContent::_updatePaneTitlebar()
    {
        const auto lifetime = get_strong();
        if (!_root.Dispatcher().HasThreadAccess())
        {
            co_await winrt::resume_foreground(_root.Dispatcher());
        }
        _title.Text(_paneTitleOverride.empty() ? _control.Title() : _paneTitleOverride);
        _titlebar.Visibility(_showPaneTitlebar ? Visibility::Visible : Visibility::Collapsed);
        if (const auto color = _control.TabColor())
        {
            _titlebar.Background(winrt::Windows::UI::Xaml::Media::SolidColorBrush{ color.Value() });
        }
        else
        {
            _titlebar.Background(_control.BackgroundBrush());
        }
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
        _controlEvents._ShowNotification = _control.ShowNotification(winrt::auto_revoke, { get_weak(), &TerminalPaneContent::_controlShowNotification });
    }
    void TerminalPaneContent::_removeControlEvents()
    {
        _controlEvents = {};
    }

    winrt::Windows::UI::Xaml::FrameworkElement TerminalPaneContent::GetRoot()
    {
        return _root;
    }
    winrt::Microsoft::Terminal::Control::TermControl TerminalPaneContent::GetTermControl()
    {
        return _control;
    }
    winrt::Windows::Foundation::Size TerminalPaneContent::MinimumSize()
    {
        auto size = _control.MinimumSize();
        size.Height += _showPaneTitlebar ? PaneTitlebarHeight : 0;
        return size;
    }
    void TerminalPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _control.Focus(reason);
    }
    void TerminalPaneContent::Close()
    {
        // We deliberately remove the event handlers before closing the control.
        // This is to prevent reentrancy issues, pointless callbacks, etc.
        _removeControlEvents();

        _control.Close();
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
        if (const auto dir = _control.WorkingDirectory(); ::Microsoft::Console::Utils::IsValidDirectory(dir.c_str()))
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
        case BuildStartupKind::Persist:
        {
            const auto connection = _control.Connection();
            const auto id = connection ? connection.SessionId() : winrt::guid{};
            if (id != winrt::guid{})
            {
                args.SessionId(id);
            }
            break;
        }
        default:
            break;
        }

        return args;
    }

    void TerminalPaneContent::_controlTitleChanged(const IInspectable&, const IInspectable&)
    {
        _updatePaneTitlebar();
        TitleChanged.raise(*this, nullptr);
    }
    void TerminalPaneContent::_controlTabColorChanged(const IInspectable&, const IInspectable&)
    {
        _updatePaneTitlebar();
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

    void TerminalPaneContent::_controlShowNotification(const IInspectable& /*sender*/, const ShowNotificationEventArgs& args)
    {
        NotificationRequested.raise(*this, winrt::make<implementation::NotificationEventArgs>(args.Title(), args.Body()));
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
                        // Sound paths are resolved and validated by CascadiaSettings
                        // before we reach this point.
                        auto soundPath{ sounds.GetAt(rand() % sounds.Size()).Resolved() };
                        PlaySoundW(soundPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_SENTRY | SND_NODEFAULT);
                    }
                    else
                    {
                        const auto soundAlias = reinterpret_cast<LPCWSTR>(SND_ALIAS_SYSTEMHAND);
                        PlaySoundW(soundAlias, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
                    }
                }

                if (WI_IsFlagSet(_profile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Window))
                {
                    _control.BellLightOn();
                }

                // raise the event with the bool values corresponding to the taskbar and notification flags
                BellRequested.raise(*this,
                                    *winrt::make_self<TerminalApp::implementation::BellEventArgs>(
                                        WI_IsFlagSet(_profile.BellStyle(), BellStyle::Taskbar),
                                        WI_IsFlagSet(_profile.BellStyle(), BellStyle::Notification)));
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

    void TerminalPaneContent::UpdateSettings(const CascadiaSettings& settings,
                                             const winrt::Microsoft::Terminal::Settings::Model::WindowSettings& windowSettings)
    {
        _showPaneTitlebar = windowSettings.ShowPaneTitlebar();
        _updatePaneTitlebar();
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
        const auto titleHeight = direction == PaneSnapDirection::Height && _showPaneTitlebar ? PaneTitlebarHeight : 0.0f;
        return _control.SnapDimensionToGrid(direction == PaneSnapDirection::Width, sizeToSnap - titleHeight) + titleHeight;
    }
    Windows::Foundation::Size TerminalPaneContent::GridUnitSize()
    {
        return _control.CharacterDimensions();
    }
}
