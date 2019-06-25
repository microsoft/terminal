// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConhostConnection.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConhostConnection : ConhostConnectionT<ConhostConnection>
    {
        ConhostConnection(const hstring& cmdline, const hstring& startingDirectory, const uint32_t rows, const uint32_t cols, const guid& guid);

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
        winrt::event<TerminalConnection::TerminalOutputEventArgs> _outputHandlers;
        winrt::event<TerminalConnection::TerminalDisconnectedEventArgs> _disconnectHandlers;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        hstring _commandline;
        hstring _startingDirectory;
        guid _guid{}; // A unique session identifier for connected client

        bool _connected{};
        std::atomic<bool> _closing{ false };

        wil::unique_hfile _inPipe; // The pipe for writing input to
        wil::unique_hfile _outPipe; // The pipe for reading output from
        wil::unique_hfile _signalPipe;
        wil::unique_handle _hOutputThread;
        wil::unique_process_information _piConhost;
        wil::unique_handle _hJob;

        static DWORD WINAPI StaticOutputThreadProc(LPVOID lpParameter);
        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct ConhostConnection : ConhostConnectionT<ConhostConnection, implementation::ConhostConnection>
    {
    };
}
