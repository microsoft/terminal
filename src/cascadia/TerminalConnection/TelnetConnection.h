// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TelnetConnection.g.h"

#include <mutex>
#include <condition_variable>

#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4251)
#include <telnetpp/core.hpp>
#include <telnetpp/session.hpp>
#include <telnetpp/options/naws/server.hpp>
#pragma warning(pop)

#include "winrt/Windows.Networking.Sockets.h"
#include "winrt/Windows.Storage.Streams.h"

#include "../cascadia/inc/cppwinrt_utils.h"
#include "ConnectionStateHolder.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct TelnetConnection : TelnetConnectionT<TelnetConnection>, ConnectionStateHolder<TelnetConnection>
    {
        static winrt::guid ConnectionType();

        TelnetConnection(const hstring& uri);

        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);

    private:
        hstring _uri;

        void _applicationReceive(telnetpp::bytes data,
                                 std::function<void(telnetpp::bytes)> const& send);

        void _socketBufferedSend(telnetpp::bytes data);
        fire_and_forget _socketFlushBuffer();
        void _socketSend(telnetpp::bytes data);
        size_t _socketReceive(gsl::span<telnetpp::byte> buffer);

        telnetpp::session _session;
        // NAWS = Negotiation About Window Size
        telnetpp::options::naws::server _nawsServer;
        Windows::Networking::Sockets::StreamSocket _socket;
        Windows::Storage::Streams::DataWriter _writer;
        Windows::Storage::Streams::DataReader _reader;

        std::optional<std::pair<uint32_t, uint32_t>> _prevResize;

        static constexpr size_t _receiveBufferSize = 1024;
        std::array<telnetpp::byte, _receiveBufferSize> _receiveBuffer;

        wil::unique_handle _hOutputThread;

        DWORD _outputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct TelnetConnection : TelnetConnectionT<TelnetConnection, implementation::TelnetConnection>
    {
    };
}
