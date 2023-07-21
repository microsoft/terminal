// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ConptyConnection.g.h"
#include "ConnectionStateHolder.h"

#include "ITerminalHandoff.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConptyConnection : ConptyConnectionT<ConptyConnection>, ConnectionStateHolder<ConptyConnection>
    {
        ConptyConnection(const HANDLE hSig,
                         const HANDLE hIn,
                         const HANDLE hOut,
                         const HANDLE hRef,
                         const HANDLE hServerProcess,
                         const HANDLE hClientProcess,
                         TERMINAL_STARTUP_INFO startupInfo);

        ConptyConnection() noexcept = default;
        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);

        static winrt::fire_and_forget final_release(std::unique_ptr<ConptyConnection> connection);

        void Start();
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close() noexcept;
        void ClearBuffer();

        void ShowHide(const bool show);

        void ReparentWindow(const uint64_t newParent);

        winrt::guid Guid() const noexcept;
        winrt::hstring Commandline() const;
        winrt::hstring StartingTitle() const;
        WORD ShowWindow() const noexcept;

        static void StartInboundListener();
        static void StopInboundListener();

        static winrt::event_token NewConnection(const NewConnectionHandler& handler);
        static void NewConnection(const winrt::event_token& token);

        static Windows::Foundation::Collections::ValueSet CreateSettings(const winrt::hstring& cmdline,
                                                                         const winrt::hstring& startingDirectory,
                                                                         const winrt::hstring& startingTitle,
                                                                         const Windows::Foundation::Collections::IMapView<hstring, hstring>& environment,
                                                                         uint32_t rows,
                                                                         uint32_t columns,
                                                                         const winrt::guid& guid,
                                                                         const winrt::guid& profileGuid);

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);

    private:
        static void closePseudoConsoleAsync(HPCON hPC) noexcept;
        static HRESULT NewHandoff(HANDLE in, HANDLE out, HANDLE signal, HANDLE ref, HANDLE server, HANDLE client, TERMINAL_STARTUP_INFO startupInfo) noexcept;
        static winrt::hstring _commandlineFromProcess(HANDLE process);

        HRESULT _LaunchAttachedClient() noexcept;
        void _indicateExitWithStatus(unsigned int status) noexcept;
        void _LastConPtyClientDisconnected() noexcept;

        til::CoordType _rows{};
        til::CoordType _cols{};
        uint64_t _initialParentHwnd{ 0 };
        hstring _commandline{};
        hstring _startingDirectory{};
        hstring _startingTitle{};
        bool _initialVisibility{ true };
        Windows::Foundation::Collections::ValueSet _environment{ nullptr };
        guid _guid{}; // A unique session identifier for connected client
        hstring _clientName{}; // The name of the process hosted by this ConPTY connection (as of launch).

        bool _receivedFirstByte{ false };
        std::chrono::high_resolution_clock::time_point _startTime{};

        wil::unique_hfile _inPipe; // The pipe for writing input to
        wil::unique_hfile _outPipe; // The pipe for reading output from
        wil::unique_handle _hOutputThread;
        wil::unique_process_information _piClient;
        wil::unique_any<HPCON, decltype(closePseudoConsoleAsync), closePseudoConsoleAsync> _hPC;

        til::u8state _u8State{};
        std::wstring _u16Str{};
        std::array<char, 4096> _buffer{};
        bool _passthroughMode{};
        bool _inheritCursor{ false };
        bool _reloadEnvironmentVariables{};
        guid _profileGuid{};

        struct StartupInfoFromDefTerm
        {
            winrt::hstring title{};
            winrt::hstring iconPath{};
            int32_t iconIndex{};
            WORD showWindow{};

        } _startupInfo{};

        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(ConptyConnection);
}
