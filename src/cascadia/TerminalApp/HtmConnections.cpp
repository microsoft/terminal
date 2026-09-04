// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "HtmConnections.h"
#include "HtmSession.h"

#include <winrt/Windows.System.Threading.h>

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
            // Stateful conversion: KEYEVENTF_UNICODE may deliver one surrogate
            // per WriteInput; til::u16u8 without state would emit CESU-8.
            const auto utf8 = til::u16u8(winrt_array_to_wstring_view(data), _u16ToUtf8);
            if (utf8.empty() || utf8 == "\x1b[I" || utf8 == "\x1b[O")
            {
                return;
            }
            if (!_session)
            {
                return;
            }
            const auto keys = DecodeWin32InputMode(utf8, _win32Decode);
            if (keys.empty())
            {
                return;
            }
            _session->HandleLeaderInput(keys);
            return;
        }
        _wrapped.WriteInput(data);
    }

    void HtmLeaderConnection::Resize(uint32_t rows, uint32_t columns)
    {
        _wrapped.Resize(rows, columns);
        if (!_htmMode || !_session || rows == 0 || columns == 0)
        {
            return;
        }
        uint32_t generation = 0;
        {
            std::lock_guard lock{ _stateMutex };
            if (_rows == rows && _cols == columns)
            {
                return;
            }
            _rows = rows;
            _cols = columns;
            generation = ++_resizeGeneration;
        }
        const auto weak = get_weak();
        winrt::Windows::System::Threading::ThreadPoolTimer::CreateTimer(
            [weak, generation](const auto&) {
                if (const auto self = weak.get())
                {
                    uint32_t current = 0;
                    {
                        std::lock_guard lock{ self->_stateMutex };
                        current = self->_resizeGeneration;
                    }
                    if (current == generation)
                    {
                        self->_flushPendingClientSize();
                    }
                }
            },
            std::chrono::milliseconds{ 75 });
    }

    void HtmLeaderConnection::_flushPendingClientSize()
    {
        HtmSession* session = nullptr;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            if (_closed || !_htmMode || !_session || _rows == 0 || _cols == 0)
            {
                return;
            }
            if (_rows == _flushedRows && _cols == _flushedCols)
            {
                return;
            }
            _flushedRows = _rows;
            _flushedCols = _cols;
            session = _session;
            rows = _rows;
            cols = _cols;
        }
        session->WriteToLeader("refresh-client -C " + std::to_string(cols) + "x" + std::to_string(rows));
    }

    void HtmLeaderConnection::Close()
    {
        {
            std::lock_guard lock{ _stateMutex };
            ++_resizeGeneration;
        }
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
        try
        {
            std::lock_guard lock{ _writeMutex };
            if (_closed || !_wrapped)
            {
                return;
            }
            const auto wide = til::u8u16(bytes);
            _wrapped.WriteInput(winrt_wstring_to_array_view(wide));
        }
        catch (...)
        {
            // ConPTY may already be gone during htmd teardown; never abort.
        }
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

    void HtmLeaderConnection::ForceCloseClient()
    {
        _htmMode = false;
        _session = nullptr;
        _outputRevoker.revoke();
        _stateChangedRevoker.revoke();
        if (_wrapped)
        {
            _wrapped.Close();
            _wrapped = nullptr;
        }
        _closed = true;
        StateChanged.raise(*this, nullptr);
    }

    void HtmLeaderConnection::_OutputHandler(const winrt::array_view<const char16_t> str)
    {
        const auto utf8 = til::u16u8(winrt_array_to_wstring_view(str));
        const auto carrier = DecodeConPtyHtmCarrier(_carrierPending, utf8);
        _carrierPending = carrier.pending;
        if (carrier.decoded.empty())
        {
            return;
        }
        if (_htmMode)
        {
            if (carrier.decoded.find(TmuxControlSt) != std::string::npos)
            {
                _htmMode = false;
                if (_session)
                {
                    _session->HandleExitSequence();
                }
                return;
            }
            _ProcessHtmBytes(carrier.decoded);
            return;
        }

        _pendingInit.append(carrier.decoded);
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
        if (_session)
            _session->AttachLeader(this);
        if (!remainder.empty())
            _ProcessHtmBytes(remainder);
    }

    void HtmLeaderConnection::_ProcessHtmBytes(std::string_view utf8)
    {
        _htmBuffer.append(utf8);
        size_t newline = 0;
        while ((newline = _htmBuffer.find('\n')) != std::string::npos)
        {
            auto line = _htmBuffer.substr(0, newline);
            _htmBuffer.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (_session)
                _session->HandleLine(line);
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
        std::wstring pendingWide;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            _started = true;
            session = _session;
            paneId = _paneId;
            rows = _rows;
            cols = _cols;
            if (!_pendingOutput.empty())
            {
                pendingWide = til::u8u16(_pendingOutput);
                _pendingOutput.clear();
            }
        }
        StateChanged.raise(*this, nullptr);
        if (session)
        {
            session->RegisterFollower(this);
            if (!paneId.empty() && rows > 0 && cols > 0)
            {
                {
                    std::lock_guard lock{ _stateMutex };
                    _flushedRows = rows;
                    _flushedCols = cols;
                }
                session->WriteToLeader("resize-pane -t " + paneId + " -x " + std::to_string(cols) + " -y " + std::to_string(rows));
            }
        }
        if (!pendingWide.empty())
        {
            TerminalOutput.raise(winrt_wstring_to_array_view(pendingWide));
        }
    }

    void HtmFollowerConnection::WriteInput(const winrt::array_view<const char16_t> data)
    {
        if (!_session || _closed)
        {
            return;
        }
        // Stateful conversion: KEYEVENTF_UNICODE may deliver one surrogate
        // per WriteInput; til::u16u8 without state would emit CESU-8.
        const auto utf8 = til::u16u8(winrt_array_to_wstring_view(data), _u16ToUtf8);
        if (utf8.empty() || utf8 == "\x1b[I" || utf8 == "\x1b[O")
        {
            return;
        }
        const auto keys = DecodeWin32InputMode(utf8, _win32Decode);
        if (keys.empty())
        {
            return;
        }
        _session->SendKeys(_paneId, keys);
    }

    void HtmFollowerConnection::Resize(uint32_t rows, uint32_t columns)
    {
        // TermControl may report 0x0 during first layout; never push that to htmd.
        if (rows == 0 || columns == 0)
        {
            return;
        }
        uint32_t generation = 0;
        {
            std::lock_guard lock{ _stateMutex };
            if (_rows == rows && _cols == columns)
            {
                return;
            }
            _rows = rows;
            _cols = columns;
            // Split layout settles through dozens of intermediate sizes. Each
            // ConPTY resize tends to inject blank lines into the pane scrollback.
            generation = ++_resizeGeneration;
        }
        const auto weak = get_weak();
        winrt::Windows::System::Threading::ThreadPoolTimer::CreateTimer(
            [weak, generation](const auto&) {
                if (const auto self = weak.get())
                {
                    uint32_t current = 0;
                    {
                        std::lock_guard lock{ self->_stateMutex };
                        current = self->_resizeGeneration;
                    }
                    if (current == generation)
                    {
                        self->_flushPendingResize();
                    }
                }
            },
            // ~75ms trailing debounce covers WT split layout animation.
            std::chrono::milliseconds{ 75 });
    }

    void HtmFollowerConnection::_flushPendingResize()
    {
        HtmSession* session = nullptr;
        std::string paneId;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            if (_closed || !_session || _paneId.empty() || _rows == 0 || _cols == 0)
            {
                return;
            }
            if (_rows == _flushedRows && _cols == _flushedCols)
            {
                return;
            }
            _flushedRows = _rows;
            _flushedCols = _cols;
            session = _session;
            paneId = _paneId;
            rows = _rows;
            cols = _cols;
        }
        session->WriteToLeader("resize-pane -t " + paneId + " -x " + std::to_string(cols) + " -y " + std::to_string(rows));
    }

    void HtmFollowerConnection::SetPaneId(std::string paneId)
    {
        HtmSession* session = nullptr;
        uint32_t rows = 0;
        uint32_t cols = 0;
        {
            std::lock_guard lock{ _stateMutex };
            _paneId = std::move(paneId);
            if (_started && !_paneId.empty() && _rows > 0 && _cols > 0)
            {
                session = _session;
                paneId = _paneId;
                rows = _rows;
                cols = _cols;
                _flushedRows = rows;
                _flushedCols = cols;
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
        _closed = true;
        StateChanged.raise(*this, nullptr);
    }

    void HtmFollowerConnection::ForceCloseUi()
    {
        {
            std::lock_guard lock{ _stateMutex };
            _suppressClosePacket = true;
            _session = nullptr;
            _closed = true;
            _pendingOutput.clear();
            // Cancel trailing resize debounce timers.
            ++_resizeGeneration;
        }
        try
        {
            StateChanged.raise(*this, nullptr);
        }
        catch (...)
        {
        }
    }

    void HtmFollowerConnection::InjectOutput(std::string_view utf8)
    {
        if (utf8.empty())
        {
            return;
        }
        try
        {
            std::wstring wide;
            {
                std::lock_guard lock{ _stateMutex };
                if (_closed)
                {
                    return;
                }
                if (!_started)
                {
                    _pendingOutput.append(utf8);
                    return;
                }
                wide = til::u8u16(utf8);
                if (_closed)
                {
                    return;
                }
            }
            if (_closed)
            {
                return;
            }
            TerminalOutput.raise(winrt_wstring_to_array_view(wide));
        }
        catch (...)
        {
            // TermControl may already be tearing down during detach.
        }
    }
}
