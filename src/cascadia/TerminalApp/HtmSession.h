// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HtmConnections.h"

#include <mutex>
#include <unordered_map>
#include <vector>

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage;

    class HtmSession
    {
    public:
        explicit HtmSession(TerminalPage* page);

        void AttachLeader(HtmLeaderConnection* leader);
        void DetachLeader(HtmLeaderConnection* leader);
        bool IsActive() const noexcept;
        bool IsHtmConnection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection) const;

        void RegisterFollower(HtmFollowerConnection* follower);
        void UnregisterFollower(HtmFollowerConnection* follower);

        void WriteToLeader(std::string_view command);
        void HandleLine(std::string_view line);
        void HandleExitSequence();
        void SendKeys(std::string_view paneId, std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserTab();
        bool HandleUserClose(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);

    private:
        struct PendingFollower
        {
            HtmFollowerConnection* connection;
            bool isTab;
        };

        void _appendToPane(const std::string& paneId, std::string_view utf8);
        void _exitHtmMode();
        void _finishReply();

        TerminalPage* _page;
        HtmLeaderConnection* _leader{ nullptr };
        mutable std::mutex _mutex;
        std::unordered_map<std::string, HtmFollowerConnection*> _followers;
        std::vector<PendingFollower> _pendingFollowers;
        std::vector<std::string> _replyLines;
        bool _inReply{ false };
        bool _suppressClosePackets{ false };
    };
}
