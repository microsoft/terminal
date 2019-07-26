// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConhostConnection.g.h"

// FIXME: idk how to include this form cppwinrt_utils.h
#define DECLARE_EVENT(name, eventHandler, args)          \
public:                                                  \
    winrt::event_token name(args const& handler);        \
    void name(winrt::event_token const& token) noexcept; \
                                                         \
private:                                                 \
    winrt::event<args> eventHandler;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConhostConnection : ConhostConnectionT<ConhostConnection>
    {
        ConhostConnection(const hstring& cmdline, const hstring& startingDirectory, const uint32_t rows, const uint32_t cols, const guid& guid);

        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        hstring GetConnectionFailatureMessage();
        hstring GetConnectionFailatureTabTitle();
        hstring GetDisconnectionMessage();
        hstring GetDisconnectionTabTitle(hstring previousTitle);

        winrt::guid Guid() const noexcept;

        DECLARE_EVENT(TerminalOutput, _outputHandlers, TerminalConnection::TerminalOutputEventArgs);
        DECLARE_EVENT(TerminalConnected, _connectHandlers, TerminalConnection::TerminalConnectedEventArgs);
        DECLARE_EVENT(TerminalDisconnected, _disconnectHandlers, TerminalConnection::TerminalDisconnectedEventArgs);

    private:
        uint32_t _initialRows{};
        uint32_t _initialCols{};
        hstring _commandline;
        hstring _startingDirectory;
        guid _guid{}; // A unique session identifier for connected client

        bool _connected{};
        std::atomic<bool> _closing{ false };

        // These fields are about process created by passed commandline
        DWORD _processStartupErrorCode{ 0 };
        DWORD _processExitCode{ 0 };
        wil::unique_handle _processHandle;

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
