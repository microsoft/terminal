// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxWindow.h"
#include "TerminalPage.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    struct TmuxWindow::State
    {
        weak_ref<TerminalPage> source;
        weak_ref<TerminalPage> page;
    };

    TmuxWindow::TmuxWindow(TerminalPage& source, const Profile& profile, std::shared_ptr<TmuxSession> session) :
        _session{ std::move(session) },
        _profile{ profile },
        _state{ std::make_unique<State>() }
    {
        _state->source = source.get_weak();
    }

    TmuxWindow::~TmuxWindow()
    {
        Detach();
    }

    ITerminalConnection TmuxWindow::Wrap(TerminalPage& source,
                                         const ITerminalConnection& transport,
                                         const Profile& profile,
                                         const uint32_t historyLimit)
    {
        auto session = std::make_shared<TmuxSession>(transport, historyLimit);
        auto window = winrt::make_self<TmuxWindow>(source, profile, session);
        const auto weak = window->get_weak();
        const auto dispatcher = source.Dispatcher();
        session->WindowsChanged = [weak, dispatcher](std::vector<::Microsoft::Terminal::Tmux::Window> windows) {
            dispatcher.RunAsync(CoreDispatcherPriority::Normal, [weak, windows = std::move(windows)]() mutable {
                try
                {
                    if (const auto self = weak.get())
                    {
                        self->_WindowsChanged(std::move(windows));
                    }
                }
                CATCH_LOG();
            });
        };
        session->ResizeRequested = [weak, dispatcher] {
            dispatcher.RunAsync(CoreDispatcherPriority::Low, [weak] {
                if (const auto self = weak.get())
                {
                    if (const auto page = self->_Page())
                    {
                        page->_ResizeTmuxWindows(self->_session);
                    }
                    else
                    {
                        self->_session->AcknowledgeResize();
                    }
                }
            });
        };
        return winrt::make<TmuxGatewayConnection>(std::move(session), *window);
    }

    bool TmuxWindow::Equals(const INewContentArgs& other) const
    {
        const auto window = From(other);
        return window && window.get() == this;
    }

    winrt::com_ptr<TmuxWindow> TmuxWindow::From(const INewContentArgs& args)
    {
        return args && args.try_as<ITmuxWindowIdentity>() ? args.as<TmuxWindow>() : nullptr;
    }

    winrt::com_ptr<TerminalPage> TmuxWindow::_Page() const
    {
        return _state->page.get();
    }

    void TmuxWindow::_WindowsChanged(std::vector<::Microsoft::Terminal::Tmux::Window> windows)
    {
        _windows = std::move(windows);
        if (_detaching)
        {
            if (_windows.empty() || !_session->Active())
            {
                _detaching = false;
                _requested = false;
            }
            return;
        }
        if (const auto page = _Page())
        {
            page->_SyncTmuxWindows(_session, _profile, _windows);
            if (_windows.empty() || !_session->Active())
            {
                Detach();
                page->CloseWindowRequested.raise(*page, nullptr);
                _detaching = false;
                _requested = false;
            }
        }
        else if (!_requested && !_windows.empty() && _session->Active())
        {
            if (const auto source = _state->source.get())
            {
                _requested = true;
                _session->WindowOwned(true);
                try
                {
                    source->_OpenNewWindow(*this);
                }
                catch (...)
                {
                    _requested = false;
                    _session->WindowOwned(false);
                    throw;
                }
            }
        }
    }

    void TmuxWindow::Attach(TerminalPage& page)
    {
        _state->page = page.get_weak();
        _unloaded = page.Unloaded(winrt::auto_revoke, [weak = get_weak()](auto&&, auto&&) {
            if (const auto self = weak.get())
            {
                self->Detach();
            }
        });
        const auto button = [](const Controls::Symbol symbol, const hstring& id, const hstring& name, const hstring& tip) {
            Controls::Button result;
            result.Height(24);
            result.Margin(Thickness{ 0, 4, 0, 4 });
            result.Padding(Thickness{ 10, 0, 10, 0 });
            result.Background(Media::SolidColorBrush{ Windows::UI::Colors::Transparent() });
            result.BorderThickness(Thickness{});
            Controls::SymbolIcon icon{ symbol };
            result.Content(icon);
            Automation::AutomationProperties::SetAutomationId(result, id);
            Automation::AutomationProperties::SetName(result, name);
            Controls::ToolTipService::SetToolTip(result, box_value(tip));
            return result;
        };
        _newTabButton = button(Controls::Symbol::Add, L"TmuxNewTabButton", RS_(L"TmuxNewTabButtonName"), RS_(L"TmuxNewTabToolTip"));
        _newTabButton.Click([weak = get_weak()](auto&&, auto&&) {
            if (const auto self = weak.get())
            {
                self->NewTab();
            }
        });
        const auto settings = button(Controls::Symbol::Setting, L"TmuxSettingsButton", RS_(L"SettingsMenuItem"), RS_(L"SettingsToolTip"));
        settings.Click([weak = page.get_weak()](auto&&, auto&&) {
            if (const auto owner = weak.get())
            {
                owner->OpenSettingsUI();
            }
        });
        Controls::StackPanel controls;
        controls.Orientation(Controls::Orientation::Horizontal);
        controls.Children().Append(_newTabButton);
        controls.Children().Append(settings);
        page._tabView.TabStripFooter().as<Controls::Grid>().Children().Append(controls);
        UpdateControls();
        page._SyncTmuxWindows(_session, _profile, _windows);
    }

    void TmuxWindow::UpdateControls()
    {
        if (const auto page = _Page())
        {
            page->_newTabButton.Visibility(Visibility::Collapsed);
            _newTabButton.IsEnabled(_session->Active());
            page->_tabRow.ShowWorkspacesButton(false);
        }
    }

    bool TmuxWindow::TryNewTab(const INewContentArgs& args)
    {
        if (args)
        {
            const auto terminal = args.try_as<NewTerminalArgs>();
            if (!terminal)
            {
                return false;
            }
            if (!terminal.Profile().empty() || terminal.ProfileIndex() ||
                !terminal.Commandline().empty() || !terminal.StartingDirectory().empty() ||
                !terminal.TabTitle().empty() || terminal.ContentId() || terminal.SessionId() != winrt::guid{})
            {
                if (const auto page = _Page())
                {
                    page->_OpenNewWindow(args);
                }
                return true;
            }
        }
        NewTab();
        return true;
    }

    void TmuxWindow::NewTab()
    {
        if (_session->Active())
        {
            _session->Command("new-window");
        }
    }

    void TmuxWindow::Detach()
    {
        if (_Page())
        {
            _state->page = weak_ref<TerminalPage>{};
            _unloaded.revoke();
            _detaching = true;
            _session->Command("detach-client");
            _session->WindowOwned(false);
        }
    }
}
