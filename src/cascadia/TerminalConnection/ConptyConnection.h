// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConptyConnection.g.h"
#include <conpty-static.h>

namespace wil
{
    // These belong in WIL upstream, so when we reingest the change that has them we'll get rid of ours.
    using unique_static_pseudoconsole_handle = wil::unique_any<HPCON, decltype(&::ConptyClosePseudoConsole), ::ConptyClosePseudoConsole>;
}

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection>
    {
        ConptyConnection(const hstring& cmdline, const hstring& startingDirectory, const hstring& startingTitle, const uint32_t rows, const uint32_t cols, const guid& guid);

        winrt::event_token TerminalOutput(TerminalConnection::TerminalOutputEventArgs const& handler);
        void TerminalOutput(winrt::event_token const& token) noexcept;
        winrt::event_token TerminalDisconnected(TerminalConnection::TerminalDisconnectedEventArgs const& handler);
        void TerminalDisconnected(winrt::event_token const& token) noexcept;
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid Guid() const noexcept;

    private:
        HRESULT _LaunchAttachedClient() noexcept;
        void _ClientTerminated() noexcept;

        winrt::event<TerminalConnection::TerminalOutputEventArgs> _outputHandlers;
        winrt::event<TerminalConnection::TerminalDisconnectedEventArgs> _disconnectHandlers;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        hstring _commandline;
        hstring _startingDirectory;
        hstring _startingTitle;
        guid _guid{}; // A unique session identifier for connected client

        bool _connected{};
        std::atomic<bool> _closing{ false };
        bool _recievedFirstByte{ false };
        std::chrono::high_resolution_clock::time_point _startTime{};

        wil::unique_hfile _inPipe; // The pipe for writing input to
        wil::unique_hfile _outPipe; // The pipe for reading output from
        wil::unique_handle _hOutputThread;
        wil::unique_process_information _piClient;
        wil::unique_static_pseudoconsole_handle _hPC;
        wil::unique_threadpool_wait _clientExitWait;

        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection, implementation::ConptyConnection>
    {
    };
}
