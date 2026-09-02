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
        _wrapped{ wrapped },
        _sessionId{ wrapped.SessionId() },
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
            if (utf8 == "\x1b[I" || utf8 == "\x1b[O")
            {
                return;
            }
            // With VT input enabled, Windows Terminal reports Escape as an
            // enhanced-key sequence whose first codepoint is 27. Plain VT
            // input still arrives as the single ESC byte.
            if (utf8 == "\x1b" || utf8.starts_with("\x1b[27;"))
            {
                _session->WriteToLeader("detach-client");
                return;
            }
            if (utf8 == "x" || utf8.starts_with("\x1b[88;0;120;1;"))
            {
                _session->WriteToLeader("kill-server");
                return;
            }
            _session->SendKeys(_paneId, utf8);
            return;
        }
        _wrapped.WriteInput(data);
    }

    void HtmLeaderConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _wrapped.Resize(rows, columns);
        if (_htmMode && !_paneId.empty())
        {
            _session->WriteToLeader("refresh-client -C " + std::to_string(columns) + "x" + std::to_string(rows));
        }
    }

    void HtmLeaderConnection::Close()
    {
        _closed = true;
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
        return _sessionId;
    }

    ConnectionState HtmLeaderConnection::State() const noexcept
    {
        return _closed ? ConnectionState::Closed : ConnectionState::Connected;
    }

    void HtmLeaderConnection::WriteRaw(std::string_view bytes)
    {
        if (bytes.empty())
        {
            return;
        }
        // Pane input, resizes, and app actions can arrive on different UI and
        // connection threads. Keep each HTM frame in one ConPTY write so a
        // resize cannot splice itself into a key or split packet.
        std::lock_guard lock{ _writeMutex };
        if (!_wrapped)
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
            if (utf8.find(TmuxControlSt) != std::string::npos)
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

        _pendingInit.append(utf8);
        const auto marker = _pendingInit.find(TmuxControlDcs);
        if (marker == std::string::npos)
        {
            if (_pendingInit.size() > TmuxControlDcs.size())
            {
                const auto render = _pendingInit.substr(0, _pendingInit.size() - TmuxControlDcs.size());
                const auto wide = til::u8u16(render);
                TerminalOutput.raise(winrt_wstring_to_array_view(wide));
                _pendingInit.erase(0, _pendingInit.size() - TmuxControlDcs.size());
            }
            return;
        }
        const auto prefix = _pendingInit.substr(0, marker);
        if (!prefix.empty())
        {
            const auto wide = til::u8u16(prefix);
            TerminalOutput.raise(winrt_wstring_to_array_view(wide));
        }
        const auto remainder = _pendingInit.substr(marker + TmuxControlDcs.size());
        _pendingInit.clear();
        _htmMode = true;
        if (_session) _session->AttachLeader(this);
        if (!remainder.empty()) _ProcessHtmBytes(remainder);
    }

    void HtmLeaderConnection::_ProcessHtmBytes(std::string_view utf8)
    {
        _htmBuffer.append(utf8);
        size_t newline = 0;
        while ((newline = _htmBuffer.find('\n')) != std::string::npos)
        {
            auto line = _htmBuffer.substr(0, newline);
            _htmBuffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (_session) _session->HandleLine(line);
        }
    }

    HtmFollowerConnection::HtmFollowerConnection(HtmSession* session, std::string paneId) :
        _session{ session },
        _paneId{ std::move(paneId) }
    {
    }

    void HtmFollowerConnection::Start()
    {
        HtmSession* session = nullptr;
        std::string paneId;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            _started = true;
            session = _session;
            paneId = _paneId;
            rows = _rows;
            cols = _cols;
        }
        StateChanged.raise(*this, nullptr);
        if (session)
        {
            session->RegisterFollower(this);
            if (!paneId.empty())
            {
                session->WriteToLeader("resize-pane -t " + paneId + " -x " + std::to_string(cols) + " -y " + std::to_string(rows));
            }
        }
    }

    void HtmFollowerConnection::WriteInput(const winrt::array_view<const char16_t> data)
    {
        if (!_session)
        {
            return;
        }
        const auto utf8 = til::u16u8(winrt_array_to_wstring_view(data));
        _session->SendKeys(_paneId, utf8);
    }

    void HtmFollowerConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _rows = rows;
        _cols = columns;
        if (_session && !_paneId.empty())
        {
            _session->WriteToLeader("resize-pane -t " + _paneId + " -x " + std::to_string(columns) + " -y " + std::to_string(rows));
        }
    }

    void HtmFollowerConnection::SetPaneId(std::string paneId)
    {
        HtmSession* session = nullptr;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            _paneId = std::move(paneId);
            if (_started && !_paneId.empty())
            {
                session = _session;
                paneId = _paneId;
                rows = _rows;
                cols = _cols;
            }
        }
        if (session)
        {
            session->WriteToLeader("resize-pane -t " + paneId + " -x " + std::to_string(cols) + " -y " + std::to_string(rows));
        }
    }

    void HtmFollowerConnection::Close()
    {
        if (_session)
        {
            if (!_suppressClosePacket)
            {
                _session->WriteToLeader("kill-pane -t " + _paneId);
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
