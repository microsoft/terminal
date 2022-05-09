// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PipeConnection.g.h"
#include "ConnectionStateHolder.h"

#include <conpty-static.h>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct PipeConnection : PipeConnectionT<PipeConnection>, ConnectionStateHolder<PipeConnection>
    {
        static winrt::guid ConnectionType() noexcept;
        PipeConnection() noexcept = default;
        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);

        static winrt::fire_and_forget final_release(std::unique_ptr<PipeConnection> connection);

        void Start();
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close() noexcept;
        void ClearBuffer();

        void ShowHide(const bool show);

        void ReparentWindow(const uint64_t newParent);

        winrt::guid Guid() const noexcept;
        winrt::hstring Commandline() const;

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);

    private:
        HRESULT _LaunchAttachedClient() noexcept;
        void _indicateExitWithStatus(unsigned int status) noexcept;
        void _ClientTerminated() noexcept;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        uint64_t _initialParentHwnd{ 0 };
        hstring _commandline{};
        hstring _startingDirectory{};
        hstring _startingTitle{};
        bool _initialVisibility{ false };
        Windows::Foundation::Collections::ValueSet _environment{ nullptr };
        guid _guid{}; // A unique session identifier for connected client
        hstring _clientName{}; // The name of the process hosted by this ConPTY connection (as of launch).

        bool _receivedFirstByte{ false };
        std::chrono::high_resolution_clock::time_point _startTime{};

        wil::unique_hfile _inPipe; // The pipe for writing input to
        wil::unique_hfile _outPipe; // The pipe for reading output from
        wil::unique_handle _hOutputThread;
        wil::unique_process_information _piClient;
        // wil::unique_static_pseudoconsole_handle _hPC;
        wil::unique_threadpool_wait _clientExitWait;

        til::u8state _u8State{};
        std::wstring _u16Str{};
        std::array<char, 4096> _buffer{};
        bool _passthroughMode{};

        DWORD _OutputThread();
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(PipeConnection);
}
