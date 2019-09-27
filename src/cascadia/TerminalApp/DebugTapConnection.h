#pragma once

#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::TerminalApp::implementation
{
    class DebugInputTapConnection;
    class DebugTapConnection : public winrt::implements<DebugTapConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection>
    {
    public:
        DebugTapConnection(Microsoft::Terminal::TerminalConnection::ITerminalConnection wrappedConnection);
        ~DebugTapConnection();
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        DECLARE_EVENT(TerminalOutput, _terminalOutput, Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs);
        DECLARE_EVENT(TerminalDisconnected, _terminalDisconnected, Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs);

    private:
        void PrintInput(const hstring& data);

        void _OutputHandler(const hstring str);
        void _DisconnectedHandler();
        uint32_t _columns{ 0 };
        size_t _off{ 0 };
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalOutput_revoker _outputRevoker;
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection::TerminalDisconnected_revoker _disconnectedRevoker;
        winrt::weak_ref<Microsoft::Terminal::TerminalConnection::ITerminalConnection> _wrappedConnection;

        friend class DebugInputTapConnection;
    };
}

std::tuple<winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection> OpenDebugTapConnection(winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection baseConnection);
