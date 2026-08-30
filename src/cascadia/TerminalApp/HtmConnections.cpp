// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HtmConnections.h"
#include "HtmSession.h"

using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::Microsoft::Terminal::Htm;

namespace winrt::TerminalApp::implementation
{
    HtmLeaderConnection::HtmLeaderConnection(ITerminalConnection wrapped, HtmSession* session) :
        _wrapped{ std::move(wrapped) },
        _session{ session }
    {
        _outputRevoker = _wrapped.TerminalOutput(winrt::auto_revoke, { get_weak(), &HtmLeaderConnection::_OutputHandler });
        _stateChangedRevoker = _wrapped.StateChanged(winrt::auto_revoke, [weak = get_weak()](auto&&, auto&&) {
            if (const auto self = weak.get())
            {
                self->StateChanged.raise(*self, nullptr);
            }
        });
    }

    void HtmLeaderConnection::Initialize(const Windows::Foundation::Collections::ValueSet& settings)
    {
        _wrapped.Initialize(settings);
    }

    void HtmLeaderConnection::Start()
    {
        _wrapped.Start();
    }

    void HtmLeaderConnection::WriteInput(const winrt::array_view<const char16_t> data)
    {
        if (_htmMode)
        {
            const auto utf8 = til::u16u8(winrt_array_to_wstring_view(data));
            WriteRaw(FrameInsertDebugKeys(utf8));
            return;
        }
        _wrapped.WriteInput(data);
    }

    void HtmLeaderConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _wrapped.Resize(rows, columns);
        if (_htmMode && !_paneId.empty())
        {
            WriteRaw(FrameResizePane(_paneId, static_cast<int32_t>(columns), static_cast<int32_t>(rows)));
        }
    }

    void HtmLeaderConnection::Close()
    {
        if (_session && _htmMode)
        {
            _session->DetachLeader(this);
        }
        _outputRevoker.revoke();
        _stateChangedRevoker.revoke();
        if (_wrapped)
        {
            _wrapped.Close();
        }
        _wrapped = nullptr;
    }

    winrt::guid HtmLeaderConnection::SessionId() const noexcept
    {
        return _wrapped ? _wrapped.SessionId() : winrt::guid{};
    }

    ConnectionState HtmLeaderConnection::State() const noexcept
    {
        return _wrapped ? _wrapped.State() : ConnectionState::Closed;
    }

    void HtmLeaderConnection::WriteRaw(std::string_view bytes)
    {
        if (!_wrapped || bytes.empty())
        {
            return;
        }
        const auto wide = til::u8u16(bytes);
        _wrapped.WriteInput(winrt_wstring_to_array_view(wide));
    }

    void HtmLeaderConnection::InjectOutput(std::string_view utf8)
    {
        if (utf8.empty())
        {
            return;
        }
        const auto wide = til::u8u16(utf8);
        TerminalOutput.raise(winrt_wstring_to_array_view(wide));
    }

    void HtmLeaderConnection::_OutputHandler(const winrt::array_view<const char16_t> str)
    {
        const auto utf8 = til::u16u8(winrt_array_to_wstring_view(str));
        if (_htmMode)
        {
            if (utf8.find(ExitSequence) != std::string::npos)
            {
                _htmMode = false;
                if (_session)
                {
                    _session->HandleExitSequence();
                }
                return;
            }
            _ProcessHtmBytes(utf8);
            return;
        }

        const auto result = ConsumeInitPayload(_pendingInit, utf8);
        _pendingInit = result.pending;
        if (!result.prefix.empty())
        {
            const auto wide = til::u8u16(result.prefix);
            TerminalOutput.raise(winrt_wstring_to_array_view(wide));
        }
        if (result.matched)
        {
            _htmMode = true;
            if (_session)
            {
                _session->AttachLeader(this);
            }
            if (!result.remainder.empty())
            {
                _ProcessHtmBytes(result.remainder);
            }
        }
    }

    void HtmLeaderConnection::_ProcessHtmBytes(std::string_view utf8)
    {
        _htmBuffer.append(utf8);
        auto [packets, rest] = ParsePackets(_htmBuffer);
        _htmBuffer = std::move(rest);
        for (const auto& packet : packets)
        {
            if (packet.invalidLength)
            {
                _htmMode = false;
                if (_session)
                {
                    _session->HandleExitSequence();
                }
                return;
            }
            if (packet.header == SessionEnd)
            {
                _htmMode = false;
                if (_session)
                {
                    _session->HandlePacket(packet.header, packet.payload);
                }
                return;
            }
            if (_session)
            {
                _session->HandlePacket(packet.header, packet.payload);
            }
        }
    }

    HtmFollowerConnection::HtmFollowerConnection(HtmSession* session, std::string paneId) :
        _session{ session },
        _paneId{ std::move(paneId) }
    {
    }

    void HtmFollowerConnection::Start()
    {
        _started = true;
        StateChanged.raise(*this, nullptr);
        if (_session)
        {
            _session->RegisterFollower(this);
        }
    }

    void HtmFollowerConnection::WriteInput(const winrt::array_view<const char16_t> data)
    {
        if (!_session)
        {
            return;
        }
        const auto utf8 = til::u16u8(winrt_array_to_wstring_view(data));
        _session->WriteToLeader(FrameInsertKeys(_paneId, utf8));
    }

    void HtmFollowerConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _rows = rows;
        _cols = columns;
        if (_session)
        {
            _session->WriteToLeader(FrameResizePane(_paneId, static_cast<int32_t>(columns), static_cast<int32_t>(rows)));
        }
    }

    void HtmFollowerConnection::Close()
    {
        if (_session)
        {
            if (!_suppressClosePacket)
            {
                _session->WriteToLeader(FrameClientClosePane(_paneId));
            }
            _session->UnregisterFollower(this);
        }
        _session = nullptr;
        StateChanged.raise(*this, nullptr);
    }

    void HtmFollowerConnection::InjectOutput(std::string_view utf8)
    {
        if (utf8.empty())
        {
            return;
        }
        const auto wide = til::u8u16(utf8);
        TerminalOutput.raise(winrt_wstring_to_array_view(wide));
    }
}
