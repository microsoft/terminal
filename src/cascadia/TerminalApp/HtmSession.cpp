// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HtmSession.h"
#include "HtmConnections.h"
#include "TerminalPage.h"

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::Microsoft::Terminal::Htm;

namespace winrt::TerminalApp::implementation
{
    HtmSession::HtmSession(TerminalPage* page) : _page{ page } {}

    void HtmSession::AttachLeader(HtmLeaderConnection* leader)
    {
        {
            std::lock_guard lock{ _mutex };
            _leader = leader;
        }
        // DCS is detected inside ConptyConnection's output callback. Queue the
        // first command so that callback can return before we call WriteInput
        // on the same connection.
        const auto weakLeader = leader->get_weak();
        _page->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, weakLeader]() {
            if (const auto leader = weakLeader.get(); leader && _leader == leader.get() &&
                                                      leader->State() == ConnectionState::Connected)
            {
                WriteToLeader("refresh-client -C 80x24");
            }
        });
    }

    void HtmSession::DetachLeader(HtmLeaderConnection* leader)
    {
        if (_leader == leader)
        {
            _leader = nullptr;
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
        if (const auto leader{ connection.try_as<HtmLeaderConnection>() })
            return leader.get() == _leader;
        if (const auto follower{ connection.try_as<HtmFollowerConnection>() })
            return _followers.contains(follower->PaneId());
        return false;
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

    void HtmSession::WriteToLeader(std::string_view command)
    {
        if (_leader)
        {
            std::string line{ command };
            // A Windows console in line-input mode submits on CR, not LF.
            // htmd accepts CR, LF, and CRLF as tmux command delimiters.
            if (line.empty() || (line.back() != '\r' && line.back() != '\n'))
                line.push_back('\r');
            _leader->WriteRaw(line);
        }
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
        if (line.rfind("%output ", 0) == 0)
        {
            const auto first = line.find(' ', 8);
            if (first != std::string_view::npos)
                _appendToPane(std::string{ line.substr(8, first - 8) }, UnescapeControlOutput(line.substr(first + 1)));
            return;
        }
        if (line.rfind("%window-pane-changed ", 0) == 0)
        {
            const auto pos = line.rfind(' ');
            if (pos != std::string_view::npos)
            {
                const std::string paneId{ line.substr(pos + 1) };
                HtmFollowerConnection* follower = nullptr;
                {
                    std::lock_guard lock{ _mutex };
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
                else if (_leader && _leader->PaneId().empty())
                {
                    _leader->SetPaneId(paneId);
                }
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
                const auto comma = layout.rfind(',');
                if (comma != std::string_view::npos &&
                    layout.find_first_of("[]{}") == std::string_view::npos)
                {
                    const std::string paneId{ "%" + std::string{ layout.substr(comma + 1) } };
                    HtmFollowerConnection* follower = nullptr;
                    {
                        std::lock_guard lock{ _mutex };
                        if (!_pendingFollowers.empty() && _pendingFollowers.front().isTab)
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

    void HtmSession::HandleExitSequence() { _exitHtmMode(); }

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
            if (_leader && _leader->PaneId() == paneId)
            {
                _leader->InjectOutput(data);
                return;
            }
            std::lock_guard lock{ _mutex };
            if (const auto it = _followers.find(paneId); it != _followers.end() && it->second)
                it->second->InjectOutput(data);
        });
    }

    void HtmSession::_exitHtmMode()
    {
        _suppressClosePackets = true;
        std::lock_guard lock{ _mutex };
        _followers.clear();
        _pendingFollowers.clear();
        _suppressClosePackets = false;
    }
}
