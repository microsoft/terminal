// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TelnetConnection.g.h"

#include <mutex>
#include <condition_variable>

#pragma warning(push)
#pragma warning(disable:4100)
#pragma warning(disable:4251)
#include <telnetpp/core.hpp>
#include <telnetpp/session.hpp>
#include <telnetpp/options/echo/server.hpp>
#pragma warning(pop)

#include "winrt/Windows.Networking.Sockets.h"
#include "winrt/Windows.Storage.Streams.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct TelnetConnection : TelnetConnectionT<TelnetConnection>
    {
        TelnetConnection();

        winrt::event_token TerminalOutput(TerminalConnection::TerminalOutputEventArgs const& handler);
        void TerminalOutput(winrt::event_token const& token) noexcept;
        winrt::event_token TerminalDisconnected(TerminalConnection::TerminalDisconnectedEventArgs const& handler);
        void TerminalDisconnected(winrt::event_token const& token) noexcept;
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

    private:
        winrt::event<TerminalConnection::TerminalOutputEventArgs> _outputHandlers;
        winrt::event<TerminalConnection::TerminalDisconnectedEventArgs> _disconnectHandlers;

        void _applicationReceive(telnetpp::bytes data,
                                 std::function<void(telnetpp::bytes)> const& send);

        winrt::fire_and_forget _socketSend(telnetpp::bytes data);
        int _socketReceive(telnetpp::byte* buffer, int size);

        telnetpp::session _session;
        telnetpp::options::echo::server _echoServer;
        Windows::Networking::Sockets::StreamSocket _socket;
        Windows::Storage::Streams::DataWriter _writer;
        Windows::Storage::Streams::DataReader _reader;

        telnetpp::byte _receiveBuffer[1024];


        std::condition_variable _canProceed;
        std::mutex _commonMutex;

        enum class State
        {
            AccessStored,
            DeviceFlow,
            TenantChoice,
            StoreTokens,
            TermConnecting,
            TermConnected,
            NoConnect
        };
        
        State _state{ State::TermConnecting };

        std::optional<bool> _store;
        std::optional<bool> _removeOrNew;

        bool _connected{};
        std::atomic<bool> _closing{ false };

        wil::unique_handle _hOutputThread;

        static DWORD WINAPI StaticOutputThreadProc(LPVOID lpParameter);
        DWORD _OutputThread();

        void _WriteStringWithNewline(const winrt::hstring& str);
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct TelnetConnection : TelnetConnectionT<TelnetConnection, implementation::TelnetConnection>
    {
    };
}
