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

        bool AllowsUserInput();
        VisualConnectionState VisualConnectionState();
        hstring GetTabTitle(hstring terminalTitle);

        winrt::guid Guid() const noexcept;

        DECLARE_EVENT(TerminalOutput, _outputHandlers, TerminalConnection::TerminalOutputEventArgs);
        DECLARE_EVENT(TerminalDisconnected, _disconnectHandlers, TerminalConnection::TerminalDisconnectedEventArgs);
        DECLARE_EVENT(StateChanged, _stateChangedHandlers, TerminalConnection::StateChangedEventArgs);

    private:
        uint32_t _initialRows{};
        uint32_t _initialCols{};
        hstring _commandline;
        hstring _startingDirectory;
        guid _guid{}; // A unique session identifier for connected client

        // Protects fields accessed simultaneously by main thread and _OutputThread.
        // Note the connection is considered publicly not thread-safe.
        std::mutex _outputThreadMutex;

        bool _open{ false };
        std::atomic<bool> _closing{ false };
        bool _allowsUserInput{ false };
        TerminalConnection::VisualConnectionState _visualConnectionState{ VisualConnectionState::NotConnected };

        // These fields are about process created by passed commandline
        std::optional<DWORD> _processStartupErrorCode{};
        std::optional<DWORD> _processExitCode{};
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
