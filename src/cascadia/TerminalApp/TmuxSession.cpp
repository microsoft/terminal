// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TmuxSession.h"
#include "TmuxConnections.h"
#include "TerminalPage.h"

#include <winrt/Windows.System.Threading.h>

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::Microsoft::Terminal::Tmux;

namespace winrt::TerminalApp::implementation
{
    void TmuxSession::SetNativeHostPage(TerminalPage* page) noexcept
    {
        std::lock_guard lock{ _mutex };
        if (!_nativeHostPage && page)
        {
            _nativeHostPage = page->get_weak();
        }
    }

    TerminalPage* TmuxSession::NativeHostPage() const noexcept
    {
        std::lock_guard lock{ _mutex };
        if (const auto host = _nativeHostPage.get())
        {
            return host.get();
        }
        return nullptr;
    }

    void TmuxSession::ClearNativeHostPage(TerminalPage* page) noexcept
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

    void TmuxSession::RegisterFollowerPage(TerminalPage* page) noexcept
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

    void TmuxSession::SetFollowerAffinityHost(const ITerminalConnection& follower, std::string_view sourcePaneId)
    {
        const auto pending = AsTmuxFollower(follower);
        if (!pending)
        {
            return;
        }
        {
            std::lock_guard lock{ _mutex };
            const auto source = _paneToWindow.find(std::string{ sourcePaneId });
            if (source == _paneToWindow.end())
            {
                return;
            }
            const auto group = _windowAffinityGroup.find(source->second);
            const auto& host = group == _windowAffinityGroup.end() ? source->second : group->second;
            for (auto& queued : _pendingFollowers)
            {
                if (queued.connection == pending)
                {
                    queued.affinityGroup = host;
                    return;
                }
            }
            // The tmux reply may win the race with the UI tab construction.
            // In that case the follower is already registered; update the
            // newly-assigned mux window directly.
            for (const auto& [paneId, live] : _followers)
            {
                if (live == pending)
                {
                    const auto target = _paneToWindow.find(paneId);
                    if (target != _paneToWindow.end())
                    {
                        _windowAffinityGroup[target->second] = host;
                    }
                    break;
                }
            }
        }
        _publishAffinities();
    }

    void TmuxSession::OpenFollowerAsTab(const ITerminalConnection& follower)
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
                host->_TmuxNewTab(follower);
            });
            return;
        }
        // No native host yet — first tab still needs an OS window to live in.
        if (_page)
        {
            _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, follower]() {
                _page->_TmuxNewWindow(follower);
            });
        }
    }

    void TmuxSession::OpenFollowerAsWindow(const ITerminalConnection& follower)
    {
        if (!follower || !_page)
        {
            return;
        }
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, follower]() {
            _page->_TmuxNewWindow(follower);
        });
    }

    TmuxSession::TmuxSession(TerminalPage* page) : _page{ page } {}

    void TmuxSession::AttachLeader(TmuxLeaderConnection* leader)
    {
        {
            std::lock_guard lock{ _mutex };
            _leader = leader;
            _detaching = false;
            _loadingPersistedAffinities = true;
            _skipPreAffinityReply = true;
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
                // @affinities is session state owned by TMUX, so it survives
                // the native client that published it. Read it before the
                // refresh-client layout notifications reconstruct followers.
                // The control transport is replaced after detach, while the
                // mux session (and its live @affinities option) remains.
                // Query that session-scoped value: it is also what the pane
                // dump reads, so replay cannot disagree with live state.
                WriteToLeader("show-options -qv @affinities");
            }
        });
    }

    void TmuxSession::DetachLeader(TmuxLeaderConnection* leader)
    {
        if (_leader == leader)
        {
            _leader = nullptr;
            // Snapshot/close native follower windows before clearing maps so we
            // do not leave TermControls writing through a torn-down session.
            _closeFollowerUi();
            _exitTmuxMode();
        }
    }

    bool TmuxSession::IsActive() const noexcept
    {
        std::lock_guard lock{ _mutex };
        return _leader != nullptr;
    }

    bool TmuxSession::IsTmuxConnection(const ITerminalConnection& connection) const
    {
        if (const auto follower{ AsTmuxFollower(connection) })
            return _followers.contains(follower->PaneId());
        if (const auto leader{ AsTmuxLeader(connection) })
            return leader == _leader;
        return false;
    }

    std::string TmuxSession::LeaderPaneId() const
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

    void TmuxSession::RegisterFollower(TmuxFollowerConnection* follower)
    {
        if (follower && !follower->PaneId().empty())
        {
            std::lock_guard lock{ _mutex };
            _followers[follower->PaneId()] = follower;
        }
    }

    void TmuxSession::UnregisterFollower(TmuxFollowerConnection* follower)
    {
        if (follower)
        {
            std::lock_guard lock{ _mutex };
            _followers.erase(follower->PaneId());
        }
    }

    bool TmuxSession::HasFollower(const std::string& paneId) const
    {
        std::lock_guard lock{ _mutex };
        const auto it = _followers.find(paneId);
        return it != _followers.end() && it->second && !it->second->IsClosed();
    }

    std::string TmuxSession::FirstLiveFollowerPaneId() const
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

    void TmuxSession::WriteToLeader(std::string_view command)
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
        try
        {
            const auto strongLeader = _leader->get_strong();
            std::thread([strongLeader, line = std::move(line)]() {
                try
                {
                    if (strongLeader)
                    {
                        strongLeader->WriteRaw(line);
                    }
                }
                catch (...)
                {
                }
            }).detach();
        }
        catch (...)
        {
        }
    }

    void TmuxSession::SendKeys(std::string_view paneId, std::string_view utf8)
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

    void TmuxSession::HandleLine(std::string_view line)
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
            TmuxFollowerConnection* follower = nullptr;
            std::string affinityGroup;
            bool separateNativeWindow = false;
            {
                std::lock_guard lock{ _mutex };
                if (!windowId.empty())
                {
                    _paneToWindow[paneId] = windowId;
                    if (const auto hint = _paneAffinityHint.find(paneId); hint != _paneAffinityHint.end())
                    {
                        auto affinityGroup = hint->second;
                        if (affinityGroup.empty() && !_separateNativePane.contains(paneId))
                        {
                            for (const auto& [otherPane, existingWindow] : _paneToWindow)
                            {
                                if (otherPane != paneId && existingWindow != windowId)
                                {
                                    const auto group = _windowAffinityGroup.find(existingWindow);
                                    affinityGroup = group == _windowAffinityGroup.end() ? existingWindow : group->second;
                                    break;
                                }
                            }
                        }
                        _windowAffinityGroup[windowId] = affinityGroup.empty() ? windowId : affinityGroup;
                        _paneAffinityHint.erase(hint);
                        _separateNativePane.erase(paneId);
                    }
                }
                if (!_pendingFollowers.empty())
                {
                    follower = _pendingFollowers.front().connection;
                    affinityGroup = _pendingFollowers.front().affinityGroup;
                    separateNativeWindow = _pendingFollowers.front().separateNativeWindow;
                    _pendingFollowers.erase(_pendingFollowers.begin());
                    _followers[paneId] = follower;
                    if (affinityGroup.empty() && !separateNativeWindow)
                    {
                        for (const auto& [_, existingWindow] : _paneToWindow)
                        {
                            if (existingWindow != windowId)
                            {
                                const auto group = _windowAffinityGroup.find(existingWindow);
                                affinityGroup = group == _windowAffinityGroup.end() ? existingWindow : group->second;
                                break;
                            }
                        }
                    }
                    if (!windowId.empty())
                    {
                        _windowAffinityGroup[windowId] = affinityGroup.empty() ? windowId : affinityGroup;
                    }
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
            _publishAffinities();
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
        if (line.rfind("%session-window-changed ", 0) == 0)
        {
            const auto space = line.rfind(' ');
            if (space != std::string_view::npos && space + 1 < line.size())
            {
                std::lock_guard lock{ _mutex };
                _activeWindowId = line.substr(space + 1);
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
                const std::string windowId{ line.substr(15, layoutBegin - 15) };
                const auto layout = line.substr(layoutBegin + 1, layoutEnd - layoutBegin - 1);
                // TMUX sends the complete layout before (and sometimes instead
                // of) %window-pane-changed. Preserve its window-to-pane
                // relationship so the terminal can publish @affinities.
                if (!windowId.empty())
                {
                    std::lock_guard lock{ _mutex };
                    for (const auto& paneId : PaneIdsFromTmuxLayout(layout))
                    {
                        _paneToWindow[paneId] = windowId;
                        if (const auto hint = _paneAffinityHint.find(paneId); hint != _paneAffinityHint.end())
                        {
                            auto affinityGroup = hint->second;
                            if (affinityGroup.empty() && !_separateNativePane.contains(paneId))
                            {
                                for (const auto& [otherPane, existingWindow] : _paneToWindow)
                                {
                                    if (otherPane != paneId && existingWindow != windowId)
                                    {
                                        const auto group = _windowAffinityGroup.find(existingWindow);
                                        affinityGroup = group == _windowAffinityGroup.end() ? existingWindow : group->second;
                                        break;
                                    }
                                }
                            }
                            _windowAffinityGroup[windowId] = affinityGroup.empty() ? windowId : affinityGroup;
                            _paneAffinityHint.erase(hint);
                            _separateNativePane.erase(paneId);
                        }
                    }
                }
                _syncFollowersToLayout(layout, windowId);
                const auto comma = layout.rfind(',');
                if (comma != std::string_view::npos &&
                    layout.find_first_of("[]{}") == std::string_view::npos)
                {
                    const std::string paneId{ "%" + std::string{ layout.substr(comma + 1) } };
                    TmuxFollowerConnection* follower = nullptr;
                    std::string affinityGroup;
                    bool separateNativeWindow = false;
                    bool splitInFlight = false;
                    {
                        std::lock_guard lock{ _mutex };
                        if (!_pendingFollowers.empty() && _pendingFollowers.front().isTab)
                        {
                            follower = _pendingFollowers.front().connection;
                            affinityGroup = _pendingFollowers.front().affinityGroup;
                            separateNativeWindow = _pendingFollowers.front().separateNativeWindow;
                            _pendingFollowers.erase(_pendingFollowers.begin());
                            _followers[paneId] = follower;
                            if (affinityGroup.empty() && !separateNativeWindow)
                            {
                                for (const auto& [_, existingWindow] : _paneToWindow)
                                {
                                    if (existingWindow != windowId)
                                    {
                                        const auto group = _windowAffinityGroup.find(existingWindow);
                                        affinityGroup = group == _windowAffinityGroup.end() ? existingWindow : group->second;
                                        break;
                                    }
                                }
                            }
                            if (!windowId.empty())
                            {
                                _windowAffinityGroup[windowId] = affinityGroup.empty() ? windowId : affinityGroup;
                            }
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
                _publishAffinities();
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
            _exitTmuxMode();
            return;
        }
        if (_inReply)
            _replyLines.emplace_back(line);
    }

    void TmuxSession::_finishReply()
    {
        _inReply = false;
        bool refreshAfterAffinityLoad = false;
        {
            std::lock_guard lock{ _mutex };
            if (_loadingPersistedAffinities)
            {
                // Entering control mode can leave one empty command reply in
                // flight ahead of our show-options request. Do not mistake it
                // for an empty @affinities value and start layout replay early.
                if (_skipPreAffinityReply && _replyLines.empty())
                {
                    _skipPreAffinityReply = false;
                    return;
                }
                _skipPreAffinityReply = false;
                _loadingPersistedAffinities = false;
                // Layout replay has no native-host ownership information.
                // When the daemon restored an existing value, keep publication
                // suppressed until a Windows Terminal tab/window action
                // establishes fresh ownership. A new daemon replies with an
                // empty show-options result; in that case the initial layout
                // is authoritative and must seed @affinities immediately.
                // The control transport is replaced on reattach, not this
                // TmuxSession object. If the reply is empty because the
                // socket's reply/notification ordering raced, its existing
                // native-host map is still the authoritative restored state.
                const bool hasPersistedAffinities = !_replyLines.empty() || !_windowAffinityGroup.empty();
                _suppressInitialAffinityPublish = hasPersistedAffinities;
                if (hasPersistedAffinities)
                {
                    // TMUX persists groups as "0,1,3 2,4". Restore only the
                    // group host relation; the refresh layout supplies the
                    // current pane-to-window relation immediately after this.
                    for (const auto& groupText : _replyLines)
                    {
                        std::string_view remaining{ groupText };
                        while (!remaining.empty())
                        {
                            const auto space = remaining.find(' ');
                            const auto group = remaining.substr(0, space);
                            const auto comma = group.find(',');
                            if (!group.empty())
                            {
                                const std::string host{ "@" + std::string{ group.substr(0, comma) } };
                                size_t begin = 0;
                                while (begin < group.size())
                                {
                                    const auto end = group.find(',', begin);
                                    const auto member = group.substr(begin, end == std::string_view::npos ? group.size() - begin : end - begin);
                                    if (!member.empty())
                                    {
                                        _windowAffinityGroup["@" + std::string{ member }] = host;
                                    }
                                    if (end == std::string_view::npos)
                                    {
                                        break;
                                    }
                                    begin = end + 1;
                                }
                            }
                            if (space == std::string_view::npos)
                            {
                                break;
                            }
                            remaining.remove_prefix(space + 1);
                        }
                    }
                }
                // tmux control commands are ordered. Issuing refresh only
                // after this reply prevents its layout notifications from
                // overwriting a surviving multi-window affinity before this
                // fresh native client has restored it.
                refreshAfterAffinityLoad = true;
            }
        }
        if (refreshAfterAffinityLoad)
        {
            WriteToLeader("refresh-client -C 80x24");
            return;
        }
        if (_replyLines.empty())
            return;
        // -P -F '#{pane_id}' replies with exactly the new %pane identifier.
        const auto id = _replyLines.front();
        if (id.empty() || id.front() != '%')
            return;
        TmuxFollowerConnection* follower = nullptr;
        {
            std::lock_guard lock{ _mutex };
            if (_pendingFollowers.empty())
                return;
            follower = _pendingFollowers.front().connection;
            const auto affinityGroup = _pendingFollowers.front().affinityGroup;
            const auto separateNativeWindow = _pendingFollowers.front().separateNativeWindow;
            _pendingFollowers.erase(_pendingFollowers.begin());
            _paneAffinityHint[id] = affinityGroup;
            if (separateNativeWindow)
            {
                _separateNativePane.insert(id);
            }
        }
        follower->SetPaneId(id);
        std::lock_guard lock{ _mutex };
        _followers[id] = follower;
    }

    void TmuxSession::HandleExitSequence()
    {
        _detaching = true;
        _closeFollowerUi();
        _exitTmuxMode();
        _detaching = false;
    }

    ITerminalConnection TmuxSession::CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical)
    {
        if (!_leader || sourcePaneId.empty())
            return nullptr;
        auto follower = winrt::make_self<TmuxFollowerConnection>(this, "");
        {
            std::lock_guard lock{ _mutex };
            _pendingFollowers.push_back({ follower.get(), false, false, {} });
        }
        WriteToLeader(std::string{ "split-window -P -F '#{pane_id}' -t " } + sourcePaneId + (vertical ? " -h" : " -v"));
        return follower.as<ITerminalConnection>();
    }

    ITerminalConnection TmuxSession::CreateFollowerForUserTab(std::string_view sourcePaneId)
    {
        return _createFollowerForUserWindow(sourcePaneId, false);
    }

    ITerminalConnection TmuxSession::CreateFollowerForUserWindow(std::string_view sourcePaneId)
    {
        return _createFollowerForUserWindow(sourcePaneId, true);
    }

    ITerminalConnection TmuxSession::_createFollowerForUserWindow(std::string_view sourcePaneId, bool separateNativeWindow)
    {
        if (!_leader)
            return nullptr;
        auto follower = winrt::make_self<TmuxFollowerConnection>(this, "");
        std::string affinityGroup;
        {
            std::lock_guard lock{ _mutex };
            _suppressInitialAffinityPublish = false;
            if (!separateNativeWindow)
            {
                // The focused WT pane is synchronous with the user action;
                // %session-window-changed can still name the previously
                // focused native window when another OS window was raised.
                const auto pane = _paneToWindow.find(std::string{ sourcePaneId });
                if (pane != _paneToWindow.end())
                {
                    const auto group = _windowAffinityGroup.find(pane->second);
                    affinityGroup = group != _windowAffinityGroup.end() ? group->second : pane->second;
                }
                if (affinityGroup.empty() && !_activeWindowId.empty())
                {
                    const auto group = _windowAffinityGroup.find(_activeWindowId);
                    affinityGroup = group == _windowAffinityGroup.end() ? _activeWindowId : group->second;
                }
                // Command-line new-tab requests can arrive on the session's
                // control page, where no follower is reported as focused.
                // They still target the existing native host, so inherit its
                // sole group rather than turning the tab into a new host.
                if (affinityGroup.empty() && _windowAffinityGroup.size() == 1)
                {
                    affinityGroup = _windowAffinityGroup.begin()->second;
                }
                if (affinityGroup.empty() && !_paneToWindow.empty())
                {
                    affinityGroup = _paneToWindow.begin()->second;
                }
            }
            _pendingFollowers.push_back({ follower.get(), true, separateNativeWindow, std::move(affinityGroup) });
        }
        WriteToLeader("new-window -P -F '#{pane_id}'");
        return follower.as<ITerminalConnection>();
    }

    bool TmuxSession::HandleUserClose(const ITerminalConnection& connection)
    {
        if (_suppressClosePackets || !IsTmuxConnection(connection))
            return false;
        if (const auto follower{ AsTmuxFollower(connection) })
        {
            WriteToLeader("kill-pane -t " + follower->PaneId());
            return true;
        }
        if (const auto leader{ AsTmuxLeader(connection) })
        {
            WriteToLeader("kill-pane -t " + leader->PaneId());
            return true;
        }
        return false;
    }

    void TmuxSession::_appendToPane(const std::string& paneId, std::string_view utf8)
    {
        const std::string data{ utf8 };
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, paneId, data]() {
            TmuxFollowerConnection* follower = nullptr;
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

    void TmuxSession::_exitTmuxMode()
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

    void TmuxSession::_gatewayPrint(std::string_view text)
    {
        if (_leader)
        {
            _leader->InjectOutput(text);
        }
    }

    void TmuxSession::_logProtocol(std::string_view direction, std::string_view line)
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

    void TmuxSession::_ensureNativePane(const std::string& paneId)
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
            auto follower = winrt::make_self<TmuxFollowerConnection>(this, paneId);
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

    void TmuxSession::_closeFollowerUi()
    {
        std::vector<TmuxFollowerConnection*> followers;
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
            // The TMUX server remains alive across a client detach/reattach.
            // Preserve its pane-to-native-host metadata so the replacement
            // followers reconstruct the same OS-window affinity groups.
        }
        // Native TMUX panes live in other OS windows (RequestNewWindow). Silence
        // first, then close on each hosting page (gateway cannot _TmuxFindPane them).
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
                    page->_TmuxClosePane(id);
                }
            });
        }
    }

    void TmuxSession::_syncFollowersToLayout(std::string_view layout, std::string_view windowId)
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
        std::vector<TmuxFollowerConnection*> staleFollowers;
        {
            std::lock_guard lock{ _mutex };
            for (const auto& [id, follower] : _followers)
            {
                // A %layout-change describes one mux window. Do not close
                // panes belonging to other windows merely because they do not
                // occur in this window's layout.
                const auto mapped = _paneToWindow.find(id);
                const bool belongsToLayoutWindow = windowId.empty() ||
                                                   mapped == _paneToWindow.end() ||
                                                   mapped->second == windowId;
                if (belongsToLayoutWindow && !liveSet.contains(id))
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
        // ForceCloseUi + hosting-page _TmuxClosePane tears down TermControls.
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
                    page->_TmuxClosePane(id);
                }
            });
        }
    }

    void TmuxSession::_publishAffinities()
    {
        // TMUX uses @affinities for the same native-window grouping metadata
        // that iTerm2 publishes to tmux. A user New Tab joins its source
        // native host; a user New Window starts a distinct native host.
        std::unordered_map<std::string, std::vector<std::string>> groups;
        {
            std::lock_guard lock{ _mutex };
            if (_loadingPersistedAffinities)
            {
                return;
            }
            if (_suppressInitialAffinityPublish)
            {
                return;
            }
            groups.reserve(_paneToWindow.size());
            for (const auto& [_, window] : _paneToWindow)
            {
                if (!window.empty())
                {
                    const auto group = _windowAffinityGroup.find(window);
                    const auto& key = group == _windowAffinityGroup.end() ? window : group->second;
                    auto& members = groups[key];
                    if (std::find(members.begin(), members.end(), window) == members.end())
                    {
                        members.push_back(window);
                    }
                }
            }
        }
        if (groups.empty())
        {
            return;
        }
        std::vector<std::vector<std::string>> ordered;
        ordered.reserve(groups.size());
        for (auto& [_, members] : groups)
        {
            std::sort(members.begin(), members.end());
            ordered.push_back(std::move(members));
        }
        std::sort(ordered.begin(), ordered.end());
        std::string value;
        for (const auto& members : ordered)
        {
            if (!value.empty())
            {
                value.push_back(' ');
            }
            for (size_t i = 0; i < members.size(); ++i)
            {
                if (i)
                {
                    value.push_back(',');
                }
                const auto& window = members[i];
                value.append(window.substr(1)); // TMUX stores numeric tmux window ids.
            }
        }
        WriteToLeader("set-option @affinities \"" + value + "\"");
    }

    void TmuxSession::_renameWindowTabs(const std::string& windowId, const std::string& name)
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
                    page->_TmuxSetTabTitleForPane(paneId, title);
                }
            });
        }
    }

    void TmuxSession::_detachCleanly()
    {
        _detaching = true;
        WriteToLeader("detach-client");
        _closeFollowerUi();
        // Drop the leader so a leftover gateway window cannot keep sending
        // split-window / send-keys into a dead ConPTY after detach-client.
        _leader = nullptr;
        _exitTmuxMode();
    }

    void TmuxSession::_forceQuit()
    {
        _detaching = true;
        _closeFollowerUi();
        if (_leader)
        {
            auto* leader = _leader;
            _leader = nullptr;
            leader->ForceCloseClient();
        }
        _exitTmuxMode();
        _detaching = false;
    }

    void TmuxSession::_toggleLogging()
    {
        _protocolLogging = !_protocolLogging;
        _gatewayPrint(_protocolLogging ? "\r\ntmux logging enabled\r\n" : "\r\ntmux logging disabled\r\n");
    }

    void TmuxSession::_beginCommandPrompt()
    {
        _commandPrompt = true;
        _commandBuffer.clear();
        _gatewayPrint("\r\nEnter a tmux command: ");
    }

    void TmuxSession::_handleCommandPromptKey(char ch)
    {
        if (ch == '\r' || ch == '\n')
        {
            _commandPrompt = false;
            _gatewayPrint("\r\n");
            const auto command = std::move(_commandBuffer);
            _commandBuffer.clear();
            if (!command.empty())
            {
                constexpr std::string_view selectPrefix{ "select-window -t @" };
                if (command.rfind(selectPrefix, 0) == 0)
                {
                    const auto end = command.find_first_of(" \t", selectPrefix.size());
                    std::lock_guard lock{ _mutex };
                    _activeWindowId = command.substr(selectPrefix.size(), end - selectPrefix.size());
                    _activeWindowId.insert(_activeWindowId.begin(), '@');
                }
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

    void TmuxSession::HandleLeaderInput(std::string_view keys)
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
