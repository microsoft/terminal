// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HtmProtocol.h"

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <til/winrt.h>

#include <mutex>

namespace winrt::TerminalApp::implementation
{
    class HtmSession;

    class HtmLeaderConnection : public winrt::implements<HtmLeaderConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        HtmLeaderConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection wrapped,
                            HtmSession* session);

        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept;
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;

        void WriteRaw(std::string_view bytes);
        void InjectOutput(std::string_view utf8);
        bool InHtmMode() const noexcept { return _htmMode; }
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
        void _ProcessHtmBytes(std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _wrapped{ nullptr };
        winrt::guid _sessionId{};
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        HtmSession* _session{ nullptr };
        bool _htmMode{ false };
        std::string _pendingInit;
        std::string _htmBuffer;
        mutable std::mutex _stateMutex;
        std::string _paneId;
        std::mutex _writeMutex;
        bool _closed{ false };
    };

    class HtmFollowerConnection : public winrt::implements<HtmFollowerConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        HtmFollowerConnection(HtmSession* session, std::string paneId);

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/) {};
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept { return {}; }
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept
        {
            return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected;
        }

        std::string PaneId() const noexcept
        {
            std::lock_guard lock{ _stateMutex };
            return _paneId;
        }
        void SetPaneId(std::string paneId);
        void InjectOutput(std::string_view utf8);
        void SetSuppressClosePacket(bool value) noexcept
        {
            std::lock_guard lock{ _stateMutex };
            _suppressClosePacket = value;
        }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        HtmSession* _session{ nullptr };
        mutable std::mutex _stateMutex;
        std::string _paneId;
        bool _started{ false };
        bool _suppressClosePacket{ false };
        uint32_t _rows{ 24 };
        uint32_t _cols{ 80 };
    };
}
