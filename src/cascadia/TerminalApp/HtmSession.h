// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HtmConnections.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
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
        std::string LeaderPaneId() const;

        void RegisterFollower(HtmFollowerConnection* follower);
        void UnregisterFollower(HtmFollowerConnection* follower);
        bool HasFollower(const std::string& paneId) const;
        std::string FirstLiveFollowerPaneId() const;

        void WriteToLeader(std::string_view command);
        void HandleLine(std::string_view line);
        void HandleExitSequence();
        void HandleLeaderInput(std::string_view keys);
        void SendKeys(std::string_view paneId, std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserTab();
        bool HandleUserClose(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);

        // First native HTM pane opens an OS window; later panes become tabs on
        // that host (WT-native), instead of one OS window per tmux window.
        void SetNativeHostPage(TerminalPage* page) noexcept;
        TerminalPage* NativeHostPage() const noexcept;
        void ClearNativeHostPage(TerminalPage* page) noexcept;
        void RegisterFollowerPage(TerminalPage* page) noexcept;
        // WT new-tab → tab on the native HTM host (first one opens an OS window).
        void OpenFollowerAsTab(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& follower);
        // WT new-window / server new-window → always a new OS window.
        void OpenFollowerAsWindow(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& follower);

    private:
        struct PendingFollower
        {
            HtmFollowerConnection* connection;
            bool isTab;
        };

        void _appendToPane(const std::string& paneId, std::string_view utf8);
        void _exitHtmMode();
        void _finishReply();
        void _gatewayPrint(std::string_view text);
        void _logProtocol(std::string_view direction, std::string_view line);
        void _ensureNativePane(const std::string& paneId);
        void _closeFollowerUi();
        void _syncFollowersToLayout(std::string_view layout);
        void _renameWindowTabs(const std::string& windowId, const std::string& name);
        void _detachCleanly();
        void _forceQuit();
        void _toggleLogging();
        void _beginCommandPrompt();
        void _handleCommandPromptKey(char ch);

        TerminalPage* _page;
        winrt::weak_ref<TerminalPage> _nativeHostPage;
        std::vector<winrt::weak_ref<TerminalPage>> _followerPages;
        std::unordered_map<std::string, std::string> _paneToWindow; // "%0" -> "@1"
        HtmLeaderConnection* _leader{ nullptr };
        mutable std::mutex _mutex;
        std::unordered_map<std::string, HtmFollowerConnection*> _followers;
        std::unordered_set<std::string> _pendingNativePanes;
        std::vector<PendingFollower> _pendingFollowers;
        std::vector<std::string> _replyLines;
        std::string _commandBuffer;
        std::string _homePaneId;
        bool _inReply{ false };
        bool _suppressClosePackets{ false };
        bool _protocolLogging{ false };
        bool _commandPrompt{ false };
        bool _detaching{ false };
    };
}
