// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TmuxConnections.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace winrt::TerminalApp::implementation
{
    struct TerminalPage;

    class TmuxSession
    {
    public:
        explicit TmuxSession(TerminalPage* page);

        void AttachLeader(TmuxLeaderConnection* leader);
        void DetachLeader(TmuxLeaderConnection* leader);
        bool IsActive() const noexcept;
        bool IsTmuxConnection(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection) const;
        std::string LeaderPaneId() const;

        void RegisterFollower(TmuxFollowerConnection* follower);
        void UnregisterFollower(TmuxFollowerConnection* follower);
        bool HasFollower(const std::string& paneId) const;
        std::string FirstLiveFollowerPaneId() const;

        void WriteToLeader(std::string_view command);
        void HandleLine(std::string_view line);
        void HandleExitSequence();
        void HandleLeaderInput(std::string_view keys);
        void SendKeys(std::string_view paneId, std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserTab(std::string_view sourcePaneId = {});
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserWindow(std::string_view sourcePaneId = {});
        bool HandleUserClose(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);

        // First native TMUX pane opens an OS window; later panes become tabs on
        // that host (WT-native), instead of one OS window per tmux window.
        void SetNativeHostPage(TerminalPage* page) noexcept;
        TerminalPage* NativeHostPage() const noexcept;
        void ClearNativeHostPage(TerminalPage* page) noexcept;
        void RegisterFollowerPage(TerminalPage* page) noexcept;
        void SetFollowerAffinityHost(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& follower, std::string_view sourcePaneId);
        // WT new-tab → tab on the native TMUX host (first one opens an OS window).
        void OpenFollowerAsTab(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& follower);
        // WT new-window / server new-window → always a new OS window.
        void OpenFollowerAsWindow(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& follower);

    private:
        struct PendingFollower
        {
            TmuxFollowerConnection* connection;
            bool isTab;
            bool separateNativeWindow;
            std::string affinityGroup;
        };

        void _appendToPane(const std::string& paneId, std::string_view utf8);
        void _exitTmuxMode();
        void _finishReply();
        void _gatewayPrint(std::string_view text);
        void _logProtocol(std::string_view direction, std::string_view line);
        void _ensureNativePane(const std::string& paneId);
        void _closeFollowerUi();
        void _syncFollowersToLayout(std::string_view layout, std::string_view windowId);
        void _publishAffinities();
        void _renameWindowTabs(const std::string& windowId, const std::string& name);
        void _detachCleanly();
        void _forceQuit();
        void _toggleLogging();
        void _beginCommandPrompt();
        void _handleCommandPromptKey(char ch);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _createFollowerForUserWindow(std::string_view sourcePaneId, bool separateNativeWindow);

        TerminalPage* _page;
        winrt::weak_ref<TerminalPage> _nativeHostPage;
        std::vector<winrt::weak_ref<TerminalPage>> _followerPages;
        std::unordered_map<std::string, std::string> _paneToWindow; // "%0" -> "@1"
        std::unordered_map<std::string, std::string> _windowAffinityGroup; // "@1" -> native host key
        std::unordered_map<std::string, std::string> _paneAffinityHint;
        std::unordered_set<std::string> _separateNativePane;
        TmuxLeaderConnection* _leader{ nullptr };
        mutable std::recursive_mutex _mutex;
        std::unordered_map<std::string, TmuxFollowerConnection*> _followers;
        std::unordered_set<std::string> _pendingNativePanes;
        std::vector<PendingFollower> _pendingFollowers;
        std::vector<std::string> _replyLines;
        std::string _commandBuffer;
        std::string _homePaneId;
        std::string _activeWindowId;
        bool _inReply{ false };
        // A fresh Windows Terminal client can reattach to a daemon that has
        // retained the iTerm2-compatible affinity setting. Do not replace it
        // with a transient one-window-per-pane reconstruction until we have
        // read that setting.
        bool _loadingPersistedAffinities{ false };
        // One command reply may already be in flight when the control-mode
        // handshake queues the @affinities query.
        bool _skipPreAffinityReply{ false };
        // Initial refresh-client notifications describe panes but not native
        // window ownership. Keep the daemon's existing affinity value intact
        // until an explicit Windows Terminal tab/window action establishes a
        // new ownership decision.
        bool _suppressInitialAffinityPublish{ true };
        bool _suppressClosePackets{ false };
        bool _protocolLogging{ false };
        bool _commandPrompt{ false };
        bool _detaching{ false };
    };
}
