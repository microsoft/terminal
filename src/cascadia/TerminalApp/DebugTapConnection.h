// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <til/latch.h>

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection;
    class DebugTapConnection : public winrt::implements<DebugTapConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        explicit DebugTapConnection(Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection);
        void Initialize(const Windows::Foundation::Collections::ValueSet& /*settings*/){};
        ~DebugTapConnection();
        void Start();
        void WriteInput(const winrt::array_view<const char16_t> data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        winrt::guid SessionId() const noexcept;
        winrt::Microsoft::Terminal::TerminalConnection::ConnectionState State() const noexcept;

        void SetInputTap(const Microsoft::Terminal::TerminalConnection::ITerminalConnection& inputTap);

        til::event<winrt::Microsoft::Terminal::TerminalConnection::TerminalOutputHandler> TerminalOutput;

        til::typed_event<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Windows::Foundation::IInspectable> StateChanged;

    private:
        void _PrintInput(const std::wstring_view data);
        void _OutputHandler(const std::wstring_view str);

        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::StateChanged_revoker _stateChangedRevoker;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _wrappedConnection;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _inputSide;

        til::latch _start{ 1 };

        friend class DebugInputTapConnection;
    };
}

std::tuple<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> OpenDebugTapConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection baseConnection);
