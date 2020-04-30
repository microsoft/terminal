// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection;
    class DebugTapConnection : public winrt::implements<DebugTapConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        explicit DebugTapConnection(Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection);
        ~DebugTapConnection();
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;

        void SetInputTap(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& inputTap);

        WINRT_CALLBACK(TerminalOutput, winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler);

        TYPED_EVENT(StateChanged, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable);

    private:
        void _PrintInput(const hstring& data);
        void _OutputHandler(const hstring str);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _wrappedConnection;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _inputSide;

        friend class DebugInputTapConnection;
    };
}

std::tuple<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> OpenDebugTapConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection baseConnection);
