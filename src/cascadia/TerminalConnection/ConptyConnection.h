// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConptyConnection.g.h"
#include "ConnectionStateHolder.h"
#include "../inc/cppwinrt_utils.h"

#include <conpty-static.h>

namespace wil
{
    // These belong in WIL upstream, so when we reingest the change that has them we'll get rid of ours.
    using unique_static_pseudoconsole_handle = wil::unique_any<HPCON, decltype(&::ConptyClosePseudoConsole), ::ConptyClosePseudoConsole>;
}

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection>, ConnectionStateHolder<ConptyConnection>
    {
        ConptyConnection(const HANDLE hSig,
                         const HANDLE hIn,
                         const HANDLE hOut,
                         const HANDLE hClientProcess);

        ConptyConnection(
            const hstring& cmdline,
            const hstring& startingDirectory,
            const hstring& startingTitle,
            const Windows::Foundation::Collections::IMapView<hstring, hstring>& environment,
            const uint32_t rows,
            const uint32_t cols,
            const guid& guid);
        static winrt::fire_and_forget final_release(std::unique_ptr<ConptyConnection> connection);

        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close() noexcept;

        winrt::guid Guid() const noexcept;

        static void StartInboundListener();
        static void StopInboundListener();

        static winrt::event_token NewConnection(NewConnectionHandler const& handler);
        static void NewConnection(winrt::event_token const& token);

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);

    private:
        HRESULT _LaunchAttachedClient() noexcept;
        void _indicateExitWithStatus(unsigned int status) noexcept;
        void _ClientTerminated() noexcept;

        static HRESULT NewHandoff(HANDLE in, HANDLE out, HANDLE signal, HANDLE process) noexcept;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        hstring _commandline;
        hstring _startingDirectory;
        hstring _startingTitle;
        Windows::Foundation::Collections::IMapView<hstring, hstring> _environment;
        guid _guid{}; // A unique session identifier for connected client
        hstring _clientName{}; // The name of the process hosted by this ConPTY connection (as of launch).

        bool _receivedFirstByte{ false };
        std::chrono::high_resolution_clock::time_point _startTime{};

        wil::unique_hfile _inPipe; // The pipe for writing input to
        wil::unique_hfile _outPipe; // The pipe for reading output from
        wil::unique_handle _hOutputThread;
        wil::unique_process_information _piClient;
        wil::unique_static_pseudoconsole_handle _hPC;
        wil::unique_threadpool_wait _clientExitWait;

        til::u8state _u8State;
        std::wstring _u16Str;
        std::array<char, 4096> _buffer;

        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection, implementation::ConptyConnection>
    {
    };
}
