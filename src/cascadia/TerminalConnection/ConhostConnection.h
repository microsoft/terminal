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
        hstring _commandline{};
        hstring _startingDirectory{};

        bool _connected{};
        HANDLE _inPipe{ INVALID_HANDLE_VALUE }; // The pipe for writing input to
        HANDLE _outPipe{ INVALID_HANDLE_VALUE }; // The pipe for reading output from
        HANDLE _signalPipe{ INVALID_HANDLE_VALUE };
        DWORD _outputThreadId{};
        HANDLE _hOutputThread{ nullptr };
        PROCESS_INFORMATION _piConhost{};
        guid _guid{}; // A "unique" session identifier for connected client
        bool _closing{};

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
