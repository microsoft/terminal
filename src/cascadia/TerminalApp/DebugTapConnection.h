// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.Connection.h>
#include <til/latch.h>

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection;
    class DebugTapConnection : public winrt::implements<DebugTapConnection, winrt::Microsoft::Terminal::Connection::ITerminalConnection>
    {
    public:
        explicit DebugTapConnection(Microsoft::Terminal::Connection::ITerminalConnection wrappedConnection);
        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/){};
        ~DebugTapConnection();
        void Start();
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
        winrt::Microsoft::Terminal::Connection::ConnectionState State() const noexcept;

        void SetInputTap(const Microsoft::Terminal::Connection::ITerminalConnection& inputTap);

        WINRT_CALLBACK(TerminalOutput, winrt::Microsoft::Terminal::Connection::TerminalOutputHandler);

        TYPED_EVENT(StateChanged, winrt::Microsoft::Terminal::Connection::ITerminalConnection, winrt::Windows::Foundation::IInspectable);

    private:
        void _PrintInput(const hstring& data);
        void _OutputHandler(const hstring str);

        winrt::Microsoft::Terminal::Connection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::Connection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        winrt::weak_ref<Microsoft::Terminal::Connection::ITerminalConnection> _wrappedConnection;
        winrt::weak_ref<Microsoft::Terminal::Connection::ITerminalConnection> _inputSide;

        til::latch _start{ 1 };

        friend class DebugInputTapConnection;
    };
}

std::tuple<winrt::Microsoft::Terminal::Connection::ITerminalConnection, winrt::Microsoft::Terminal::Connection::ITerminalConnection> OpenDebugTapConnection(winrt::Microsoft::Terminal::Connection::ITerminalConnection baseConnection);
