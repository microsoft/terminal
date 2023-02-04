// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <til/latch.h>

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection;
    class DebugTapConnection : public winrt::implements<DebugTapConnection, MTConnection::ITerminalConnection>
    {
    public:
        explicit DebugTapConnection(Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection);
        void Initialize(const WFC::ValueSet& /*settings*/){};
        ~DebugTapConnection();
        void Start();
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
        MTConnection::ConnectionState State() const noexcept;

        void SetInputTap(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& inputTap);

        WINRT_CALLBACK(TerminalOutput, MTConnection::TerminalOutputHandler);

        TYPED_EVENT(StateChanged, MTConnection::ITerminalConnection, WF::IInspectable);

    private:
        void _PrintInput(const hstring& data);
        void _OutputHandler(const hstring str);

        MTConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        MTConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _wrappedConnection;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _inputSide;

        til::latch _start{ 1 };

        friend class DebugInputTapConnection;
    };
}

std::tuple<MTConnection::ITerminalConnection, MTConnection::ITerminalConnection> OpenDebugTapConnection(MTConnection::ITerminalConnection baseConnection);
