// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HtmSession.h"
#include "HtmConnections.h"
#include "TerminalPage.h"

#include "../../types/inc/utils.hpp"

#include <algorithm>
#include <winrt/Windows.Data.Json.h>

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Windows::Data::Json;
using namespace ::Microsoft::Terminal::Htm;
using namespace ::Microsoft::Console;

namespace winrt::TerminalApp::implementation
{
    HtmSession::HtmSession(TerminalPage* page) :
        _page{ page }
    {
    }

    void HtmSession::AttachLeader(HtmLeaderConnection* leader)
    {
        std::lock_guard lock{ _mutex };
        _leader = leader;
    }

    void HtmSession::DetachLeader(HtmLeaderConnection* leader)
    {
        bool wasLeader = false;
        {
            std::lock_guard lock{ _mutex };
            if (_leader == leader)
            {
                _leader = nullptr;
                wasLeader = true;
            }
        }
        if (wasLeader)
        {
            _exitHtmMode(true);
        }
    }

    bool HtmSession::IsActive() const noexcept
    {
        return _leader != nullptr;
    }

    bool HtmSession::IsHtmConnection(const ITerminalConnection& connection) const
    {
        if (!connection)
        {
            return false;
        }
        if (const auto leader{ connection.try_as<HtmLeaderConnection>() })
        {
            return leader.get() == _leader;
        }
        if (const auto follower{ connection.try_as<HtmFollowerConnection>() })
        {
            return _followers.contains(follower->PaneId());
        }
        return false;
    }

    void HtmSession::RegisterFollower(HtmFollowerConnection* follower)
    {
        if (!follower)
        {
            return;
        }
        std::lock_guard lock{ _mutex };
        _followers[follower->PaneId()] = follower;
        _initializedPanes.insert(follower->PaneId());
    }

    void HtmSession::UnregisterFollower(HtmFollowerConnection* follower)
    {
        if (!follower)
        {
            return;
        }
        std::lock_guard lock{ _mutex };
        _followers.erase(follower->PaneId());
    }

    void HtmSession::WriteToLeader(std::string_view packet)
    {
        if (_leader)
        {
            _leader->WriteRaw(packet);
        }
    }

    std::string HtmSession::GenerateUuid()
    {
        return til::u16u8(Utils::GuidToPlainString(Utils::CreateGuid()));
    }

    void HtmSession::HandleExitSequence()
    {
        _exitHtmMode(true);
    }

    void HtmSession::HandlePacket(char header, std::string_view payload)
    {
        switch (header)
        {
        case InitState:
            _applyInitState(std::string{ payload });
            break;
        case AppendToPane:
        {
            if (payload.size() < UuidLength)
            {
                break;
            }
            const auto paneId = std::string{ payload.substr(0, UuidLength) };
            const auto decoded = Base64Decode(payload.substr(UuidLength));
            _appendToPane(paneId, decoded);
            break;
        }
        case DebugLog:
        {
            const auto decoded = Base64Decode(payload);
            if (_leader)
            {
                _leader->InjectOutput(decoded);
            }
            break;
        }
        case ServerClosePane:
        {
            if (payload.size() >= UuidLength)
            {
                _closePaneFromServer(std::string{ payload.substr(0, UuidLength) });
            }
            break;
        }
        case SessionEnd:
            _exitHtmMode(true);
            break;
        default:
            break;
        }
    }

    ITerminalConnection HtmSession::CreateFollowerForUserSplit(const std::string& sourcePaneId, bool vertical)
    {
        if (!_leader || sourcePaneId.empty() || _applyingLayout)
        {
            return nullptr;
        }
        const auto newId = GenerateUuid();
        WriteToLeader(FrameNewSplit(sourcePaneId, newId, vertical));
        _nextPaneId = newId;
        return winrt::make<HtmFollowerConnection>(this, newId);
    }

    ITerminalConnection HtmSession::CreateFollowerForUserTab()
    {
        if (!_leader || _applyingLayout)
        {
            return nullptr;
        }
        const auto tabId = GenerateUuid();
        const auto paneId = GenerateUuid();
        WriteToLeader(FrameNewTab(tabId, paneId));
        _nextPaneId = paneId;
        return winrt::make<HtmFollowerConnection>(this, paneId);
    }

    bool HtmSession::HandleUserClose(const ITerminalConnection& connection)
    {
        if (!IsHtmConnection(connection) || _suppressClosePackets)
        {
            return false;
        }
        if (const auto follower{ connection.try_as<HtmFollowerConnection>() })
        {
            // Follower Close() sends CLIENT_CLOSE_PANE.
            return true;
        }
        if (const auto leader{ connection.try_as<HtmLeaderConnection>() })
        {
            if (leader.get() == _leader && !leader->PaneId().empty())
            {
                WriteToLeader(FrameClientClosePane(leader->PaneId()));
                return true;
            }
        }
        return false;
    }

    std::string HtmSession::_firstPaneId(const JsonObject& state, const winrt::hstring& paneOrSplit)
    {
        const auto paneKey = paneOrSplit;
        if (state.HasKey(L"panes"))
        {
            if (const auto panes{ state.GetNamedObject(L"panes", nullptr) })
            {
                if (panes.HasKey(paneKey))
                {
                    return til::u16u8(paneKey);
                }
            }
        }
        if (!state.HasKey(L"splits"))
        {
            return til::u16u8(paneKey);
        }
        const auto splits = state.GetNamedObject(L"splits", nullptr);
        if (!splits || !splits.HasKey(paneKey))
        {
            return til::u16u8(paneKey);
        }
        const auto split = splits.GetNamedObject(paneKey);
        const auto children = split.GetNamedArray(L"panesOrSplits", nullptr);
        if (!children || children.Size() == 0)
        {
            return til::u16u8(paneKey);
        }
        return _firstPaneId(state, children.GetAt(0).GetString());
    }

    void HtmSession::_createSplits(const JsonObject& state, const JsonObject& split)
    {
        if (!split)
        {
            return;
        }
        const auto children = split.GetNamedArray(L"panesOrSplits", nullptr);
        const bool vertical = split.GetNamedBoolean(L"vertical", false);
        if (!children)
        {
            return;
        }
        for (uint32_t i = 1; i < children.Size(); ++i)
        {
            const auto sourceId = _firstPaneId(state, children.GetAt(i - 1).GetString());
            const auto newId = _firstPaneId(state, children.GetAt(i).GetString());
            _nextPaneId = newId;
            auto follower = winrt::make<HtmFollowerConnection>(this, newId);
            _page->_HtmSplitExisting(sourceId, follower, vertical);
            _initializedPanes.insert(newId);
        }
        for (uint32_t i = 0; i < children.Size(); ++i)
        {
            const auto id = children.GetAt(i).GetString();
            if (state.HasKey(L"splits"))
            {
                const auto splits = state.GetNamedObject(L"splits");
                if (splits.HasKey(id))
                {
                    _createSplits(state, splits.GetNamedObject(id));
                }
            }
        }
    }

    void HtmSession::_applyInitState(const std::string& json)
    {
        JsonObject state{ nullptr };
        try
        {
            state = JsonObject::Parse(winrt::hstring{ til::u8u16(json) });
        }
        catch (...)
        {
            return;
        }
        if (!state)
        {
            return;
        }

        const auto dispatcher = _page->Dispatcher();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, state]() {
            try
            {
                _applyingLayout = true;
                _initializedPanes.clear();

                std::vector<std::pair<int, JsonObject>> tabs;
                if (state.HasKey(L"tabs"))
                {
                    const auto tabMap = state.GetNamedObject(L"tabs");
                    for (const auto& item : tabMap)
                    {
                        const auto tab = item.Value().GetObject();
                        const auto order = static_cast<int>(tab.GetNamedNumber(L"order", 0));
                        tabs.emplace_back(order, tab);
                    }
                }
                std::sort(tabs.begin(), tabs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

                for (size_t i = 0; i < tabs.size(); ++i)
                {
                    const auto& tab = tabs[i].second;
                    const auto root = tab.GetNamedString(L"paneOrSplit");
                    const auto firstPane = _firstPaneId(state, root);
                    if (i == 0)
                    {
                        if (_leader)
                        {
                            _leader->SetPaneId(firstPane);
                        }
                        _initializedPanes.insert(firstPane);
                    }
                    else
                    {
                        _nextPaneId = firstPane;
                        auto follower = winrt::make<HtmFollowerConnection>(this, firstPane);
                        _page->_HtmNewTab(follower);
                        _initializedPanes.insert(firstPane);
                    }
                    if (state.HasKey(L"splits"))
                    {
                        const auto splits = state.GetNamedObject(L"splits");
                        if (splits.HasKey(root))
                        {
                            _createSplits(state, splits.GetNamedObject(root));
                        }
                    }
                }
            }
            catch (...)
            {
            }
            _applyingLayout = false;
        });
    }

    void HtmSession::_appendToPane(const std::string& paneId, std::string_view utf8)
    {
        const std::string data{ utf8 };
        const auto dispatcher = _page->Dispatcher();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, paneId, data]() {
            if (_leader && _leader->PaneId() == paneId)
            {
                _leader->InjectOutput(data);
                return;
            }
            std::lock_guard lock{ _mutex };
            if (const auto it = _followers.find(paneId); it != _followers.end() && it->second)
            {
                it->second->InjectOutput(data);
            }
        });
    }

    void HtmSession::_closePaneFromServer(const std::string& paneId)
    {
        const auto dispatcher = _page->Dispatcher();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this, paneId]() {
            _suppressClosePackets = true;
            _page->_HtmClosePane(paneId);
            _suppressClosePackets = false;
            std::lock_guard lock{ _mutex };
            _followers.erase(paneId);
        });
    }

    void HtmSession::_exitHtmMode(bool /*fromServer*/)
    {
        const auto dispatcher = _page->Dispatcher();
        dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [this]() {
            _suppressClosePackets = true;
            std::vector<std::string> ids;
            {
                std::lock_guard lock{ _mutex };
                for (const auto& [id, _] : _followers)
                {
                    ids.push_back(id);
                }
            }
            for (const auto& id : ids)
            {
                _page->_HtmClosePane(id);
            }
            {
                std::lock_guard lock{ _mutex };
                _followers.clear();
                _leader = nullptr;
            }
            _suppressClosePackets = false;
        });
    }
}
