// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HtmConnections.h"

#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <winrt/Windows.Data.Json.h>

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

        void WriteToLeader(std::string_view packet);
        void HandlePacket(char header, std::string_view payload);
        void HandleExitSequence();

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection CreateFollowerForUserTab();
        bool HandleUserClose(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection);

        std::string GenerateUuid();

    private:
        void _applyInitState(const std::string& json);
        void _appendToPane(const std::string& paneId, std::string_view utf8);
        void _closePaneFromServer(const std::string& paneId);
        void _exitHtmMode(bool fromServer);
        std::string _firstPaneId(const winrt::Windows::Data::Json::JsonObject& state, const winrt::hstring& paneOrSplit);
        void _createSplits(const winrt::Windows::Data::Json::JsonObject& state, const winrt::Windows::Data::Json::JsonObject& split);

        TerminalPage* _page;
        HtmLeaderConnection* _leader{ nullptr };
        std::mutex _mutex;
        std::unordered_map<std::string, HtmFollowerConnection*> _followers;
        std::unordered_set<std::string> _initializedPanes;
        std::string _nextPaneId;
        bool _applyingLayout{ false };
        bool _suppressClosePackets{ false };
    };
}
