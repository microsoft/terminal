// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "TerminalSettingsCache.h"
#include "TmuxWindow.h"
#include "../TerminalSettingsAppAdapterLib/TerminalSettings.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace ::Microsoft::Terminal::Tmux;
using TmuxRemoteWindow = ::Microsoft::Terminal::Tmux::Window;

namespace winrt::TerminalApp::implementation
{
    ITerminalConnection TerminalPage::_WrapTmuxConnection(const ITerminalConnection& connection,
                                                          const Profile& profile,
                                                          const uint32_t historyLimit)
    {
        return TmuxWindow::Wrap(*this, connection, profile, historyLimit);
    }

    void TerminalPage::_SyncTmuxWindows(const std::shared_ptr<TmuxSession>& session,
                                        const Profile& profile,
                                        std::vector<TmuxRemoteWindow> windows)
    {
        if (windows.empty() || !session->Active())
        {
            if (const auto found = _tmuxSessions.find(session); found != _tmuxSessions.end())
            {
                auto previous = std::move(found->second);
                _tmuxSessions.erase(found);
                for (auto& [id, state] : previous.windows)
                {
                    if (const auto tab = state.tab.get())
                    {
                        tab->Close();
                    }
                }
            }
            return;
        }

        auto& state = _tmuxSessions[session];
        const auto focused = _GetFocusedTab();
        bool sessionFocused{ static_cast<bool>(_tmuxWindow) };
        if (const auto control = _GetActiveControl())
        {
            if (const auto pane = TmuxPaneConnection::From(control.Connection()))
            {
                sessionFocused = pane->Session() == session;
            }
            else if (const auto gateway = TmuxGatewayConnection::From(control.Connection()))
            {
                sessionFocused = gateway->UnderlyingConnection() == session->Transport();
            }
        }
        TerminalApp::Tab initialSelection{ nullptr };
        std::map<uint32_t, TmuxTabState> next;
        for (auto& window : windows)
        {
            auto old = state.windows.find(window.id);
            TmuxTabState entry;
            if (old != state.windows.end())
            {
                entry = std::move(old->second);
            }
            auto tab = entry.tab.get();
            const auto added = !tab;
            const auto changed = !tab || entry.window.layoutText != window.layoutText;
            if (changed)
            {
                std::map<uint32_t, TerminalApp::IPaneContent> contents;
                window.layout.Leaves([&](const Layout& leaf) {
                    const auto existing = entry.contents.find(leaf.pane);
                    if (existing != entry.contents.end())
                    {
                        contents.emplace(leaf.pane, existing->second);
                        return;
                    }
                    const auto settings = winrt::Microsoft::Terminal::Settings::TerminalSettings::CreateWithProfile(_settings, _currentWindowSettings(), profile);
                    auto connection = winrt::make_self<TmuxPaneConnection>(session, window.id, leaf.pane);
                    const auto control = _CreateNewControlAndContent(settings, *connection);
                    control.GotFocus([weak = connection->get_weak()](auto&&, auto&&) {
                        if (const auto pane = weak.get())
                        {
                            pane->Focus();
                        }
                    });
                    contents.emplace(leaf.pane, winrt::make<TerminalPaneContent>(profile, _terminalSettingsCache, control));
                });

                const auto build = [&]() {
                    std::function<std::shared_ptr<Pane>(const Layout&)> tree;
                    tree = [&](const Layout& node) -> std::shared_ptr<Pane> {
                        if (node.kind == Layout::Kind::Leaf)
                        {
                            return std::make_shared<Pane>(contents.at(node.pane), node.pane == window.activePane);
                        }
                        auto rest = tree(node.children.back());
                        auto extent = node.kind == Layout::Kind::LeftRight ? node.children.back().width : node.children.back().height;
                        for (size_t i = node.children.size() - 1; i > 0; --i)
                        {
                            const auto& child = node.children[i - 1];
                            const auto size = node.kind == Layout::Kind::LeftRight ? child.width : child.height;
                            const auto ratio = static_cast<float>(size) / static_cast<float>(size + extent);
                            rest = std::make_shared<Pane>(tree(child), rest, node.kind == Layout::Kind::LeftRight ? SplitState::Vertical : SplitState::Horizontal, ratio);
                            extent += size + 1;
                        }
                        return rest;
                    };
                    return tree(window.layout);
                };
                if (tab)
                {
                    tab->ReplacePaneTree(build);
                    tab->UpdateSettings(_settings, _currentWindowSettings());
                }
                else
                {
                    const auto newTab = _CreateNewTabFromPane(build());
                    tab = newTab.as<Tab>();
                    entry.tab = tab->get_weak();
                }
                for (const auto& [id, content] : entry.contents)
                {
                    if (!contents.contains(id))
                    {
                        content.Close();
                    }
                }
                entry.contents = std::move(contents);
            }
            tab->SetTabText(winrt::to_hstring(window.name));
            if (window.activePane != entry.window.activePane || window.zoomed)
            {
                tab->GetRootPane()->WalkTree([&](const auto& pane) {
                    if (const auto control = pane->GetTerminalControl())
                    {
                        if (const auto connection = TmuxPaneConnection::From(control.Connection());
                            connection && connection->PaneId() == window.activePane)
                        {
                            tab->FocusPane(pane->Id().value());
                        }
                    }
                });
            }
            if (window.zoomed && !tab->IsZoomed())
            {
                tab->EnterZoom();
            }
            else if (!window.zoomed && tab->IsZoomed())
            {
                tab->ExitZoom();
            }
            if (window.active && (!state.opened || (added && sessionFocused)))
            {
                initialSelection = *tab;
            }
            entry.window = std::move(window);
            next.emplace(entry.window.id, std::move(entry));
        }
        for (const auto& [id, entry] : state.windows)
        {
            if (!next.contains(id))
            {
                if (const auto tab = entry.tab.get())
                {
                    tab->Close();
                }
            }
        }
        state.windows = std::move(next);
        state.opened = true;
        if (initialSelection)
        {
            _SetFocusedTab(initialSelection);
        }
        else if (focused && _GetTabIndex(focused))
        {
            _SetFocusedTab(focused);
        }
        session->PaneResized();
    }

    void TerminalPage::_ResizeTmuxWindows(const std::shared_ptr<TmuxSession>& session)
    {
        session->AcknowledgeResize();
        const auto state = _tmuxSessions.find(session);
        if (state == _tmuxSessions.end() || !session->Active())
        {
            return;
        }
        for (const auto& [id, entry] : state->second.windows)
        {
            const auto tab = entry.tab.get();
            if (!tab)
            {
                continue;
            }
            const auto measure = [&](const uint32_t pane) -> std::pair<double, double> {
                const auto content = entry.contents.at(pane).as<TerminalApp::TerminalPaneContent>();
                const auto size = content.GetTermControl().UnsnappedViewportSize();
                return { size.Width, size.Height };
            };
            // Rounding each pane separately makes the total depend on the split
            // ratio, feeding one-cell changes back into tmux indefinitely.
            const auto size = entry.window.zoomed ? measure(entry.window.activePane) : entry.window.layout.Measure(measure);
            if (size.first > 0 && size.second > 0)
            {
                session->ResizeWindow(id, std::max(static_cast<uint32_t>(size.first), 1u), std::max(static_cast<uint32_t>(size.second), 1u));
            }
        }
        session->CapturePending();
    }

    bool TerminalPage::_TryTmuxSplit(const winrt::com_ptr<Tab>& tab, SplitDirection direction, const float size)
    {
        if (tab)
        {
            if (const auto control = tab->GetActiveTerminalControl())
            {
                if (const auto connection = TmuxPaneConnection::From(control.Connection()))
                {
                    if (direction == SplitDirection::Automatic)
                    {
                        const auto viewport = control.ViewportSize();
                        direction = viewport.Width > viewport.Height * 2 ? SplitDirection::Right : SplitDirection::Down;
                    }
                    connection->Split(direction == SplitDirection::Left || direction == SplitDirection::Right,
                                      direction == SplitDirection::Left || direction == SplitDirection::Up,
                                      size);
                    return true;
                }
            }
        }
        return false;
    }

    void TerminalPage::_KillTmuxPanes(const std::shared_ptr<Pane>& pane)
    {
        if (pane)
        {
            pane->WalkTree([](const auto& child) {
                if (const auto control = child->GetTerminalControl())
                {
                    if (const auto connection = TmuxPaneConnection::From(control.Connection()))
                    {
                        connection->Kill();
                    }
                }
            });
        }
    }

    bool TerminalPage::_ContainsTmuxPane(const std::shared_ptr<Pane>& pane, const bool includeGateway)
    {
        return pane && pane->WalkTree([includeGateway](const auto& child) {
            const auto control = child->GetTerminalControl();
            if (!control)
            {
                return false;
            }
            if (TmuxPaneConnection::From(control.Connection()))
            {
                return true;
            }
            const auto gateway = includeGateway ? TmuxGatewayConnection::From(control.Connection()) : nullptr;
            return gateway && gateway->InControlMode();
        });
    }
}
