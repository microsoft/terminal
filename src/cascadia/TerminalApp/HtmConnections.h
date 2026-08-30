// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "HtmProtocol.h"

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <til/event.h>

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
        void SetPaneId(std::string paneId) { _paneId = std::move(paneId); }
        const std::string& PaneId() const noexcept { return _paneId; }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        void _OutputHandler(const winrt::array_view<const char16_t> str);
        void _ProcessHtmBytes(std::string_view utf8);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _wrapped{ nullptr };
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        HtmSession* _session{ nullptr };
        bool _htmMode{ false };
        std::string _pendingInit;
        std::string _htmBuffer;
        std::string _paneId;
    };

    class HtmFollowerConnection : public winrt::implements<HtmFollowerConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        HtmFollowerConnection(HtmSession* session, std::string paneId);

        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/){};
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept { return {}; }
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept
        {
            return winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Connected;
        }

        const std::string& PaneId() const noexcept { return _paneId; }
        void InjectOutput(std::string_view utf8);
        void SetSuppressClosePacket(bool value) noexcept { _suppressClosePacket = value; }

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;
        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        HtmSession* _session{ nullptr };
        std::string _paneId;
        bool _started{ false };
        bool _suppressClosePacket{ false };
        uint32_t _rows{ 24 };
        uint32_t _cols{ 80 };
    };
}
