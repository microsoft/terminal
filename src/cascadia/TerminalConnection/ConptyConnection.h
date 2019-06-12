// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConptyConnection.g.h"
// Note that the ConptyConnection is no longer a part of this project
// Until there's platform-level support for full-trust universal applications,
// all ProcThreadAttribute things will be unusable. Unfortunately, this means
// we'll be unable to use the conpty API directly.
namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection>
    {
        ConptyConnection() = delete;
        ConptyConnection(hstring const& commandline, uint32_t initialRows, uint32_t initialCols);

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

        uint32_t _initialRows;
        uint32_t _initialCols;
        hstring _commandline;

        bool _connected;
        HANDLE _inPipe; // The pipe for writing input to
        HANDLE _outPipe; // The pipe for reading output from
        HPCON _hPC;
        DWORD _outputThreadId;
        HANDLE _hOutputThread;
        PROCESS_INFORMATION _piClient;

        static DWORD WINAPI StaticOutputThreadProc(LPVOID lpParameter);
        void _CreatePseudoConsole();
        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection, implementation::ConptyConnection>
    {
    };
}
