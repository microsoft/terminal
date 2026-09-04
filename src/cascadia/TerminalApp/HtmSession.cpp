// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HtmSession.h"
#include "HtmConnections.h"
#include "TerminalPage.h"

#include <winrt/Windows.System.Threading.h>

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::Microsoft::Terminal::Htm;

namespace winrt::TerminalApp::implementation
{
    void HtmSession::SetNativeHostPage(TerminalPage* page) noexcept
    {
        std::lock_guard lock{ _mutex };
        if (!_nativeHostPage && page)
        {
            _nativeHostPage = page->get_weak();
        }
    }

    TerminalPage* HtmSession::NativeHostPage() const noexcept
    {
        std::lock_guard lock{ _mutex };
        if (const auto host = _nativeHostPage.get())
        {
            return host.get();
        }
        return nullptr;
    }

    void HtmSession::ClearNativeHostPage(TerminalPage* page) noexcept
    {
        std::lock_guard lock{ _mutex };
        if (const auto host = _nativeHostPage.get(); host && host.get() == page)
        {
            _nativeHostPage = nullptr;
        }
        std::erase_if(_followerPages, [page](const winrt::weak_ref<TerminalPage>& weak) {
            const auto live = weak.get();
            return !live || live.get() == page;
        });
    }

    void HtmSession::RegisterFollowerPage(TerminalPage* page) noexcept
    {
        if (!page)
        {
            return;
        }
        std::lock_guard lock{ _mutex };
        for (const auto& weak : _followerPages)
        {
            if (const auto live = weak.get(); live && live.get() == page)
            {
                return;
            }
        }
        _followerPages.push_back(page->get_weak());
        if (!_nativeHostPage)
        {
            _nativeHostPage = page->get_weak();
        }
    }

    void HtmSession::OpenFollowerAsTab(const ITerminalConnection& follower)
    {
        if (!follower)
        {
            return;
        }
        winrt::com_ptr<TerminalPage> host;
        {
            std::lock_guard lock{ _mutex };
            host = _nativeHostPage.get();
        }
        if (host)
        {
            host->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [host, follower]() {
                host->_HtmNewTab(follower);
            });
            return;
        }
        // No native host yet — first tab still needs an OS window to live in.
        if (_page)
        {
            _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, follower]() {
                _page->_HtmNewWindow(follower);
            });
        }
    }

    void HtmSession::OpenFollowerAsWindow(const ITerminalConnection& follower)
    {
        if (!follower || !_page)
        {
            return;
        }
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, follower]() {
            _page->_HtmNewWindow(follower);
        });
    }

    HtmSession::HtmSession(TerminalPage* page) : _page{ page } {}

    void HtmSession::AttachLeader(HtmLeaderConnection* leader)
    {
        {
            std::lock_guard lock{ _mutex };
            _leader = leader;
            _detaching = false;
        }
        // DCS is detected inside ConptyConnection's output callback. Queue the
        // first command so that callback can return before we call WriteInput
        // on the same connection.
        const auto weakLeader = leader->get_weak();
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, weakLeader]() {
            if (const auto leader = weakLeader.get(); leader && _leader == leader.get() &&
                                                      leader->State() == ConnectionState::Connected)
            {
                leader->InjectOutput(std::string{ TmuxCommandMenu });
                WriteToLeader("refresh-client -C 80x24");
            }
        });
    }

    void HtmSession::DetachLeader(HtmLeaderConnection* leader)
    {
        if (_leader == leader)
        {
            _leader = nullptr;
            // Snapshot/close native follower windows before clearing maps so we
            // do not leave TermControls writing through a torn-down session.
            _closeFollowerUi();
            _exitHtmMode();
        }
    }

    bool HtmSession::IsActive() const noexcept
    {
        std::lock_guard lock{ _mutex };
        return _leader != nullptr;
    }

    bool HtmSession::IsHtmConnection(const ITerminalConnection& connection) const
    {
        // Follower before leader: both only implement ITerminalConnection, so a
        // leader try_as on a follower can falsely succeed.
        if (const auto follower{ connection.try_as<HtmFollowerConnection>() })
            return _followers.contains(follower->PaneId());
        if (const auto leader{ connection.try_as<HtmLeaderConnection>() })
            return leader.get() == _leader;
        return false;
    }

    std::string HtmSession::LeaderPaneId() const
    {
        std::lock_guard lock{ _mutex };
        auto live = [&](const std::string& id) -> bool {
            const auto it = _followers.find(id);
            return it != _followers.end() && it->second && !it->second->IsClosed();
        };
        if (!_homePaneId.empty() && live(_homePaneId))
        {
            return _homePaneId;
        }
        for (const auto& [id, follower] : _followers)
        {
            if (follower && !follower->IsClosed())
            {
                return id;
            }
        }
        if (_leader)
        {
            auto id = _leader->PaneId();
            if (!id.empty())
            {
                return id;
            }
        }
        return "%0";
    }

    void HtmSession::RegisterFollower(HtmFollowerConnection* follower)
    {
        if (follower && !follower->PaneId().empty())
        {
            std::lock_guard lock{ _mutex };
            _followers[follower->PaneId()] = follower;
        }
    }

    void HtmSession::UnregisterFollower(HtmFollowerConnection* follower)
    {
        if (follower)
        {
            std::lock_guard lock{ _mutex };
            _followers.erase(follower->PaneId());
        }
    }

    bool HtmSession::HasFollower(const std::string& paneId) const
    {
        std::lock_guard lock{ _mutex };
        const auto it = _followers.find(paneId);
        return it != _followers.end() && it->second && !it->second->IsClosed();
    }

    std::string HtmSession::FirstLiveFollowerPaneId() const
    {
        std::lock_guard lock{ _mutex };
        for (const auto& [id, follower] : _followers)
        {
            if (follower && !follower->IsClosed())
            {
                return id;
            }
        }
        return {};
    }

    void HtmSession::WriteToLeader(std::string_view command)
    {
        if (!_leader)
        {
            return;
        }
        std::string line{ command };
        // A Windows console in line-input mode submits on CR, not LF.
        // htmd accepts CR, LF, and CRLF as tmux command delimiters.
        if (line.empty() || (line.back() != '\r' && line.back() != '\n'))
            line.push_back('\r');
        _logProtocol(">", command);
        // Never WriteInput the leader ConPTY on the UI thread or nested inside
        // the leader's TerminalOutput handler. Action handlers (split/new-tab)
        // and follower Start/Resize otherwise deadlock the window ("Not
        // Responding") before htmd ever sees split-window.
        // Hold a strong ref: killing htmd can destroy the leader while a queued
        // write still runs (Debug abort / UAF during stress teardown).
        const auto strongLeader = _leader->get_strong();
        winrt::Windows::System::Threading::ThreadPool::RunAsync(
            [strongLeader, line = std::move(line)](const auto&) {
                if (strongLeader)
                {
                    strongLeader->WriteRaw(line);
                }
            });
    }

    void HtmSession::SendKeys(std::string_view paneId, std::string_view utf8)
    {
        if (paneId.empty())
            return;
        static constexpr char hex[] = "0123456789abcdef";
        std::string command{ "send-keys -H -t " };
        command += paneId;
        for (unsigned char byte : utf8)
        {
            command += " 0x";
            command += hex[byte >> 4];
            command += hex[byte & 15];
        }
        WriteToLeader(command);
    }

    void HtmSession::HandleLine(std::string_view line)
    {
        _logProtocol("<", line);
        if (line.rfind("%output ", 0) == 0)
        {
            if (_detaching)
            {
                return;
            }
            const auto first = line.find(' ', 8);
            if (first != std::string_view::npos)
                _appendToPane(std::string{ line.substr(8, first - 8) }, UnescapeControlOutput(line.substr(first + 1)));
            return;
        }
        if (line.rfind("%window-pane-changed ", 0) == 0)
        {
            // "%window-pane-changed @0 %1"
            const auto body = line.substr(21);
            const auto space = body.find(' ');
            std::string windowId;
            std::string paneId;
            if (space == std::string_view::npos)
            {
                paneId = std::string{ body };
            }
            else
            {
                windowId = std::string{ body.substr(0, space) };
                paneId = std::string{ body.substr(space + 1) };
            }
            if (paneId.empty())
            {
                return;
            }
            HtmFollowerConnection* follower = nullptr;
            {
                std::lock_guard lock{ _mutex };
                if (!windowId.empty())
                {
                    _paneToWindow[paneId] = windowId;
                }
                if (!_pendingFollowers.empty())
                {
                    follower = _pendingFollowers.front().connection;
                    _pendingFollowers.erase(_pendingFollowers.begin());
                    _followers[paneId] = follower;
                }
            }
            if (follower)
            {
                follower->SetPaneId(paneId);
            }
            else
            {
                _ensureNativePane(paneId);
            }
            return;
        }
        if (line.rfind("%window-renamed ", 0) == 0)
        {
            // "%window-renamed @0 timeout"
            const auto body = line.substr(16);
            const auto space = body.find(' ');
            if (space != std::string_view::npos && space + 1 < body.size())
            {
                const std::string windowId{ body.substr(0, space) };
                const std::string name{ body.substr(space + 1) };
                _renameWindowTabs(windowId, name);
            }
            return;
        }
        if (line.rfind("%layout-change ", 0) == 0)
        {
            // tmux does not send %window-pane-changed for a newly-created
            // window. Its initial layout is necessarily a single leaf, whose
            // final comma-separated field is the pane ID. Treat that
            // authoritative notification as a fallback when the new-window
            // command reply races follower startup or delivery.
            const auto layoutBegin = line.find(' ', 15);
            const auto layoutEnd = layoutBegin == std::string_view::npos ? std::string_view::npos : line.find(' ', layoutBegin + 1);
            if (layoutBegin != std::string_view::npos && layoutEnd != std::string_view::npos)
            {
                const auto layout = line.substr(layoutBegin + 1, layoutEnd - layoutBegin - 1);
                _syncFollowersToLayout(layout);
                const auto comma = layout.rfind(',');
                if (comma != std::string_view::npos &&
                    layout.find_first_of("[]{}") == std::string_view::npos)
                {
                    const std::string paneId{ "%" + std::string{ layout.substr(comma + 1) } };
                    HtmFollowerConnection* follower = nullptr;
                    bool splitInFlight = false;
                    {
                        std::lock_guard lock{ _mutex };
                        if (!_pendingFollowers.empty() && _pendingFollowers.front().isTab)
                        {
                            follower = _pendingFollowers.front().connection;
                            _pendingFollowers.erase(_pendingFollowers.begin());
                            _followers[paneId] = follower;
                        }
                        else if (!_pendingFollowers.empty())
                        {
                            // A user split is in flight; wait for %window-pane-changed
                            // or the -P reply rather than opening a duplicate tab.
                            splitInFlight = true;
                        }
                    }
                    if (follower)
                    {
                        follower->SetPaneId(paneId);
                    }
                    else if (!splitInFlight)
                    {
                        _ensureNativePane(paneId);
                    }
                }
            }
            return;
        }
        if (line.rfind("%begin ", 0) == 0)
        {
            _inReply = true;
            _replyLines.clear();
            return;
        }
        if (line.rfind("%end ", 0) == 0 || line.rfind("%error ", 0) == 0)
        {
            _finishReply();
            return;
        }
        if (line == "%exit")
        {
            _closeFollowerUi();
            _exitHtmMode();
            return;
        }
        if (_inReply)
            _replyLines.emplace_back(line);
    }

    void HtmSession::_finishReply()
    {
        _inReply = false;
        if (_replyLines.empty())
            return;
        // -P -F '#{pane_id}' replies with exactly the new %pane identifier.
        const auto id = _replyLines.front();
        if (id.empty() || id.front() != '%')
            return;
        HtmFollowerConnection* follower = nullptr;
        {
            std::lock_guard lock{ _mutex };
            if (_pendingFollowers.empty())
                return;
            follower = _pendingFollowers.front().connection;
            _pendingFollowers.erase(_pendingFollowers.begin());
        }
        follower->SetPaneId(id);
        std::lock_guard lock{ _mutex };
        _followers[id] = follower;
    }

    void HtmSession::HandleExitSequence()
    {
        _detaching = true;
        _closeFollowerUi();
        _exitHtmMode();
        _detaching = false;
    }

    ITerminalConnection HtmSession::CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical)
    {
        if (!_leader || sourcePaneId.empty())
            return nullptr;
        auto follower = winrt::make_self<HtmFollowerConnection>(this, "");
        {
            std::lock_guard lock{ _mutex };
            _pendingFollowers.push_back({ follower.get(), false });
        }
        WriteToLeader(std::string{ "split-window -P -F '#{pane_id}' -t " } + sourcePaneId + (vertical ? " -h" : " -v"));
        return follower.as<ITerminalConnection>();
    }

    ITerminalConnection HtmSession::CreateFollowerForUserTab()
    {
        if (!_leader)
            return nullptr;
        auto follower = winrt::make_self<HtmFollowerConnection>(this, "");
        {
            std::lock_guard lock{ _mutex };
            _pendingFollowers.push_back({ follower.get(), true });
        }
        WriteToLeader("new-window -P -F '#{pane_id}'");
        return follower.as<ITerminalConnection>();
    }

    bool HtmSession::HandleUserClose(const ITerminalConnection& connection)
    {
        if (_suppressClosePackets || !IsHtmConnection(connection))
            return false;
        if (const auto follower{ connection.try_as<HtmFollowerConnection>() })
        {
            WriteToLeader("kill-pane -t " + follower->PaneId());
            return true;
        }
        if (const auto leader{ connection.try_as<HtmLeaderConnection>() })
        {
            WriteToLeader("kill-pane -t " + leader->PaneId());
            return true;
        }
        return false;
    }

    void HtmSession::_appendToPane(const std::string& paneId, std::string_view utf8)
    {
        const std::string data{ utf8 };
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, paneId, data]() {
            HtmFollowerConnection* follower = nullptr;
            {
                std::lock_guard lock{ _mutex };
                if (const auto it = _followers.find(paneId); it != _followers.end())
                {
                    follower = it->second;
                }
            }
            if (follower)
            {
                follower->InjectOutput(data);
            }
        });
    }

    void HtmSession::_exitHtmMode()
    {
        _suppressClosePackets = true;
        _commandPrompt = false;
        _commandBuffer.clear();
        _homePaneId.clear();
        std::lock_guard lock{ _mutex };
        _followers.clear();
        _pendingFollowers.clear();
        _pendingNativePanes.clear();
        _suppressClosePackets = false;
    }

    void HtmSession::_gatewayPrint(std::string_view text)
    {
        if (_leader)
        {
            _leader->InjectOutput(text);
        }
    }

    void HtmSession::_logProtocol(std::string_view direction, std::string_view line)
    {
        if (!_protocolLogging)
        {
            return;
        }
        if (line.rfind("%output ", 0) == 0)
        {
            return;
        }
        std::string text{ "\r\n" };
        text.append(direction);
        text.push_back(' ');
        auto visible = line;
        if (!visible.empty() && (visible.back() == '\r' || visible.back() == '\n'))
        {
            visible.remove_suffix(1);
        }
        text.append(visible);
        text.append("\r\n");
        _gatewayPrint(text);
    }

    void HtmSession::_ensureNativePane(const std::string& paneId)
    {
        if (paneId.empty() || !_page)
        {
            return;
        }
        {
            std::lock_guard lock{ _mutex };
            if (_followers.contains(paneId) || _pendingNativePanes.contains(paneId))
            {
                return;
            }
            _pendingNativePanes.insert(paneId);
        }
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, paneId]() {
            auto releasePending = wil::scope_exit([&]() {
                std::lock_guard lock{ _mutex };
                _pendingNativePanes.erase(paneId);
            });
            {
                std::lock_guard lock{ _mutex };
                if (!_leader || _followers.contains(paneId))
                {
                    return;
                }
            }
            auto follower = winrt::make_self<HtmFollowerConnection>(this, paneId);
            {
                std::lock_guard lock{ _mutex };
                _followers[paneId] = follower.get();
                if (_homePaneId.empty())
                {
                    _homePaneId = paneId;
                }
            }
            OpenFollowerAsWindow(follower.as<ITerminalConnection>());
        });
    }

    void HtmSession::_closeFollowerUi()
    {
        std::vector<HtmFollowerConnection*> followers;
        std::vector<std::string> ids;
        std::vector<winrt::com_ptr<TerminalPage>> pages;
        {
            std::lock_guard lock{ _mutex };
            followers.reserve(_followers.size());
            ids.reserve(_followers.size());
            for (const auto& [id, follower] : _followers)
            {
                ids.push_back(id);
                followers.push_back(follower);
            }
            // Drop map entries first so late %output cannot re-enter InjectOutput
            // after we mark followers closed.
            _followers.clear();
            for (const auto& weak : _followerPages)
            {
                if (auto live = weak.get())
                {
                    pages.push_back(live);
                }
            }
            _nativeHostPage = nullptr;
            _followerPages.clear();
            _paneToWindow.clear();
        }
        // Native HTM panes live in other OS windows (RequestNewWindow). Silence
        // first, then close on each hosting page (gateway cannot _HtmFindPane them).
        for (auto* follower : followers)
        {
            if (follower)
            {
                follower->ForceCloseUi();
            }
        }
        for (const auto& page : pages)
        {
            page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [page, ids]() {
                for (const auto& id : ids)
                {
                    page->_HtmClosePane(id);
                }
            });
        }
    }

    void HtmSession::_syncFollowersToLayout(std::string_view layout)
    {
        if (_detaching)
        {
            return;
        }
        // Drop the optional checksum prefix ("abcd,").
        auto body = layout;
        if (body.size() > 5 && body[4] == ',')
        {
            bool hex = true;
            for (size_t i = 0; i < 4; ++i)
            {
                const char c = body[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                {
                    hex = false;
                    break;
                }
            }
            if (hex)
            {
                body.remove_prefix(5);
            }
        }
        const auto live = PaneIdsFromTmuxLayout(body);
        // A non-empty layout that yields no ids is a parse miss — do not cull
        // every follower (that would drop the last pane and block later splits).
        if (live.empty())
        {
            return;
        }
        std::unordered_set<std::string> liveSet(live.begin(), live.end());
        std::vector<std::string> stale;
        std::vector<HtmFollowerConnection*> staleFollowers;
        {
            std::lock_guard lock{ _mutex };
            for (const auto& [id, follower] : _followers)
            {
                if (!liveSet.contains(id))
                {
                    stale.push_back(id);
                    if (follower)
                    {
                        staleFollowers.push_back(follower);
                    }
                }
            }
            for (const auto& id : stale)
            {
                _followers.erase(id);
                _paneToWindow.erase(id);
                if (_homePaneId == id)
                {
                    _homePaneId = live.front();
                }
            }
        }
        // ForceCloseUi + hosting-page _HtmClosePane tears down TermControls.
        // Leaving silenced inert leaves forced the e2e Cmd+W workaround.
        for (auto* follower : staleFollowers)
        {
            follower->ForceCloseUi();
        }
        std::vector<winrt::com_ptr<TerminalPage>> pages;
        {
            std::lock_guard lock{ _mutex };
            for (const auto& weak : _followerPages)
            {
                if (auto live = weak.get())
                {
                    pages.push_back(live);
                }
            }
        }
        for (const auto& page : pages)
        {
            page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [page, stale]() {
                for (const auto& id : stale)
                {
                    page->_HtmClosePane(id);
                }
            });
        }
    }

    void HtmSession::_renameWindowTabs(const std::string& windowId, const std::string& name)
    {
        if (windowId.empty() || name.empty())
        {
            return;
        }
        std::vector<std::string> paneIds;
        std::vector<winrt::com_ptr<TerminalPage>> pages;
        {
            std::lock_guard lock{ _mutex };
            for (const auto& [paneId, wid] : _paneToWindow)
            {
                if (wid == windowId)
                {
                    paneIds.push_back(paneId);
                }
            }
            for (const auto& weak : _followerPages)
            {
                if (auto live = weak.get())
                {
                    pages.push_back(live);
                }
            }
        }
        if (paneIds.empty())
        {
            return;
        }
        const auto title = winrt::hstring{ til::u8u16(name) };
        for (const auto& page : pages)
        {
            page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [page, paneIds, title]() {
                for (const auto& paneId : paneIds)
                {
                    page->_HtmSetTabTitleForPane(paneId, title);
                }
            });
        }
    }

    void HtmSession::_detachCleanly()
    {
        _detaching = true;
        WriteToLeader("detach-client");
        _closeFollowerUi();
        // Drop the leader so a leftover gateway window cannot keep sending
        // split-window / send-keys into a dead ConPTY after detach-client.
        _leader = nullptr;
        _exitHtmMode();
    }

    void HtmSession::_forceQuit()
    {
        _detaching = true;
        _closeFollowerUi();
        if (_leader)
        {
            auto* leader = _leader;
            _leader = nullptr;
            leader->ForceCloseClient();
        }
        _exitHtmMode();
        _detaching = false;
    }

    void HtmSession::_toggleLogging()
    {
        _protocolLogging = !_protocolLogging;
        _gatewayPrint(_protocolLogging ? "\r\ntmux logging enabled\r\n" : "\r\ntmux logging disabled\r\n");
    }

    void HtmSession::_beginCommandPrompt()
    {
        _commandPrompt = true;
        _commandBuffer.clear();
        _gatewayPrint("\r\nEnter a tmux command: ");
    }

    void HtmSession::_handleCommandPromptKey(char ch)
    {
        if (ch == '\r' || ch == '\n')
        {
            _commandPrompt = false;
            _gatewayPrint("\r\n");
            const auto command = std::move(_commandBuffer);
            _commandBuffer.clear();
            if (!command.empty())
            {
                WriteToLeader(command);
            }
            return;
        }
        if (ch == '\x7f' || ch == '\b')
        {
            if (!_commandBuffer.empty())
            {
                _commandBuffer.pop_back();
                _gatewayPrint("\b \b");
            }
            return;
        }
        if (ch >= 32 && ch < 127)
        {
            _commandBuffer.push_back(ch);
            _gatewayPrint(std::string(1, ch));
        }
    }

    void HtmSession::HandleLeaderInput(std::string_view keys)
    {
        for (unsigned char ch : keys)
        {
            if (_commandPrompt)
            {
                if (ch == 0x1b)
                {
                    _commandPrompt = false;
                    _commandBuffer.clear();
                    _gatewayPrint("\r\n");
                    continue;
                }
                _handleCommandPromptKey(static_cast<char>(ch));
                continue;
            }
            if (ch == 0x1b)
            {
                _detachCleanly();
            }
            else if (ch == 'x' || ch == 'X')
            {
                _forceQuit();
            }
            else if (ch == 'l' || ch == 'L')
            {
                _toggleLogging();
            }
            else if (ch == 'c' || ch == 'C')
            {
                _beginCommandPrompt();
            }
        }
    }
}
