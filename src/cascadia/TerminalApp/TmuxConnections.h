// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TmuxProtocol.h"

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <til/winrt.h>
#include <til/u8u16convert.h>

#include <mutex>

namespace winrt::TerminalApp::implementation
{
    class TmuxSession;

    struct __declspec(uuid("6B5B3E45-97F1-4F18-97F3-7EE92B1C2381"))
    ITmuxLeaderMarker : ::IUnknown
    {
    };

    struct __declspec(uuid("5A4A2B34-8C12-4E09-B812-4DD12A34B456"))
    ITmuxFollowerMarker : ::IUnknown
    {
    };

    class TmuxLeaderConnection : public winrt::implements<TmuxLeaderConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, ITmuxLeaderMarker>
    {
    public:
        TmuxLeaderConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection wrapped,
                            TmuxSession* session);

        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept;
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;

        void WriteRaw(std::string_view bytes);
        void InjectOutput(std::string_view utf8);
        void ForceCloseClient();
        bool InTmuxMode() const noexcept { return _tmuxMode; }
        TmuxSession* Session() const noexcept { return _session; }
        void SetPaneId(std::string paneId)
        {
            std::lock_guard lock{ _stateMutex };
            _paneId = std::move(paneId);
        }
        std::string PaneId() const noexcept
        {
            std::lock_guard lock{ _stateMutex };
            return _paneId;
        }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        void _OutputHandler(const winrt::array_view<const char16_t> str);
        void _ProcessTmuxBytes(std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _wrapped{ nullptr };
        winrt::guid _sessionId{};
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        TmuxSession* _session{ nullptr };
        bool _tmuxMode{ false };
        std::string _pendingInit;
        std::string _carrierPending;
        std::string _tmuxBuffer;
        mutable std::recursive_mutex _stateMutex;
        std::string _paneId;
        std::mutex _writeMutex;
        bool _closed{ false };
        // SendInput KEYEVENTF_UNICODE delivers one UTF-16 code unit per call;
        // hold high surrogates across WriteInput so emoji becomes real UTF-8.
        til::u16state _u16ToUtf8;
        ::Microsoft::Terminal::Tmux::Win32InputDecodeState _win32Decode;
        uint32_t _rows{ 24 };
        uint32_t _cols{ 80 };
        uint32_t _flushedRows{ 0 };
        uint32_t _flushedCols{ 0 };
        uint32_t _resizeGeneration{ 0 };
        void _flushPendingClientSize();
    };

    class TmuxFollowerConnection : public winrt::implements<TmuxFollowerConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, ITmuxFollowerMarker>
    {
    public:
        TmuxFollowerConnection(TmuxSession* session, std::string paneId);

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) {};
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept { return {}; }
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept
        {
            return _closed ? winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Closed :
                             winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected;
        }

        TmuxSession* Session() const noexcept { return _session; }
        std::string PaneId() const noexcept
        {
            std::lock_guard lock{ _stateMutex };
            return _paneId;
        }
        bool IsClosed() const noexcept
        {
            std::lock_guard lock{ _stateMutex };
            return _closed;
        }
        void SetPaneId(std::string paneId);
        void InjectOutput(std::string_view utf8);
        void SetSuppressClosePacket(bool value) noexcept
        {
            std::lock_guard lock{ _stateMutex };
            _suppressClosePacket = value;
        }
        // Stop accepting output/input without raising StateChanged; the page
        // still owns the TermControl and will close it via _TmuxClosePane.
        void SilenceForDetach() noexcept
        {
            std::lock_guard lock{ _stateMutex };
            _suppressClosePacket = true;
            _session = nullptr;
            _closed = true;
            _pendingOutput.clear();
        }
        void ForceCloseUi();
        void _flushPendingResize();

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        TmuxSession* _session{ nullptr };
        mutable std::recursive_mutex _stateMutex;
        std::string _paneId;
        bool _started{ false };
        bool _suppressClosePacket{ false };
        bool _closed{ false };
        std::string _pendingOutput;
        // SendInput KEYEVENTF_UNICODE delivers one UTF-16 code unit per call;
        // hold high surrogates across WriteInput so emoji becomes real UTF-8.
        til::u16state _u16ToUtf8;
        ::Microsoft::Terminal::Tmux::Win32InputDecodeState _win32Decode;
        uint32_t _rows{ 24 };
        uint32_t _cols{ 80 };
        // Last size pushed to htmd. Split layout animates through many
        // intermediate sizes; each ConPTY resize injects blank lines.
        uint32_t _flushedRows{ 0 };
        uint32_t _flushedCols{ 0 };
        uint32_t _resizeGeneration{ 0 };
    };

    inline TmuxLeaderConnection* AsTmuxLeader(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& conn) noexcept
    {
        if (conn)
        {
            if (const auto marker = conn.try_as<ITmuxLeaderMarker>())
            {
                return winrt::get_self<TmuxLeaderConnection>(marker);
            }
        }
        return nullptr;
    }

    inline TmuxFollowerConnection* AsTmuxFollower(const winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& conn) noexcept
    {
        if (conn)
        {
            if (const auto marker = conn.try_as<ITmuxFollowerMarker>())
            {
                return winrt::get_self<TmuxFollowerConnection>(marker);
            }
        }
        return nullptr;
    }
}
